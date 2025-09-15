#include "wrapping_integers.hh"
#include "debug.hh"
#include <cstdlib>

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32(static_cast<uint32_t>(n + zero_point.raw_value_));
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // 计算相对偏移量
  uint32_t offset = raw_value_ - zero_point.raw_value_;

  // 计算检查点所在的"时代"
  uint64_t checkpoint_era = checkpoint >> 32;
  
  // 考虑三个相邻时代的候选值
  uint64_t candidate1 = (checkpoint_era << 32) + offset;          // 同一时代
  uint64_t candidate2 = ((checkpoint_era + 1) << 32) + offset;    // 下一时代
  uint64_t candidate3 = checkpoint_era > 0 ?                      // 上一时代（如果可能）
                        ((checkpoint_era - 1) << 32) + offset : candidate1;

  // 计算三个候选值与检查点的距离
  uint64_t distance1 = checkpoint > candidate1 ? checkpoint - candidate1 : candidate1 - checkpoint;
  uint64_t distance2 = checkpoint > candidate2 ? checkpoint - candidate2 : candidate2 - checkpoint;
  uint64_t distance3 = checkpoint > candidate3 ? checkpoint - candidate3 : candidate3 - checkpoint;

  // 返回距离最近的候选值
  if (distance1 <= distance2 && distance1 <= distance3) {
    return candidate1;
  } else if (distance2 <= distance1 && distance2 <= distance3) {
    return candidate2;
  } else {
    return candidate3;
  }
}
