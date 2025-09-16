#pragma once

#include "address.hh"
#include "ethernet_frame.hh"
#include "ipv4_datagram.hh"

#include <memory>
#include <queue>
#include <unordered_map>

// 一个连接 IP（互联网层或网络层）
// 与以太网（网络访问层或链路层）的"网络接口"。

// 这个模块是 TCP/IP 协议栈的最低层
// （将 IP 与较低层的网络协议连接起来，
// 例如以太网）。但同一个模块也被反复用作路由器的一部分：
// 路由器通常有许多网络接口，路由器的任务是在不同接口之间
// 路由互联网数据报。

// 网络接口将数据报（来自"客户"，例如 TCP/IP 协议栈或路由器）
// 转换为以太网帧。为了填写以太网目的地址，它查找每个数据报
// 下一个 IP 跃点的以太网地址，使用[地址解析协议](\ref rfc::rfc826)进行请求。
// 在相反方向，网络接口接受以太网帧，检查它们是否针对它，
// 如果是，则根据其类型处理有效载荷。如果它是 IPv4 数据报，
// 网络接口将其传递到协议栈上层。如果它是 ARP 请求或回复，
// 网络接口处理该帧并根据需要学习或回复。
class NetworkInterface
{
public:
  // NetworkInterface 发送以太网帧的物理输出端口的抽象
  class OutputPort
  {
  public:
    virtual void transmit( const NetworkInterface& sender, const EthernetFrame& frame ) = 0;
    virtual ~OutputPort() = default;
  };

  // 使用给定的以太网（网络访问层）和 IP（互联网层）地址构造网络接口
  NetworkInterface( std::string_view name,
                    std::shared_ptr<OutputPort> port,
                    const EthernetAddress& ethernet_address,
                    const Address& ip_address );

  // 发送互联网数据报，封装在以太网帧中（如果它知道以太网目的地址）。
  // 将需要使用[ARP](\ref rfc::rfc826)查找下一个跃点的以太网目的地址。
  // 发送通过在帧上调用`transmit()`（成员变量）来完成。
  void send_datagram( const InternetDatagram& dgram, const Address& next_hop );

  // 接收以太网帧并相应地响应。
  // 如果类型是 IPv4，将数据报推送到 datagrams_in 队列。
  // 如果类型是 ARP 请求，从"sender"字段学习映射，并发送 ARP 回复。
  // 如果类型是 ARP 回复，从"sender"字段学习映射。
  void recv_frame( EthernetFrame frame );

  // 当时间流逝时定期调用
  void tick( size_t ms_since_last_tick );

  // 访问器
  const std::string& name() const { return name_; }
  const OutputPort& output() const { return *port_; }
  OutputPort& output() { return *port_; }
  std::queue<InternetDatagram>& datagrams_received() { return datagrams_received_; }

private:
  // 接口的人类可读名称
  std::string name_;

  // 物理输出端口（+ 一个辅助函数`transmit`，它使用它来发送以太网帧）
  std::shared_ptr<OutputPort> port_;
  void transmit( const EthernetFrame& frame ) const { port_->transmit( *this, frame ); }

  // 接口的以太网（称为硬件、网络访问层或链路层）地址
  EthernetAddress ethernet_address_;

  // 接口的 IP（称为互联网层或网络层）地址
  Address ip_address_;

  // 已接收的数据报
  std::queue<InternetDatagram> datagrams_received_ {};

  static constexpr size_t ARP_ENTRY_TTL_ms = 30'000;
  static constexpr size_t ARP_REQUEST_PERIOD_ms = 5'000;
  using Timer = uint64_t;
  using AddressNumber = uint32_t;

  struct ArpEntry {
    EthernetAddress ethernet_address;
    Timer timer;
  };

  //ip to anything
  std::unordered_map <AddressNumber, ArpEntry> arp_cache_ {};
  std::unordered_map <AddressNumber, std::vector<InternetDatagram>> pending_datagrams_ {};
  std::unordered_map <AddressNumber, Timer> pending_datagram_timers_ {};
};

// 中文翻译说明：
// 这个文件定义了 NetworkInterface 类，它是 TCP/IP 协议栈中的网络接口层。
// 主要功能包括：
// 1. 将 IP 数据报封装成以太网帧发送
// 2. 接收以太网帧并处理其中的 IP 数据报或 ARP 消息
// 3. 管理 ARP 地址解析
// 4. 提供定时器功能处理超时
// 
// 关键组件：
// - OutputPort: 抽象的输出端口接口，用于发送以太网帧
// - send_datagram(): 发送 IP 数据报，需要 ARP 解析目的 MAC 地址
// - recv_frame(): 接收并处理以太网帧
// - tick(): 处理定时事件，如 ARP 请求超时
// 
// 这个类是网络层和链路层的桥梁，在 TCP/IP 协议栈中扮演重要角色。
