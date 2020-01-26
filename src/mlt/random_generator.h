#ifndef RANDOM_GENERATOR_H_
#define RANDOM_GENERATOR_H_

#include <random>

class RandomGenerator {
 public:
  RandomGenerator() {
    rd_ = new std::random_device;
    gen_.seed(rd_->operator()());
  }
  ~RandomGenerator() { delete rd_; }

  template <typename DType>
  inline DType Uniform() {
    typedef typename std::conditional<
        std::is_integral<DType>::value, std::uniform_int_distribution<DType>,
        std::uniform_real_distribution<DType>>::type GType;
    GType dist_uniform;
    return dist_uniform(gen_);
  }

  template <typename DType>
  inline DType Uniform(DType a, DType b) {
    typedef typename std::conditional<
        std::is_integral<DType>::value, std::uniform_int_distribution<DType>,
        std::uniform_real_distribution<DType>>::type GType;
    GType dist_uniform(a, b);
    return dist_uniform(gen_);
  }

 private:
  std::random_device* rd_;
  std::mt19937_64 gen_;
};

#endif  // RANDOM_GENERATOR_H_
