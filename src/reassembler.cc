#include "reassembler.hh"
#include "debug.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  //data.empty, handle eof_ and eof_index_, check if next_index_ == eof_index_
  //仅仅 is_last_substring == true 并不能保证流可以关闭。TCP流的结束条件是所有数据都已经被正确组装并写入 ByteStream，
  //也就是 next_index_ == eof_index_。
  //如果只判断 is_last_substring，可能还有未组装的数据片段（比如乱序、丢包等），此时不能关闭流，否则会丢失数据。

  if (is_last_substring) {
    eof_ = true;
    eof_index_ = first_index + data.size();

    if (data.empty() && next_index_ == eof_index_) {
      output_.writer().close();
      return;
    }
  }

  if (data.empty()) return;

  if (first_index + data.size() <= next_index_) {
    if (eof_ && next_index_ == eof_index_) {
      output_.writer().close();
    }
    return;
  }

  //bytestream capacity check
  uint64_t available_capacity = output_.writer().available_capacity();

  uint64_t acceptable_end = next_index_ + available_capacity;

  if (first_index >= acceptable_end) {
    return;
  }

  uint64_t actual_start = max(first_index, next_index_);
  uint64_t actual_end = min(first_index + data.size(), acceptable_end);

  uint64_t offset = actual_start - first_index;
  uint64_t length = actual_end - actual_start;

  string usable_data = data.substr(offset, length);
  uint64_t usable_index = actual_start;
  
  // if possible, input stream
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
    //substring storage and merge
    auto it = unassembled_.lower_bound(usable_index);

    if (it != unassembled_.begin()) {
      auto prev = std::prev(it);
      uint64_t prev_end = prev->first + prev->second.size();

      if (prev_end >= usable_index) {
        if (prev_end < usable_index + usable_data.size()) {
          uint64_t extend_len = usable_index + usable_data.size() - prev_end;
          prev->second += usable_data.substr(usable_data.size() - extend_len);
        }

        //if overloaded by substring before
        if (prev_end >= usable_index + usable_data.size()) {
          if (eof_ && next_index_ == eof_index_) {
            output_.writer().close();
          }
          return;
        }

        //update substring need to implement
        usable_index = prev->first;
        usable_data = prev->second;
        unassembled_.erase(prev);
      }
    }

    //check and merge substring below
    while (it != unassembled_.end() && it->first <= usable_index + usable_data.size()) {
      uint64_t it_end = it->first + it->second.size();

      if (it_end > usable_index + usable_data.size()) {
        uint64_t extend_len = it_end - (usable_index + usable_data.size());
        usable_data += it->second.substr(it->second.size() - extend_len);
      }

      auto to_erase = it;
      ++it;
      unassembled_.erase(to_erase);
    }

    //storage substring merged
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

