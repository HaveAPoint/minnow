#include "tcp_sender.hh"
#include "debug.hh"
#include "tcp_config.hh"

using namespace std;

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return bytes_in_flight_;
}

// This function is for testing only; don't add extra state to support it.
uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retransmissions_;
}

void TCPSender::push(const TransmitFunction& transmit)
{
  // 计算有效窗口大小(将0窗口视为1进行窗口探测)
  const uint16_t effective_window = window_size_ ? window_size_ : 1;

  // 持续发送直到窗口用尽或FIN已发送
  while (bytes_in_flight_ < effective_window && !fin_sent_) {
    TCPSenderMessage msg = make_empty_message();

    // 如果连接未初始化，设置SYN标志
    if (!syn_sent_) {
      msg.SYN = true;
      syn_sent_ = true;
    }

    // 计算考虑窗口和已发送数据后的可用负载空间
    const uint64_t remaining_capacity = effective_window - bytes_in_flight_;
    const size_t max_payload = min(remaining_capacity - msg.sequence_length(), // 考虑SYN/FIN占用
                                  TCPConfig::MAX_PAYLOAD_SIZE);

    // 从输入流填充负载，不超过计算出的最大容量
    while (reader().bytes_buffered() && msg.payload.size() < max_payload) {
      const string_view data = reader().peek().substr(0, max_payload - msg.payload.size());
      msg.payload += data;
      reader().pop(data.size());
    }

    // 如果流已结束且窗口允许，设置FIN标志
    if (!fin_sent_ && reader().is_finished() && (remaining_capacity > msg.sequence_length())) {
      msg.FIN = true;
      fin_sent_ = true;
    }

    // 跳过空段(除了SYN/FIN外)
    if (msg.sequence_length() == 0)
      break;

    // 传输段并更新跟踪状态
    transmit(msg);
    next_seqno_ += msg.sequence_length();
    bytes_in_flight_ += msg.sequence_length();
    outstanding_messages_.push(move(msg));

    // 如果定时器未运行，启动重传定时器
    if (!timer_running_) {
      timer_running_ = true;
      timer_ = 0;
    }
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage msg;
  msg.seqno = isn_ + next_seqno_;
  msg.RST = input_.has_error();
  return msg;
}

void TCPSender::receive(const TCPReceiverMessage& msg)
{
  // 处理错误状态
  if (input_.has_error())
    return;
  if (msg.RST) {
    input_.set_error();
    return;
  }

  // 更新接收方窗口大小
  window_size_ = msg.window_size;

  // 如果没有确认号，直接返回
  if (!msg.ackno)
    return;

  // 将相对确认号转换为绝对序列空间
  const uint64_t ack_abs = msg.ackno->unwrap(isn_, next_seqno_);

  // 验证确认号
  if (ack_abs > next_seqno_)
    return; // 确认了未发送的数据

  // 忽略旧的确认
  if (ack_abs <= ackno_)
    return;

  bool acked = false;
  // 处理所有完全确认的段
  while (!outstanding_messages_.empty()) {
    const auto& front_msg = outstanding_messages_.front();
    const uint64_t segment_start = front_msg.seqno.unwrap(isn_, next_seqno_);
    const uint64_t segment_end = segment_start + front_msg.sequence_length();

    if (segment_end <= ack_abs) {
      // 段完全被确认，可以移除
      acked = true;
      bytes_in_flight_ -= front_msg.sequence_length();
      outstanding_messages_.pop();
    } else {
      // 段部分被确认或未被确认，停止处理
      break;
    }
  }

  // 更新已确认序列号
  ackno_ = ack_abs;

  // 如果有段被确认，重置定时器状态
  if (acked) {
    timer_ = 0;
    current_RTO_ms_ = initial_RTO_ms_;
    consecutive_retransmissions_ = 0;
    timer_running_ = !outstanding_messages_.empty();
  }
}

void TCPSender::tick(uint64_t ms_since_last_tick, const TransmitFunction& transmit)
{
  // 只有在定时器运行时更新计时器
  if (timer_running_) {
    timer_ += ms_since_last_tick;
  }

  // 检查超时条件
  if (timer_running_ && timer_ >= current_RTO_ms_ && !outstanding_messages_.empty()) {
    // 重传最早的未确认段
    transmit(outstanding_messages_.front());

    // 只有当窗口打开时应用指数退避
    if (window_size_ > 0) {
      consecutive_retransmissions_++;
      current_RTO_ms_ *= 2;
    }

    // 重置定时器以准备下一次可能的重传
    timer_ = 0;
  }
}