#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <utility>

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
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const {
    size_t total = 0;
    for (auto& r:_outgoing_segment) {
       total += r.length_in_sequence_space();
    }
    return total;
}

void TCPSender::fill_window() {
    // 看下还可以发送多少字节
    int window_size = _window_size == 0 ? 1 : _window_size;
    int can_send_size = static_cast<int>(window_size - bytes_in_flight());
    while (can_send_size > 0 && !_finished) {
        // 构造segment
        TCPSegment segment;
        // 看下是不是第一次构造
        if (_next_seqno == 0) {
            segment.header().syn = true;
        }
        uint16_t payload_size = min(static_cast<size_t>(can_send_size - segment.length_in_sequence_space())
                                        , TCPConfig::MAX_PAYLOAD_SIZE);
        // 读取数据
        segment.payload() = Buffer(_stream.read(payload_size));
        // 是否已经读完
        if (_stream.eof() && can_send_size-segment.length_in_sequence_space() > 0) {
            segment.header().fin = true;
            _finished = true;
        }
        // 如果没有数据
        if (segment.length_in_sequence_space() == 0) {
            break;
        }
        segment.header().seqno = wrap(_next_seqno, _isn);
        _segments_out.push(segment);
        _outgoing_segment.push_back(segment);

        _next_seqno += segment.length_in_sequence_space();
        can_send_size -= payload_size;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    _window_size = window_size;
    uint64_t absolute_no = unwrap(ackno, _isn, next_seqno_absolute());

    // 移除已经接收的segment
    while (!_outgoing_segment.empty()
           && absolute_no >=
                  _outgoing_segment.front().length_in_sequence_space()
                      + unwrap(_outgoing_segment.front().header().seqno, _isn, _next_seqno)
           && absolute_no <= _next_seqno) {
        _outgoing_segment.pop_front();
        _time = 0;
        _retrans_time = 0;
    }
    fill_window();
    _timeout = _initial_retransmission_timeout;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (_window_size == 0) {
        _timeout = _initial_retransmission_timeout;
    }

    if (!_outgoing_segment.empty()) {
        _time += ms_since_last_tick;
        if (_time >= _timeout) {
            _retrans_time++;
            _time = 0;
            _timeout *= 2;
            _segments_out.push(_outgoing_segment.front());
        }
    } else {
        _time = 0;
        _retrans_time = 0;
        _timeout = _initial_retransmission_timeout;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const {
    return _retrans_time;
}

void TCPSender::send_empty_segment() {
    TCPSegment segment;
    segment.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(segment);
}