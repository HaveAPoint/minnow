#include "tcp_receiver.hh"
#include "debug.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  // 处理RST标志
  if (message.RST) {
    reassembler_.reader().set_error();
    return;
  }

  // 处理SYN标志（建立连接）
  if (message.SYN && !isn_.has_value()) {
    isn_ = message.seqno;
  }
  
  // 在接收到SYN之前不处理数据
  if (!isn_.has_value()) {
    return;
  }
  
  // 计算绝对序列号和流索引
  const uint64_t checkpoint = reassembler_.writer().bytes_pushed();
  const uint64_t abs_seqno = message.seqno.unwrap(isn_.value(), checkpoint);
  
  // 确定流索引（字节流中的位置）
  const uint64_t stream_index = message.SYN ? 0 : abs_seqno - 1;
  
  // 插入数据到重组器
  reassembler_.insert(stream_index, message.payload, message.FIN);
}

TCPReceiverMessage TCPReceiver::send() const 
{
  TCPReceiverMessage msg;
  
  // 只有在建立连接(接收SYN)后才设置ackno
  if (isn_.has_value()) {
    // 计算绝对ackno: SYN(1) + bytes_pushed + FIN(如果适用)
    const uint64_t abs_ackno = 1 + reassembler_.writer().bytes_pushed() 
                             + (reassembler_.writer().is_closed() ? 1 : 0);
    
    // 转换为32位包装序列号
    msg.ackno = Wrap32::wrap(abs_ackno, isn_.value());
  }
  
  // 设置窗口大小（上限为uint16_t最大值）
  const uint64_t window_size = reassembler_.writer().available_capacity();
  msg.window_size = static_cast<uint16_t>(
      std::min(window_size, static_cast<uint64_t>(UINT16_MAX)));
  
  // 设置RST标志（如果流有错误）
  msg.RST = reassembler_.reader().has_error();
  
  return msg;
}

