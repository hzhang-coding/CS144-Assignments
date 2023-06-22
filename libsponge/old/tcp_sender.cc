#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _segments_out()
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _next_seqno(0)
    , _alarm(false)
    , _time(0)
    , _retransmission_timeout(_initial_retransmission_timeout)
    , _consecutive_retransmissions(0)
    , _window_size(1)
    , _expected_seqno(0)
    , _outstanding()
    , _syn(false)
    , _fin(false) {}

uint64_t TCPSender::bytes_in_flight() const { return _next_seqno - _expected_seqno; }

void TCPSender::fill_window() {
    uint16_t actual_window_size = max(_window_size, static_cast<uint16_t>(1));

    if (!_syn) {
        _syn = true;

        TCPSegment seg;
        seg.header().syn = true;
        seg.header().seqno = _isn;

        _next_seqno = 1;

        _segments_out.push(seg);
        _outstanding.emplace(_next_seqno, seg);
    }

    while (!_stream.buffer_empty() && _next_seqno < _expected_seqno + actual_window_size) {
        uint16_t length = min(min(_expected_seqno + actual_window_size - _next_seqno, _stream.buffer_size()),
                              TCPConfig::MAX_PAYLOAD_SIZE);

        TCPSegment seg;
        seg.header().seqno = next_seqno();
        seg.payload() = Buffer(move(_stream.read(length)));

        _next_seqno += length;

        if (_stream.eof() && _next_seqno < _expected_seqno + actual_window_size) {
            seg.header().fin = true;
            _fin = true;
            ++_next_seqno;
        }

        _segments_out.push(seg);
        _outstanding.emplace(_next_seqno, seg);
    }

    if (!_fin && _stream.eof() && _next_seqno < _expected_seqno + actual_window_size) {
        _fin = true;
        TCPSegment seg;
        seg.header().fin = true;
        seg.header().seqno = next_seqno();

        ++_next_seqno;

        _segments_out.push(seg);
        _outstanding.emplace(_next_seqno, seg);
    }

    if (!_alarm && !_outstanding.empty()) {
        _alarm = true;
        _time = 0;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    DUMMY_CODE(ackno, window_size);

    uint64_t n = unwrap(ackno, _isn, _next_seqno);
    if (n > _next_seqno) {
        return;
    }

    if (n > _expected_seqno) {
        bool valid = false;
        while (!_outstanding.empty() && _outstanding.front().first <= n) {
            _expected_seqno = _outstanding.front().first;
            _outstanding.pop();
            valid = true;
        }

        if (valid) {
            _alarm = false;
            _consecutive_retransmissions = 0;
            _retransmission_timeout = _initial_retransmission_timeout;
            if (!_outstanding.empty()) {
                _alarm = true;
                _time = 0;
            }
        }
    }

    _window_size = window_size;
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    DUMMY_CODE(ms_since_last_tick);
    if (!_alarm) {
        return;
    }
    _time += ms_since_last_tick;
    if (_time >= _retransmission_timeout) {
        _time = 0;
        ++_consecutive_retransmissions;
        if (_window_size != 0) {
            _retransmission_timeout *= 2;
        }
        _segments_out.push(_outstanding.front().second);
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    _segments_out.push(seg);
}
