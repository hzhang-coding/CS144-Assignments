#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _expected(), _record(), _recveof(false), _end(0), _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    DUMMY_CODE(data, index, eof);
    if (_output.input_ended()) {
        return;
    }
    size_t n = data.size();
    if (eof) {
        _recveof = true;
        _end = index + n;
        if (_expected == _end) {
            _output.end_input();
        }
    }
    uint64_t _boundary = _expected + _capacity - _output.buffer_size();
    if (index + n <= _expected || index >= _boundary) {
        return;
    }
    uint64_t right = min(_boundary, index + n);
    if (_record.find(index) != _record.end() && _record[index].size() >= right - index) {
        return;
    }
    if (index + n <= _boundary) {
        _record[index] = move(data);
    } else {
        _record[index] = move(data.substr(0, _boundary - index));
    }
    auto it = _record.begin();
    while (it != _record.end() && it->first <= _expected) {
        if (it->first + it->second.size() > _expected) {
            _output.write(it->second.substr(_expected - it->first));
        }
        _expected = max(_expected, it->first + it->second.size());
        _record.erase(it++);
    }
    if (_recveof && _expected == _end) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t cnt = 0;
    uint64_t curr = _expected;
    auto it = _record.begin();
    while (it != _record.end()) {
        if (curr < it->first + it->second.size()) {
            cnt += it->first + it->second.size() - max(curr, it->first);
            curr = it->first + it->second.size();
        }
        ++it;
    }
    return cnt;
}

bool StreamReassembler::empty() const { return _record.empty(); }

uint64_t StreamReassembler::expected_index() const { return _expected; }
