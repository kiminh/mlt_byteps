#include "block_mgr.h"

// #include "prism/logging.h"
// #include "dmlc/memory_io.h"
// #include <cstdio>
// #include <cstring>
// #include <memory>
// #include <bitset>

// #include <random>

// class FreelistBlockMgr : public BlockMgr {
// };

// class TreeBlockMgr : public BlockMgr {
// };

// const size_t kSizeStage1 = 500'000;
// const size_t kSizeStage2 = 600'000;

// int main() {
//   std::random_device rd;
//   std::mt19937_64 gen(rd());

//   auto block_mgr = std::make_unique<BitmapBlockMgr>(kSizeStage1);

//   for (int i = 0; i < 7000; i++) {
//     int block_num = gen() % kSizeStage1;
//     block_mgr->Take(block_num);
//   }

//   block_mgr->Resize(kSizeStage2);

//   for (int i = 0; i < 7000; i++) {
//     int block_num = gen() % kSizeStage2;
//     block_mgr->Take(block_num);
//   }

//   size_t buf_size = block_mgr->ByteSize();
//   char* block_buf = new char[buf_size];
//   block_mgr->SerializeToBuffer(block_buf, buf_size);
//   return 0;
// }
