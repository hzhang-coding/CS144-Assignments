#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : _capacity(capacity), _stream(), _isEnd(false), _totalWriten(0), _totalRead(0), _error(false) {
    DUMMY_CODE(_capacity);
}

size_t ByteStream::write(const string &data) {
    DUMMY_CODE(data);
    if (_isEnd) {
        return 0;
    }
    size_t n = min(data.size(), remaining_capacity());
    _stream.append(BufferList(move(data.substr(0, n))));
    _totalWriten += n;
    return n;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    DUMMY_CODE(len);
    size_t n = len;
    string s;
    for (auto &buf : _stream.buffers()) {
        if (n == 0) {
            break;
        }
        size_t m = min(buf.size(), n);
        s += buf.str().substr(0, m);
        n -= m;
    }
    return s;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    DUMMY_CODE(len);

    size_t n = min(len, _stream.size());
    _totalRead += n;
    _stream.remove_prefix(n);
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    DUMMY_CODE(len);

    size_t n = len;
    string s;
    while (n > 0 && !_stream.buffers().empty()) {
        if (n >= _stream.buffers().front().size()) {
            s += _stream.buffers().front().str();
            n -= _stream.buffers().front().size();
            _stream.pop_front();
        } else {
            s += _stream.buffers().front().str().substr(0, n);
            _stream.remove_prefix(n);
            n = 0;
        }
    }
    _totalRead += s.size();
    return s;
}

void ByteStream::end_input() { _isEnd = true; }

bool ByteStream::input_ended() const { return _isEnd; }

size_t ByteStream::buffer_size() const { return _stream.size(); }

bool ByteStream::buffer_empty() const { return _stream.size() == 0; }

bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _totalWriten; }

size_t ByteStream::bytes_read() const { return _totalRead; }

size_t ByteStream::remaining_capacity() const { return _capacity - buffer_size(); }
