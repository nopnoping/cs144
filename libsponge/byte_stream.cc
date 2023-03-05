#include "byte_stream.hh"
#include <sstream>

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) {
    this->capacity = capacity;
    is_end = false;
    write_num = 0;
    read_num = 0;
}

size_t ByteStream::write(const string &data) {
    size_t haveWriten = 0;
    for (auto byte : data) {
        if (stream.size() < capacity) {
            stream.push_back(byte);
            haveWriten++;
            write_num++;
        }
    }
    return haveWriten;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    std::stringstream  ss;

    for (size_t i=0; i<len && i<stream.size(); i++) {
        ss << stream[i];
    }

    return ss.str();
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    for (size_t i=0; i<len && !stream.empty(); i++) {
        stream.pop_front();
        read_num++;
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string ans = peek_output(len);
    pop_output(len);
    return ans;
}

void ByteStream::end_input() {
    is_end = true;
}

bool ByteStream::input_ended() const {
    return is_end;
}

size_t ByteStream::buffer_size() const {
    return stream.size();
}

bool ByteStream::buffer_empty() const {
    return stream.empty();
}

bool ByteStream::eof() const {
    return is_end & stream.empty();
}

size_t ByteStream::bytes_written() const {
    return write_num;
}

size_t ByteStream::bytes_read() const {
    return read_num;
}

size_t ByteStream::remaining_capacity() const {
    return capacity - stream.size();
}
