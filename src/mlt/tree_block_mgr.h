#ifndef TREE_BLOCK_MGR_H_
#define TREE_BLOCK_MGR_H_

#include "block_mgr.h"
#include "prism/logging.h"
#include "dmlc/memory_io.h"

#include <set>

class FreelistBlockMgr : public BlockMgr {
};

class TreeBlockMgr : public BlockMgr {
 public:
  TreeBlockMgr(size_t size) : BlockMgr{size}, used_{0} {
    tree_.emplace(0, size);
  }

  virtual ~TreeBlockMgr() {}

  virtual void Resize(size_t size) override {
    CHECK(size > size_);
    if (tree_.empty()) {
      tree_.emplace(size_, size);
      size_ = size;
      return;
    }

    auto it = tree_.rbegin();
    if (it->last < size_) {
      tree_.emplace(size_, size);
    } else {
      it->last = size;
    }
    size_ = size;
  }

  virtual bool Check(uint32_t seq) override {
    if (tree_.empty()) return true;

    Block key(seq, seq + 1);
    auto it = tree_.upper_bound(key);
    if (it == tree_.begin()) return true;
    std::advance(it, -1);
    return !(it->first <= seq && seq < it->last);
  }

  virtual void Take(uint32_t seq) override {
    CHECK(!tree_.empty());

    Block key(seq, seq + 1);
    auto it = tree_.upper_bound(key);
    CHECK(it != tree_.begin());
    std::advance(it, -1);

    CHECK(it->first <= seq && seq < it->last)
        << it->first << " " << seq << " " << it->last;
  
    used_++;

    if (it->first == seq) {
      it->first++;
    } else if (it->last == seq + 1) {
      it->last--;
    } else {
      tree_.emplace(seq + 1, it->last);
      it->last = seq;
    }
    if (it->first == it->last) tree_.erase(it);
    /// to ensure all operation goes success
    /// if (tree_.empty()) tree_.emplace(0, 0);
  }

  virtual size_t FreeLength() override {
    return size_ - used_;
  }

  virtual size_t ByteSize() override {
    return tree_.size() * sizeof(Block);
  }

  virtual void SerializeToBuffer(void* buf, size_t buf_size) override {
    std::unique_ptr<dmlc::SeekStream> strm =
        std::make_unique<dmlc::MemoryFixedSizeStream>(buf, buf_size);
    for (const Block& block : tree_) {
      // DLOG(INFO) << block.first << " " << block.last;
      strm->Write(block);
    }
    CHECK_EQ(strm->Tell(), ByteSize());
  }

 private:
  uint32_t used_;
  std::set<Block> tree_;
};


#endif  // TREE_BLOCK_MGR_H_