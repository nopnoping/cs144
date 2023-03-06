#include "stream_reassembler.hh"
#include <sstream>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity),
    _buffer(capacity), _bitmap(capacity){}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // only last word write to buffer
    // the eof can be initial
    if (eof) {
        size_t len = data.length();
        if (index + len <= _buffer_start - _output.buffer_size() + _capacity) {
            _eof = true;
        }
    }
    // the index is in capacity
    for (size_t i = index; i < _buffer_start - _output.buffer_size() + _capacity && i < index + data.length(); i++) {
        if (i >= _buffer_start && !_bitmap[i-_buffer_start]) {
            _bitmap[i-_buffer_start] = true;
            _buffer[i-_buffer_start] = data[i-index];
            _unass_size++;
        }
    }

    // write to _out
    std::stringstream ss;
    while (_bitmap.front()) {
        ss << _buffer.front();
        _buffer_start++;
        _unass_size--;
        _buffer.pop_front();
        _bitmap.pop_front();
        _buffer.push_back('\0');
        _bitmap.push_back(false);
    }
    _output.write(ss.str());

    if (_eof && _unass_size == 0) {
        _output.end_input();
    }

}

size_t StreamReassembler::unassembled_bytes() const {
    return _unass_size;
}

bool StreamReassembler::empty() const {
    return _unass_size == 0;
}
