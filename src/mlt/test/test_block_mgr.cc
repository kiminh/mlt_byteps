#include "block_mgr.h"
#include "bitmap_block_mgr.h"
#include "tree_block_mgr.h"

#include "benchmark/benchmark.h"

#include "random_generator.h"
#include "prism/logging.h"
#include "dmlc/memory_io.h"
#include <memory>
#include <random>

const size_t kSizeStage1 = 500'000;
const size_t kSizeStage2 = 600'000;
const size_t kMaxN = 600'000;

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

static void BM_BitmapBlockMgrCreation(benchmark::State& state) {
  for (auto _ : state) {
    auto block_mgr = std::make_unique<BitmapBlockMgr>(state.range(0));
  }
}

// static void BM_BitmapBlockMgrResize(benchmark::State& state) {
//   auto block_mgr = std::make_unique<BitmapBlockMgr>(kSizeStage1);
//   for (auto _ : state) {
//     block_mgr->Resize(state.range(0));
//   }
// }

static void BM_BitmapBlockMgrCheckAndTake(benchmark::State& state) {
  size_t size = state.range(0);
  auto block_mgr = std::make_unique<BitmapBlockMgr>(size);
  RandomGenerator gen;
  for (auto _ : state) {
    uint32_t seq = gen.Uniform(0u, static_cast<uint32_t>(size) - 1);
    if (!block_mgr->Check(seq)) block_mgr->Take(seq);
  }
}

static void BM_BitmapBlockMgrSerializeToBuffer(benchmark::State& state) {
  size_t size = state.range(0);
  auto block_mgr = std::make_unique<BitmapBlockMgr>(size);
  RandomGenerator gen;
  for (size_t i = 0; i < size; i++) {
    uint32_t seq = gen.Uniform(0u, static_cast<uint32_t>(size) - 1);
    if (seq < 10) {
      if (!block_mgr->Check(i)) block_mgr->Take(i);
    }
  }
  LOG(INFO) << "num_blocks: " << block_mgr->ByteSize() / sizeof(Block);
  for (auto _ : state) {
    size_t buf_size = block_mgr->ByteSize();
    char* block_buf = new char[buf_size];
    block_mgr->SerializeToBuffer(block_buf, buf_size);
    delete block_buf;
  }
}

static void BM_TreeBlockMgrCreation(benchmark::State& state) {
  for (auto _ : state) {
    auto block_mgr = std::make_unique<TreeBlockMgr>(state.range(0));
  }
}

// static void BM_TreeBlockMgrResize(benchmark::State& state) {
//   auto block_mgr = std::make_unique<TreeBlockMgr>(kSizeStage1);
//   for (auto _ : state) {
//     block_mgr->Resize(state.range(0));
//   }
// }

static void BM_TreeBlockMgrCheckAndTake(benchmark::State& state) {
  size_t size = state.range(0);
  auto block_mgr = std::make_unique<TreeBlockMgr>(size);
  RandomGenerator gen;
  for (auto _ : state) {
    uint32_t seq = gen.Uniform(0u, static_cast<uint32_t>(size) - 1);
    if (!block_mgr->Check(seq)) block_mgr->Take(seq);
  }
}

static void BM_TreeBlockMgrSerializeToBuffer(benchmark::State& state) {
  size_t size = state.range(0);
  auto block_mgr = std::make_unique<TreeBlockMgr>(size);
  RandomGenerator gen;
  for (size_t i = 0; i < size; i++) {
    uint32_t seq = gen.Uniform(0u, static_cast<uint32_t>(size) - 1);
    if (seq < 10) {
      if (!block_mgr->Check(i)) block_mgr->Take(i);
    }
  }
  LOG(INFO) << "num_blocks: " << block_mgr->ByteSize() / sizeof(Block);
  for (auto _ : state) {
    size_t buf_size = block_mgr->ByteSize();
    char* block_buf = new char[buf_size];
    block_mgr->SerializeToBuffer(block_buf, buf_size);
    delete block_buf;
  }
}

#define BENCHMARK_GROUP(name) \
BENCHMARK(BM_##name##Creation)->RangeMultiplier(8)->Range(8, kMaxN); \
BENCHMARK(BM_##name##CheckAndTake)->RangeMultiplier(8)->Range(8, kMaxN); \
BENCHMARK(BM_##name##SerializeToBuffer)->RangeMultiplier(8)->Range(8, kMaxN);
// BENCHMARK(BM_##name##Resize)->RangeMultiplier(8)->Range(8, kMaxN);

BENCHMARK_GROUP(BitmapBlockMgr)

BENCHMARK_GROUP(TreeBlockMgr)

BENCHMARK_MAIN();