#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    DUMMY_CODE(seg);
    if (!_syn) {
        if (seg.header().syn) {
            _syn = true;
            _isn = seg.header().seqno;
        } else {
            return;
        }
    }
    uint64_t checkpoint = _reassembler.expected_index() + 1;
    uint64_t index = unwrap(seg.header().seqno, _isn, checkpoint) - 1 + seg.header().syn;
    _reassembler.push_substring(seg.payload().copy(), index, seg.header().fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_syn) {
        return nullopt;
    }
    uint64_t n = _reassembler.expected_index() + 1 + stream_out().input_ended();
    return wrap(n, _isn);
}

size_t TCPReceiver::window_size() const { return _capacity - stream_out().buffer_size(); }
