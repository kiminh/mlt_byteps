#ifndef BLOCK_MGR_H_
#define BLOCK_MGR_H_
#include <cstdio>
#include <cstdint>
#include <mutex>

/// [first_seq, last_seq)
struct Block {
  mutable uint32_t first;
  mutable uint32_t last;

  Block() = default;
  Block(uint32_t l, uint32_t r) : first{l}, last{r} {}
  inline bool operator<(const Block& rhs) const {
    return first < rhs.first;
  }
};

class BlockMgr {
 public:
  BlockMgr(size_t size) : size_{static_cast<uint32_t>(size)} {}

  virtual ~BlockMgr() {}

  inline const uint32_t Size() const { return size_; }

  virtual void Resize(size_t size) = 0;

  /**
   * \brief check if seq has taken
   * 
   * \param seq the seqeunce number
   * \return true if this sequence number has been taken/allocated.
   */
  virtual bool Check(uint32_t seq) = 0;

  virtual void Take(uint32_t seq) = 0;

  virtual size_t FreeLength() = 0;

  virtual size_t ByteSize() = 0;

  virtual void SerializeToBuffer(void* buf, size_t buf_size) = 0;

  inline void Lock() const { mu_.lock(); }

  inline void Unlock() const { mu_.unlock(); }

  inline std::mutex& mtx() { return mu_; }

 protected:
  uint32_t size_;
  mutable std::mutex mu_;
};

#endif  // BLOCK_MGR_H_