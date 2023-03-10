#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    bool eof = false;
    if (seg.header().fin)
        eof = true;

    if (!_is_receive && seg.header().syn) {
        _is_receive = true;
        _initial_seq_no = seg.header().seqno;
        _reassembler.push_substring(seg.payload().copy(), 0, eof);
    } else if (_is_receive) {
        uint64_t checkpoint = _reassembler.get_buffer_start() == 0 ? 0 : _reassembler.get_buffer_start() - 1;
        uint64_t absolute_seqno = unwrap(seg.header().seqno, _initial_seq_no, checkpoint);
        _reassembler.push_substring(seg.payload().copy(), absolute_seqno-1, eof);
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_is_receive)
        return nullopt;
    uint64_t absolute_sqno = _reassembler.get_buffer_start() + 1;
    if (_reassembler.stream_out().input_ended())
        absolute_sqno++;
    return wrap(absolute_sqno, _initial_seq_no);
}

size_t TCPReceiver::window_size() const {
    return _capacity - _reassembler.stream_out().buffer_size();
}
