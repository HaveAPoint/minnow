#include "reassembler.hh"
#include "debug.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  //data.empty, handle eof_ and eof_index_, check if next_index_ == eof_index_
  //仅仅 is_last_substring == true 并不能保证流可以关闭。TCP流的结束条件是所有数据都已经被正确组装并写入ByteStream，
  //也就是 next_index_ == eof_index_。
  //如果只判断 is_last_substring，可能还有未组装的数据片段（比如乱序、丢包等），此时不能关闭流，否则会丢失数据。

  if (is_last_substring) {
    eof_ = true;
    eof_index_ = first_index + data.size();

    if (next_index_ == eof_index_) {
      output_.writer().close();
    }
  }

  if (data.empty()) return;

  //每次写入数据后、每次插入新片段后,判断 eof_ && next_index_ == eof_index_
  if (first_index + data.size() <= next_index_) {
    if (eof_ && next_index_ == eof_index_) {
      output_.writer().close();
    }
    return;
  }

  //bytestream capacity check
  uint64_t available_capacity = output_.writer().available_capacity();

  uint64_t acceptable_end = next_index_ + available_capacity;

  //all out of acceptable
  if (first_index >= acceptable_end) {
    return;
  }

  //用目前的窗口大小来切割数据
  uint64_t actual_start = max(first_index, next_index_);
  uint64_t actual_end = min(first_index + data.size(), acceptable_end);

  uint64_t offset = actual_start - first_index;
  uint64_t length = actual_end - actual_start;

  string usable_data = data.substr(offset, length);
  uint64_t usable_index = actual_start;
  
  // merged into output_ instantly
  if (usable_index == next_index_) {
    output_.writer().push(usable_data);
    next_index_ += usable_data.size();

    while (!unassembled_.empty()) {
      auto it = unassembled_.begin();
      if (it->first > next_index_) {
        break;
      }

      //overlap means bytes need to pass
      //substr(n) means ...
      uint64_t overlap = next_index_ - it->first;
      if (overlap < it->second.size()) {
        string new_data = it->second.substr(overlap);
        output_.writer().push(new_data);
        next_index_ += new_data.size();
      }

      unassembled_.erase(it);
    }
  } else if (usable_data.size() > 0){
    // 查找插入位置或合并点
    auto it = unassembled_.lower_bound(usable_index);

    // 尝试与前一个片段合并
    if (it != unassembled_.begin()) {
      auto prev = std::prev(it);  // 获取前一个片段
      uint64_t prev_end = prev->first + prev->second.size();  // 计算结束位置

      // 检查是否有重叠
      if (prev_end >= usable_index) {
        // 如果前一个片段部分覆盖当前片段
        if (prev_end < usable_index + usable_data.size()) {
          // 计算需要扩展的长度
          uint64_t extend_len = usable_index + usable_data.size() - prev_end;
          // 拼接未被覆盖的部分
          prev->second += usable_data.substr(usable_data.size() - extend_len);
        }

        // 如果前一个片段完全覆盖当前片段，不需要存储
        if (prev_end >= usable_index + usable_data.size()) {
          if (eof_ && next_index_ == eof_index_) {
            output_.writer().close();
          }
          return;
        }

        // 更新合并后的片段信息
        usable_index = prev->first;
        usable_data = prev->second;
        unassembled_.erase(prev);  // 删除旧的片段
      }
    }

    // 尝试与后续片段合并
    while (it != unassembled_.end() && it->first <= usable_index + usable_data.size()) {
      uint64_t it_end = it->first + it->second.size();

      // 如果后续片段有新数据
      if (it_end > usable_index + usable_data.size()) {
        // 计算需要扩展的长度
        uint64_t extend_len = it_end - (usable_index + usable_data.size());
        // 拼接新数据
        usable_data += it->second.substr(it->second.size() - extend_len);
      }

      // 删除已合并的片段
      auto to_erase = it;
      ++it;  // 先移动迭代器再删除
      unassembled_.erase(to_erase);
    }

    // 存储合并后的片段
    unassembled_[usable_index] = usable_data;
  }

  if (eof_ && next_index_ == eof_index_) {
    output_.writer().close();
  }
}

// How many bytes are stored in the Reassembler itself?
// This function is for testing only; don't add extra state to support it.
uint64_t Reassembler::count_bytes_pending() const
{
  uint64_t total = 0;
  for (const auto& [index, data] : unassembled_) {
    if (index + data.size() > next_index_) {
      uint64_t valid_start = max(index, next_index_);
      total += (index + data.size()) - valid_start;
    }
  }
  return total;
}

