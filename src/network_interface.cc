#include <iostream>

#include "arp_message.hh"
#include "debug.hh"
#include "ethernet_frame.hh"
#include "exception.hh"
#include "helpers.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  const AddressNumber next_hop_ip = next_hop.ipv4_numeric();
  auto arp_it = arp_cache_.find( next_hop_ip );
  if (arp_it != arp_cache_.end()) {
    const EthernetAddress& next_hop_ethernet_address = arp_it->second.ethernet_address;
    transmit( {{ next_hop_ethernet_address, ethernet_address_, EthernetHeader::TYPE_IPv4 }, serialize( dgram )});
    return;
  }

  pending_datagrams_[next_hop_ip].emplace_back( dgram );

  if ( pending_datagram_timers_.find( next_hop_ip ) != pending_datagram_timers_.end()) {
    return;
  }

  pending_datagram_timers_.emplace( next_hop_ip, Timer {} );

  const ARPMessage arp_request = {
    .opcode = ARPMessage::OPCODE_REQUEST,
    .sender_ethernet_address = ethernet_address_,
    .sender_ip_address = ip_address_.ipv4_numeric(),
    .target_ethernet_address = {},
    .target_ip_address = next_hop_ip

  };

  transmit ( { { ETHERNET_BROADCAST, ethernet_address_, EthernetHeader::TYPE_ARP }, serialize(arp_request)});

}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( EthernetFrame frame )
{
  if ( frame.header.dst != ethernet_address_ && frame.header.dst != ETHERNET_BROADCAST) {
    return;
  }

  if ( frame.header.type == EthernetHeader::TYPE_IPv4 ) {
    InternetDatagram dgram;
    if ( parse( dgram, frame.payload ) ) {
      datagrams_received_.push( move ( dgram ));
    }
    return;
  }

  if ( frame.header.type == EthernetHeader::TYPE_ARP ) {
    ARPMessage msg;
    if ( !parse( msg, frame.payload ) ) {
      return;
    }

    const AddressNumber sender_ip = msg.sender_ip_address;
    const EthernetAddress sender_eth = msg.sender_ethernet_address;

    arp_cache_[sender_ip] = { sender_eth, 0};

    // 处理ARP请求
    if (msg.opcode == ARPMessage::OPCODE_REQUEST && msg.target_ip_address == ip_address_.ipv4_numeric()) {
      ARPMessage arp_reply = {
        .opcode = ARPMessage::OPCODE_REPLY,
        .sender_ethernet_address = ethernet_address_,
        .sender_ip_address = ip_address_.ipv4_numeric(),
        .target_ethernet_address = sender_eth,
        .target_ip_address = sender_ip
      };

      transmit({ { sender_eth, ethernet_address_, EthernetHeader::TYPE_ARP}, serialize(arp_reply) });
    }

    // 无论是请求还是回复，只要更新了ARP缓存，就检查待处理数据报
    auto it = pending_datagrams_.find(sender_ip);
    if (it != pending_datagrams_.end()) {
      for (const auto& dgram : it->second) {
        transmit({ { sender_eth, ethernet_address_, EthernetHeader::TYPE_IPv4 }, serialize(dgram) });
      }
      pending_datagrams_.erase(it);
      pending_datagram_timers_.erase(sender_ip);
    }
  }


}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  for (auto it = arp_cache_.begin(); it != arp_cache_.end(); ) {
    it->second.timer += ms_since_last_tick;
    if (it->second.timer >= ARP_ENTRY_TTL_ms) 
      it = arp_cache_.erase(it);
    else 
      ++it;
  }

  for (auto it = pending_datagram_timers_.begin(); it != pending_datagram_timers_.end(); ) {
    it->second += ms_since_last_tick;
    if (it->second >= ARP_REQUEST_PERIOD_ms) {
      pending_datagrams_.erase(it->first);
      it = pending_datagram_timers_.erase(it);
    } else {
      ++it;
    }
  }
}
