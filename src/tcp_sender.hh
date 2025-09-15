#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <functional>
#include <queue>

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), initial_RTO_ms_( initial_RTO_ms ), current_RTO_ms_( initial_RTO_ms )
  {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  
  // For testing: how many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; 
  // For testing: how many consecutive retransmissions have happened?
  const Writer& writer() const { return input_.writer(); }
  const Reader& reader() const { return input_.reader(); }
  Writer& writer() { return input_.writer(); }

private:
  Reader& reader() { return input_.reader(); }

  ByteStream input_;
  Wrap32 isn_;

  /*state flag*/
  bool syn_sent_{};                     //has the syn flag been sent?
  bool fin_sent_{};                     //has the FIN flag been sent?

  /* window management */
  uint16_t window_size_{ 1 };
  // 窗口管理 - 接收方通告的窗口大小
  // 16位：TCP标准窗口字段大小
  // 初始值1：默认值，实际由对端ACK更新
  // 为0时视为1进行零窗口探测

  uint64_t next_seqno_ {};
  // 下一个要使用的绝对序列号（相对isn_的偏移）
  // 64位：避免回绕，简化计算
  // 初始值0：从ISN开始计算
  // 每发送段后增加sequence_length()

  uint64_t ackno_ {};
  // 已被确认的最高绝对序列号
  // 用于判断哪些outstanding段可以移除
  // receive()中根据对端ACK更新
  // 满足不变量：ackno_ <= next_seqno_

  uint64_t bytes_in_flight_ {};
  // 已发送但未确认的字节总数
  // 用于流量控制：bytes_in_flight_ < window_size_
  // 发送时增加，收到ACK时减少
  // 等于outstanding_messages_中所有段的sequence_length()之和

  /* Retransmission timer management */
  uint64_t initial_RTO_ms_ {};
  // 初始重传超时时间（毫秒）
  // 构造函数参数设置，整个连接期间不变
  // 收到新ACK时current_RTO_ms_重置为此值

  uint64_t current_RTO_ms_ {};
  // 当前使用的重传超时时间（毫秒）
  // 初始等于initial_RTO_ms_
  // 超时重传时指数退避（*=2）
  // 收到新ACK时重置为initial_RTO_ms_

  uint64_t timer_{};
  // 当前计时器累计时间（毫秒）
  // tick()中累加ms_since_last_tick
  // 达到current_RTO_ms_时触发重传
  // 超时或收到ACK时重置为0

  bool timer_running_ {};
  // 计时器运行状态标志
  // 有未确认段时为true，队列空时为false
  // 控制是否在tick()中累加时间和检查超时

  uint64_t consecutive_retransmissions_{};
  // 连续重传次数计数器
  // 每次超时重传时+1
  // 收到新ACK时重置为0
  // 用于测试和可能的连接放弃策略

  /*outstanding segments waiting for acknowledgment*/
  std::queue<TCPSenderMessage> outstanding_messages_ {};
  // 等待确认的段队列（FIFO）
  // 保存已发送但未被完全确认的TCPSenderMessage
  // 队头：最早发送的段，重传时优先处理
  // receive()中按序移除已确认的前缀段
  // 队列为空时停止计时器
};
