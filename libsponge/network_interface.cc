#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address), _ip(_ip_address.ipv4_numeric()) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    DUMMY_CODE(dgram, next_hop, next_hop_ip);

    if (_ip_to_ethernet.count(next_hop_ip) && _waiting.empty()) {
        EthernetAddress next_hop_ethernet = _ip_to_ethernet[next_hop_ip].first;

        EthernetFrame s_frame;
        s_frame.header().dst = next_hop_ethernet;
        s_frame.header().src = _ethernet_address;
        s_frame.header().type = EthernetHeader::TYPE_IPv4;
        s_frame.payload() = move(dgram.serialize());

        _frames_out.emplace(s_frame);
    } else {
        if (_waiting.empty()) {
            ARPMessage s_msg;
            s_msg.opcode = ARPMessage::OPCODE_REQUEST;
            s_msg.sender_ethernet_address = _ethernet_address;
            s_msg.sender_ip_address = _ip;
            s_msg.target_ip_address = next_hop_ip;

            EthernetFrame s_frame;
            s_frame.header().dst = ETHERNET_BROADCAST;
            s_frame.header().src = _ethernet_address;
            s_frame.header().type = EthernetHeader::TYPE_ARP;
            s_frame.payload() = move(s_msg.serialize());

            _frames_out.emplace(s_frame);
        }

        _waiting.push({_time, {dgram, next_hop_ip}});
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    DUMMY_CODE(frame);

    if (frame.header().dst != _ethernet_address && frame.header().dst != ETHERNET_BROADCAST) {
        return nullopt;
    }
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram ret;
        if (ret.parse(frame.payload()) == ParseResult::NoError) {
            return ret;
        }
    } else if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage r_msg;
        if (r_msg.parse(frame.payload()) == ParseResult::NoError) {
            if (!_ip_to_ethernet.count(r_msg.sender_ip_address)) {
                _ip_to_ethernet[r_msg.sender_ip_address] = {r_msg.sender_ethernet_address, 1};
            } else {
                _ip_to_ethernet[r_msg.sender_ip_address].first = r_msg.sender_ethernet_address;
                ++_ip_to_ethernet[r_msg.sender_ip_address].second;
            }

            _to_expire.push({_time, r_msg.sender_ip_address});

            while (!_waiting.empty() && _ip_to_ethernet.count(_waiting.front().second.second)) {
                EthernetAddress next_hop_ethernet = _ip_to_ethernet[_waiting.front().second.second].first;

                EthernetFrame s_frame;
                s_frame.header().dst = next_hop_ethernet;
                s_frame.header().src = _ethernet_address;
                s_frame.header().type = EthernetHeader::TYPE_IPv4;
                s_frame.payload() = move(_waiting.front().second.first.serialize());

                _frames_out.emplace(s_frame);

                _waiting.pop();
            }

            if (r_msg.opcode == ARPMessage::OPCODE_REQUEST && r_msg.target_ip_address == _ip) {
                ARPMessage s_msg;
                s_msg.opcode = ARPMessage::OPCODE_REPLY;
                s_msg.sender_ethernet_address = _ethernet_address;
                s_msg.sender_ip_address = _ip;
                s_msg.target_ethernet_address = r_msg.sender_ethernet_address;
                s_msg.target_ip_address = r_msg.sender_ip_address;

                EthernetFrame s_frame;
                s_frame.header().dst = r_msg.sender_ethernet_address;
                s_frame.header().src = _ethernet_address;
                s_frame.header().type = EthernetHeader::TYPE_ARP;
                s_frame.payload() = move(s_msg.serialize());

                _frames_out.emplace(s_frame);
            }
        }
    }
    return nullopt;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    DUMMY_CODE(ms_since_last_tick);

    _time += ms_since_last_tick;

    while (!_to_expire.empty() && _time - _to_expire.front().first >= 30000) {
        // --_ip_to_ethernet[_to_expire.front().second].second;
        if (--_ip_to_ethernet[_to_expire.front().second].second == 0) {
            _ip_to_ethernet.erase(_to_expire.front().second);
        }
        _to_expire.pop();
    }

    if (!_waiting.empty() && _time - _waiting.front().first >= 5000) {
        _waiting.front().first = _time;

        ARPMessage s_msg;
        s_msg.opcode = ARPMessage::OPCODE_REQUEST;
        s_msg.sender_ethernet_address = _ethernet_address;
        s_msg.sender_ip_address = _ip;
        s_msg.target_ip_address = _waiting.front().second.second;

        EthernetFrame s_frame;
        s_frame.header().dst = ETHERNET_BROADCAST;
        s_frame.header().src = _ethernet_address;
        s_frame.header().type = EthernetHeader::TYPE_ARP;
        s_frame.payload() = move(s_msg.serialize());

        _frames_out.emplace(s_frame);
    }
}
