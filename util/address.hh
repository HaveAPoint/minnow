#pragma once

#include <cstddef>
#include <cstdint>
#include <netdb.h>
#include <string>
#include <sys/socket.h>
#include <utility>

//! IPv4 地址和 DNS 操作的包装器。
class Address
{
public:
  //! \brief [sockaddr_storage](@ref man7::socket) 的包装器。
  //! \details 一个 `sockaddr_storage` 有足够空间存储任何 socket 地址（IPv4 或 IPv6）。
  class Raw
  {
  public:
    sockaddr_storage storage {}; //!< 被包装的结构体本身。
    // NOLINTBEGIN (*-explicit-*)
    operator sockaddr*();
    operator const sockaddr*() const;
    // NOLINTEND (*-explicit-*)
  };

private:
  socklen_t _size; //!< 被包装地址的大小。
  Raw _address {}; //!< 包含地址的被包装 [sockaddr_storage](@ref man7::socket)。

  //! 从 ip/主机、服务/端口和解析器提示构造。
  Address( const std::string& node, const std::string& service, const addrinfo& hints );

public:
  //! 通过解析主机名和服务名构造。
  Address( const std::string& hostname, const std::string& service );

  //! 从点分十进制字符串（"18.243.0.1"）和数字端口构造。
  explicit Address( const std::string& ip, std::uint16_t port = 0 );

  //! 从 [sockaddr *](@ref man7::socket) 构造。
  Address( const sockaddr* addr, std::size_t size );

  //! 相等性比较。
  bool operator==( const Address& other ) const;
  bool operator!=( const Address& other ) const { return not operator==( other ); }

  //! \name 转换
  //!@{

  //! 点分十进制 IP 地址字符串（"18.243.0.1"）和数字端口。
  std::pair<std::string, uint16_t> ip_port() const;
  //! 点分十进制 IP 地址字符串（"18.243.0.1"）。
  std::string ip() const { return ip_port().first; }
  //! 数字端口（主机字节序）。
  uint16_t port() const { return ip_port().second; }
  //! 作为整数的数字 IP 地址（即，在[主机字节序](\ref man3::byteorder)中）。
  uint32_t ipv4_numeric() const;
  //! 从 32 位原始数字 IP 地址创建 Address
  static Address from_ipv4_numeric( uint32_t ip_address );
  //! 人类可读字符串，例如 "8.8.8.8:53"。
  std::string to_string() const;
  //!@}

  //! \name 底层操作
  //!@{

  //! 底层地址存储的大小。
  socklen_t size() const { return _size; }
  //! 指向底层 socket 地址存储的常量指针。
  const sockaddr* raw() const { return static_cast<const sockaddr*>( _address ); }
  //! 安全转换为底层 sockaddr 类型
  template<typename sockaddr_type>
  const sockaddr_type* as() const;

  //!@}
};

// 中文翻译说明：
// 这个文件定义了 Address 类，它是 IPv4 地址和 DNS 操作的包装器。
// 主要功能包括：
// 1. 从字符串或 sockaddr 构造地址对象
// 2. 提供地址的各种表示形式（字符串、数字等）
// 3. 支持地址比较和转换操作
// 4. 封装底层 sockaddr_storage，提供类型安全的访问
// 
// 关键组件：
// - Raw 嵌套类：包装 sockaddr_storage，提供类型转换
// - 构造函数：支持多种地址创建方式
// - 转换方法：ip_port(), ip(), port(), ipv4_numeric() 等
// -
