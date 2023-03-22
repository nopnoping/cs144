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


void TCPConnection::handle_segment(const TCPSegment &seg) {
    // RST
    if (seg.header().rst) {
        rst_error();
        return ;
    }
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

    // established -> close-wait
    // 收到remote的fin，但是自己没有fin
    if (_receiver.stream_out().input_ended() && !_has_sent_fin)
        _linger_after_streams_finish = false;

    // 内容全部发送，并且都被ack
    if (_has_sent_fin && _sender.bytes_in_flight() == 0) {
        if (!_linger_after_streams_finish) {
            _active = false;
            return ;
        } else if (_receiver.stream_out().input_ended()) {
            _time_wait = true;
        }
    }

    // 把所有segment发出
    _sender.fill_window();
    if (_sender.segments_out().empty() &&
        (seg.length_in_sequence_space() != 0 ||
         _sender.is_future_ackno(seg.header().ackno))) {
        _sender.send_empty_segment();
    }
    send_all_segment();

    _time_since_last_segment_received = 0;
}
void TCPConnection::segment_received(const TCPSegment &seg) {

    if (!_active)
        return ;

    // CLOSED/LISTEN
    if (!_send_syn && !_rev_syn) {
        // SYN_RECEIVED
        if (seg.header().syn && !seg.header().ack && !seg.header().rst) {
            _rev_syn = true;
            handle_segment(seg);
            return ;
        }
    }

    // SYN_SENT (ack+rst, ack+syn)
    if (_send_syn && !_rev_syn) {
        if (!seg.header().ack && seg.header().syn) {
            _rev_syn = true;
            handle_segment(seg);
            return ;
        }
        if (seg.header().ack && seg.header().ackno == _sender.next_seqno()) {
            if (seg.header().rst) {
                rst_error();
                return ;
            }
            if (seg.header().syn) {
                _rev_syn = true;
                handle_segment(seg);
                return ;
            }
        }
    }

    // ESTABLISHED
    if (_send_syn && _rev_syn)
        handle_segment(seg);
}

bool TCPConnection::active() const {
    return _active;
}

size_t TCPConnection::write(const string &data) {
    size_t s = _sender.stream_in().write(data);
    _sender.fill_window();
    send_all_segment();
    return s;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _time_since_last_segment_received += ms_since_last_tick;
    // TIME_WAIT
    if (_time_wait && _time_since_last_segment_received >= 10 * _cfg.rt_timeout)
        _active = false;

    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        send_rst_segment(true);
        return ;
    }
    send_all_segment();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_all_segment();
}

void TCPConnection::connect() {
    _sender.fill_window();
    send_all_segment();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            send_rst_segment(true);
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

// helper 正常情况下，发送所有数据
void TCPConnection::send_all_segment() {
    while (!_sender.segments_out().empty()) {
        TCPSegment& seg_send = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            seg_send.header().ack = true;
            seg_send.header().ackno = _receiver.ackno().value();
            seg_send.header().win = _receiver.window_size();
        }
        _segments_out.push(seg_send);
        if (seg_send.header().syn)
            _send_syn = true;
        if (seg_send.header().fin)
            _has_sent_fin = true;
    }
}

void TCPConnection::send_rst_segment(bool error) {
    if (_sender.segments_out().empty())
        _sender.send_empty_segment();
    TCPSegment rst_seg = _sender.segments_out().front();_sender.segments_out().pop();
    rst_seg.header().rst = true;
    _segments_out.push(rst_seg);
    if (error)
        rst_error();
}

void TCPConnection::rst_error() {
    _active = false;
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
}
