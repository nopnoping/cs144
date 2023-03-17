#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const {
    return _sender.stream_in().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const {
    return _sender.bytes_in_flight();
}

size_t TCPConnection::unassembled_bytes() const {
    return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const {
    return _time_since_last_segment_received;
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (_has_sent_fin && _linger_after_streams_finish && _time_since_last_segment_received >= 10 * _cfg.rt_timeout)
        _active = false;
    // 处理keep-alive case
    if (_receiver.ackno().has_value() && seg.length_in_sequence_space() == 0
        && seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
    } else {
        _receiver.segment_received(seg);
        if (seg.header().ack) {
            _sender.ack_received(seg.header().ackno, seg.header().win);
        }
    }

    // 收到了fin，但是之前没有发出过fin，则不需要linger
    if (_receiver.stream_out().input_ended() && !_has_sent_fin)
        _linger_after_streams_finish = false;

    // 把所有segment发出
    _sender.fill_window();
    bool is_send = send_all_segment();
    if (!is_send) {
        _sender.send_empty_segment();
    }
    send_all_segment();

    _time_since_last_segment_received = 0;
}

bool TCPConnection::active() const {
    if (_has_sent_fin && _linger_after_streams_finish && _time_since_last_segment_received >= 10 * _cfg.rt_timeout)
        return false;
    return _active;
}

size_t TCPConnection::write(const string &data) {
    size_t s = _sender.stream_in().write(data);
    return s;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() >= TCPConfig::MAX_RETX_ATTEMPTS) {
        send_rst_segment();
        return ;
    }
    send_all_segment();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
}

void TCPConnection::connect() {
    _sender.fill_window();
    _segments_out.push(_sender.segments_out().front());
    _sender.segments_out().pop();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            send_rst_segment();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

// helper 正常情况下，发送所有数据
bool TCPConnection::send_all_segment() {
    bool is_send = false;
    while (!_sender.segments_out().empty()) {
        TCPSegment& seg_send = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            seg_send.header().ack = true;
            seg_send.header().ackno = _receiver.ackno().value();
            seg_send.header().win = _receiver.window_size();
        }
        _segments_out.push(seg_send);
        // 发送了FIN，并且不是linger状态
        if (seg_send.header().fin && !_linger_after_streams_finish)
            _active = false;
        if (seg_send.header().fin)
            _has_sent_fin = true;
        is_send = true;
    }
    return is_send;
}

void TCPConnection::send_rst_segment() {
    if (_sender.segments_out().empty())
        _sender.send_empty_segment();
    TCPSegment rst_seg = _sender.segments_out().front();_sender.segments_out().pop();
    rst_seg.header().rst = true;
    _segments_out.push(rst_seg);
    _active = false;
}
