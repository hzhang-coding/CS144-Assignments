#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return outbound_stream().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    DUMMY_CODE(seg);
    _time = 0;

    if (!_alive) {
        return;
    }

    if (seg.header().rst) {
        bool send_rst = false;
        unclean_shutdown(send_rst);
        return;
    }

    if (_receiver.ackno().has_value() && seg.length_in_sequence_space() == 0 &&
        seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
        send_segments_out();
        return;
    }

    _receiver.segment_received(seg);

    if (seg.header().syn && !_syn) {
        connect();
        return;
    }

    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }

    if (seg.length_in_sequence_space() > 0 && _sender.segments_out().empty()) {
        _sender.send_empty_segment();
    }

    send_segments_out();
}

bool TCPConnection::active() const { return _alive; }

size_t TCPConnection::write(const string &data) {
    DUMMY_CODE(data);

    size_t n = outbound_stream().write(data);
    _sender.fill_window();
    send_segments_out();
    return n;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    DUMMY_CODE(ms_since_last_tick);

    _time += ms_since_last_tick;

    if (!_alive) {
        return;
    }

    _sender.tick(ms_since_last_tick);

    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        bool send_rst = true;
        unclean_shutdown(send_rst);
        return;
    }

    send_segments_out();
}

void TCPConnection::end_input_stream() {
    outbound_stream().end_input();
    _sender.fill_window();
    send_segments_out();
}

void TCPConnection::connect() {
    _syn = true;
    _sender.fill_window();
    send_segments_out();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            bool send_rst = true;
            unclean_shutdown(send_rst);
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::send_segments_out() {
    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = _receiver.window_size();
        }
        segments_out().push(seg);
        _sender.segments_out().pop();
    }

    if (_linger_after_streams_finish) {
        if (inbound_stream().input_ended()) {
            if (!outbound_stream().eof()) {
                _linger_after_streams_finish = false;
            } else if (_sender.bytes_in_flight() == 0 && time_since_last_segment_received() >= 10 * _cfg.rt_timeout) {
                _alive = false;
            }
        }
    } else if (outbound_stream().eof() && _sender.bytes_in_flight() == 0) {
        _alive = false;
    }
}

void TCPConnection::unclean_shutdown(bool send_rst) {
    inbound_stream().set_error();
    outbound_stream().set_error();
    _linger_after_streams_finish = false;
    _alive = false;

    if (send_rst) {
        TCPSegment seg;
        seg.header().rst = true;
        seg.header().seqno = _sender.next_seqno();
        _segments_out.push(seg);
    }
}
