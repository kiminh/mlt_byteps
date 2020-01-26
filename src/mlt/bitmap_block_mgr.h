#ifndef BITMAP_BLOCK_MGR_H_
#define BITMAP_BLOCK_MGR_H_

#include "block_mgr.h"
#include "prism/logging.h"
#include "dmlc/memory_io.h"

#include <bitset>
#include <vector>

template <typename T>
class Bitmap {
 public:
  static const size_t type_size = sizeof(T) * 8;

  Bitmap(size_t size) : size_{size}, vec_(RoundUp(size, type_size)) {}

  inline void Resize(size_t size) {
    size_ = size;
    vec_.resize(RoundUp(size_, type_size));
  }

  inline size_t Size() const { return size_; }

  // inline const T operator[](size_t pos) const {
  //   size_t offset = (pos & (type_size - 1));
  //   T val = vec_[pos / type_size];
  //   return (val >> offset) & 1;
  // }

  inline const T Test(size_t pos) const {
    size_t offset = pos & (type_size - 1);
    T val = vec_[pos / type_size];
    return (val >> offset) & 1;
  }

  inline void Set(size_t pos, T value = 1) {
    size_t offset = pos & (type_size - 1);
    T& val = vec_[pos / type_size];
    if (value) {
      val |= static_cast<T>(1) << offset;
    } else {
      val &= ~(static_cast<T>(1) << offset);
    }
  }

  inline void Reset(size_t pos) {
    size_t offset = pos & (type_size - 1);
    T& val = vec_[pos / type_size];
    val &= ~(static_cast<T>(1) << offset);
  }

  inline void Flip(size_t pos) {
    Set(pos, Test(pos) ^ 1);
  }

  inline const T UnderlayValue(size_t pos) const { return vec_[pos]; }

 private:
  static size_t RoundUp(size_t x, size_t d) { return (x + d - 1) / d; }

  size_t size_;
  std::vector<T> vec_;
};

class BitmapBlockMgr final : public BlockMgr {
 public:
  BitmapBlockMgr(size_t size)
      : BlockMgr{size}, used_{0}, segments_{1}, bitmap_{size} {}

  virtual ~BitmapBlockMgr() {}

  virtual void Resize(size_t size) override {
    CHECK(size > size_);
    bitmap_.Resize(size);
    size_ = size;
    if (bitmap_.Test(size_ - 1)) segments_++;
  }

  virtual bool Check(uint32_t seq) override {
    CHECK(0 <= seq && seq < size_) << "seq: " << seq;
    return bitmap_.Test(seq);
  }

  virtual void Take(uint32_t seq) override {
    CHECK(0 <= seq && seq < size_) << "seq: " << seq;
    if (!bitmap_.Test(seq)) {
      used_++;
      segments_++;
      if (seq == 0 || bitmap_.Test(seq - 1)) segments_--;
      if (seq + 1 == static_cast<uint32_t>(size_) || bitmap_.Test(seq + 1))
        segments_--;
      bitmap_.Set(seq);
    }
  }

  virtual size_t FreeLength() override {
    return size_ - used_;
  }

  virtual size_t ByteSize() override {
    return segments_ * sizeof(Block);
  }

  virtual void SerializeToBuffer(void* buf, size_t buf_size) override {
    std::unique_ptr<dmlc::SeekStream> strm =
        std::make_unique<dmlc::MemoryFixedSizeStream>(buf, buf_size);
    const uint32_t step_size = decltype(bitmap_)::type_size;

    uint32_t first = 0;
    uint32_t last = 0;

    long mask;
    for (uint32_t i = 0; i < size_; i += step_size) {
      // long seg = *reinterpret_cast<long*>(&bitmap_[i]);
      long seg = bitmap_.UnderlayValue(i / step_size);
      while (seg != 0 && first < i + step_size) {
        // DLOG(DEBUG) << "seg: " << std::bitset<64>(seg).to_string();

        int trailing_zeros = LastBitNotZero(seg);
        last = i + trailing_zeros;

        if (first < last) strm->Write(Block(first, last));
        // DLOG(INFO) << first << " " << last;
        // mask = 0x8000'0000'0000'0000l >> trailing_zeros << 1;
        mask = ~0l << trailing_zeros;
        first = i + LastBitNotZero(~(seg | ~mask));
        // mask = 0x8000'0000'0000'0000l >> (first - i) << 1;
        mask = ~0l << (first - i);
        seg &= mask;
      }
    }
    if (first < size_) strm->Write(Block(first, size_));
    CHECK_EQ(strm->Tell(), ByteSize());
  }

 private:
  static inline int FirstBitNotZero(long n) { return __builtin_clzl(n); }
  static inline int LastBitNotZero(long n) { return __builtin_ctzl(n); }

  uint32_t used_;
  uint32_t segments_;
  // std::vector<bool> bitmap_;
  Bitmap<long> bitmap_;
};

#endif  // BITMAP_BLOCK_MGR_H_