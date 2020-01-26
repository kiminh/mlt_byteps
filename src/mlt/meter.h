#ifndef METER_H_
#define METER_H_
#include <chrono>
#include <cstdint>

using milliseconds = std::chrono::milliseconds;
using microseconds = std::chrono::microseconds;
using Clock = std::chrono::high_resolution_clock;

struct Meter {
  Meter() = default;

  Meter(uint64_t interval_ms, const char* name = "meter",
        const int _sample = 0xf)
      : name{name},
        cnt{0},
        bytes{0},
        msgs{0},
        tp{Clock::now()} {
    interval = milliseconds(interval_ms);
    sample = RoundUpPower2(_sample + 1) - 1;
    CHECK_EQ(sample + 1, Lowbit(sample + 1)) << "sample must be 2^n-1";
  }

  inline void Add(size_t add) {
    bytes += add;
    msgs += 1;
    if ((++cnt & sample) == sample) {
      auto now = Clock::now();
      if ((now - tp) >= interval) {
        std::chrono::duration<double> dura = now - tp;
        printf("[%s] Speed: %.6f MB/s %.0f msg/s\n", name,
               bytes / dura.count() / 1000000, msgs / dura.count());
        fflush(stdout);
        bytes = 0;
        msgs = 0;
        tp = now;
      }
    }
  }

  inline long Lowbit(long x) { return x & -x; }

  inline long RoundUpPower2(long x) {
    while (x != Lowbit(x)) x += Lowbit(x);
    return x;
  }

  const char* name;
  int sample;
  int cnt;
  size_t bytes;
  size_t msgs;
  Clock::time_point tp;
  milliseconds interval;
};

class RateMeter {
 public:
  RateMeter(uint64_t interval_us, const int sample = 0x0)
      : sample_{sample}, scnt_{0}, bytes_{0}, msgs_{0}, tp_{Clock::now()} {
    interval_ = microseconds(interval_us);
  }

  inline void Update(size_t add) {
    bytes_ += add;
    msgs_ += 1;
    scnt_++;
  }

  inline bool Elapsed() {
    if (++scnt_ >= sample_) {
      scnt_ = 0;
      auto now = Clock::now();
      return (now - tp_) >= interval_;
    }
    return false;
  }

  inline double TryBytesPerSecond(size_t extra) const {
    auto dura = Clock::now() - tp_;
    return 1e9 * (bytes_ + extra) / dura.count();
  }

  inline double GetBytesPerSecond() const {
    auto dura = Clock::now() - tp_;
    return 1e9 * bytes_ / dura.count();
  }

  inline double GetMessagePerSecond() const {
    auto dura = Clock::now() - tp_;
    return 1e9 * msgs_ / dura.count();
  }

  inline void Clear() {
    bytes_ = 0;
    msgs_ = 0;
    tp_ = Clock::now();
  }

 private:
  int sample_;
  /// sample counter
  int scnt_;
  size_t bytes_;
  size_t msgs_;
  Clock::time_point tp_;
  microseconds interval_;
};

#endif  // METER_H_
