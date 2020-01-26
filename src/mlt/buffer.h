#ifndef BUFFER_H_
#define BUFFER_H_

#include "prism/logging.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/// for small buffers, typically smaller than megabytes, malloc and free is as
/// fast as several nanoseconds, so to avoid the mutex and thread
/// synchronization overhead, we do not write a buffer pool from scratch

/**
 * \brief Buffer for socket sending and receiving
 *
 * It handles the sending of the data on this buffer by moving the pointer. It
 * may not own the memory.
 */
class Buffer {
 public:
  // default constructor
  Buffer()
      : ptr_{nullptr},
        size_{0},
        msg_length_{0},
        bytes_handled_{0},
        is_owner_{false} {}

  Buffer(size_t size)
      : size_{size}, msg_length_{0}, bytes_handled_{0}, is_owner_{true} {
    ptr_ = static_cast<char*>(malloc(size_));
    CHECK(ptr_) << "malloc failed";
  }

  Buffer(const void* ptr, size_t size, uint32_t msg_length)
      : Buffer(const_cast<void*>(ptr), size, msg_length) {}

  Buffer(void* ptr, size_t size, uint32_t msg_length)
      : ptr_{static_cast<char*>(ptr)},
        size_{size},
        msg_length_{msg_length},
        bytes_handled_{0},
        is_owner_{false} {}

  Buffer(Buffer* buffer)
      : Buffer(buffer->ptr_, buffer->size_, buffer->msg_length_) {}

  ~Buffer() {
    if (is_owner_ && ptr_) {
      free(ptr_);
      ptr_ = nullptr;
    }
  }

  inline char* GetRemainBuffer() { return ptr_ + bytes_handled_; }

  inline uint32_t GetRemainSize() { return msg_length_ - bytes_handled_; }

  inline bool IsClear() { return msg_length_ == bytes_handled_; }

  inline void MarkHandled(size_t nbytes) { bytes_handled_ += nbytes; }

  inline void set_msg_length(uint32_t msg_length) { msg_length_ = msg_length; }

  inline uint32_t msg_length() const { return msg_length_; }

  inline const char* ptr() const { return ptr_; }

  inline char* ptr() { return ptr_; }

  inline size_t size() { return size_; }

  void CopyFrom(Buffer* other) {
    CHECK(size_ >= other->msg_length_);
    memcpy(ptr_, other->ptr_, other->msg_length_);
    msg_length_ = other->msg_length_;
  }

 private:
  char* ptr_;
  // buffer size
  size_t size_;
  // valid data length
  uint32_t msg_length_;
  // bytes already handled
  uint32_t bytes_handled_;
  // whether to own this memory
  bool is_owner_;
};

#endif  // BUFFER_H_