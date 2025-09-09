#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

void Writer::push( string data )
{
  // 如果流已关闭，不进行任何操作
  if (is_close_) return;

  // 计算能够推入的最大字节数
  uint64_t push_size = std::min(data.length(), available_capacity());

  // 只追加允许容量内的数据
  if (push_size > 0) {
    try {
      buffer_.append(data.substr(0, push_size));
      // 更新已推入的字节计数
      pushcnt_ += push_size;
    } catch (...) {
      // 如果发生异常（如内存不足），设置错误状态
      set_error();
    }
  }
  
  return;
}

void Writer::close()
{
  is_close_ = true;
}

bool Writer::is_closed() const
{
  return is_close_;
}

uint64_t Writer::available_capacity() const
{
  return (capacity_ - buffer_.size());
}

uint64_t Writer::bytes_pushed() const
{
  return pushcnt_;
}

string_view Reader::peek() const
{
  // 如果buffer为空，返回空string_view
  if (buffer_.empty()) {
    return {};
  }

  // 返回buffer的string_view，这是安全的因为buffer_在这个对象的生命周期内有效
  return std::string_view(buffer_);
}

void Reader::pop( uint64_t len )
{
  // 防止pop超过buffer的大小
  uint64_t pop_size = std::min(len, buffer_.size());

  // 从buffer中移除已读的数据
  buffer_ = buffer_.substr(pop_size);

  // 更新已弹出的字节计数
  popcnt_ += pop_size;

  return;
}

bool Reader::is_finished() const
{
  return is_close_ && pushcnt_ == popcnt_;
}

uint64_t Reader::bytes_buffered() const
{
  return buffer_.size();
}

uint64_t Reader::bytes_popped() const
{
  return popcnt_;
}

