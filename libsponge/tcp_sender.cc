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
       total += r.get_segment().length_in_sequence_space();
    }
    return total;
}

void TCPSender::fill_window() {
    // 看下还可以发送多少字节
    int window_size = _window_size == 0 ? 1 : _window_size;
    int can_send_size = static_cast<int>(window_size - bytes_in_flight());
    bool payload_empty_can_send = _window_size == 0;
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
        if (segment.length_in_sequence_space() == 0 && !payload_empty_can_send)
            break;
        segment.header().seqno = wrap(_next_seqno, _isn);
        RetransmissionTimer timer(segment, 0, _next_seqno);
        _segments_out.push(segment);
        _outgoing_segment.push_back(timer);

        _next_seqno += segment.length_in_sequence_space();
        can_send_size -= payload_size;
        if (payload_empty_can_send)
            break ;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    _window_size = window_size;
    _last_absolute_no = unwrap(ackno, _isn, _last_absolute_no);

    // 移除已经接收的segment
    bool can_clear_time = false;
    while (!_outgoing_segment.empty()
           && _last_absolute_no >=
                  _outgoing_segment.front().get_segment().length_in_sequence_space()
                      + _outgoing_segment.front().absolute_no()
           && _last_absolute_no <= _next_seqno) {
        _outgoing_segment.pop_front();
        can_clear_time = true;
    }
    // 清除时间
    for (size_t i=0; i<_outgoing_segment.size() && can_clear_time; i++) {
        _outgoing_segment[i].clear_time();
    }
    fill_window();
    _timeout = _initial_retransmission_timeout;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    for (size_t i=0; i<_outgoing_segment.size(); i++) {
        _outgoing_segment[i].add_time(ms_since_last_tick);
    }
    if (_window_size == 0) {
        _timeout = _initial_retransmission_timeout;
    }
    // 超时，重传
    if (!_outgoing_segment.empty() && _outgoing_segment[0].is_time_out(_timeout)) {
        _segments_out.push(_outgoing_segment[0].get_segment());
        _outgoing_segment[0].up_retrans_time();
        _timeout = _timeout * 2;
        _outgoing_segment[0].clear_time();
    }
}

unsigned int TCPSender::consecutive_retransmissions() const {
    return _outgoing_segment.empty() ? 0 : _outgoing_segment[0].retrans_time();
}

void TCPSender::send_empty_segment() {
    TCPSegment segment;
    segment.header().syn = true;
    segment.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(segment);
}

//! RetransmissionTimer
RetransmissionTimer::RetransmissionTimer(TCPSegment segment, size_t timeout, uint64_t absolute_no)
    : _segment(segment), _timeout(timeout), _absolute_no(absolute_no) {}

bool RetransmissionTimer::is_time_out(size_t rst) {
    if (_timeout >= rst)
        return true;
    return false;
}

// 对于发送，需要关注的几点：1. TCPsegment中的seqno; 2. 窗口函数大小; 3. 重试
// 如何存储正在处理中的segment？这里是认为只要push进deque，就立刻发送了；所以在fill window的时候，直接加入
// 用什么结构来存储呢？由于过期的时候，会将最小的序列重试，并且计算连续需要重试的个数，所以需要排序，因此用list来存，然后插入排序
// 但是真的需要排序吗？从流中获取的数据，本来就是有序的，所以放入也是有序的，因此没必要用list，直接用deque即可

