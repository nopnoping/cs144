#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // 1. the end byte of substring is in limit?
    // 2. the start of substring is start of unassembled?
    // 3. the unassembled bytes need send to _output?
    // 4. the start of unasssembled how to update?
    size_t end_substring = index + data.length();
    if (end_substring <= _wish_index - _output.buffer_size() + _capacity
        && index >= _wish_index
        && _map.find(index) == _map.end()) {

        if (index == _wish_index) {
            _output.write(data);
            _wish_index += data.length();
            while (_map.find(_wish_index) != _map.end()) {
                _output.write(_map[_wish_index]);
                size_t l = _map[_wish_index].length();
                _map.erase(_wish_index);
                _wish_index += l;
            }
        } else {
            _map[index] = data;
        }

        if (eof) {
            _output.end_input();
        }

    }
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t num = 0;
    for (auto &[k, v] : _map) {
        num += v.length();
    }
    return num;
}

bool StreamReassembler::empty() const {
    return _map.empty();
}
