#ifndef PRIO_FUNC_H_
#define PRIO_FUNC_H_

#include "grad_packet.h"
#include "mlt_global.h"
#include "random_generator.h"

#include <functional>
#include <random>
#include <numeric>

// input: GradPacket, output: tos
// using PktPrioFunc = std::function<int(GradPacket)>;

struct PktPrioFunc {
  virtual ~PktPrioFunc() {}
  virtual int operator()(GradPacket& pkt) = 0;
};

struct DefaultPktPrioFunc : public PktPrioFunc {
  std::string name;  // tensor name
  int layer;
  double theta;
  int num_samples;
  std::vector<size_t> offs;
  RandomGenerator gen;

  DefaultPktPrioFunc(const std::string& name, int layer, double theta)
      : name{name}, layer{layer}, theta{theta} {
    num_samples = prism::GetEnvOrDefault<int>("MLT_PRIO_FUNC_SAMPLE", 10);
    offs.resize(num_samples);
    std::iota(offs.begin(), offs.end(), 0);
  }

  int operator()(GradPacket& pkt) override {
    // current, we only support float32
    float* grad_ptr = GetGradientPtr<float>(pkt);
    size_t num_grads = GetNumGradients<float>(pkt);
    (void)grad_ptr; (void)num_grads;
    return gen.Uniform(0, 7) * 8 << 2 | gen.Uniform(0, 1);

    // /// sampling
    // //offs = RandomSortedOffsets(num_samples, num_grads);
    // float sum =
    //     std::accumulate(offs.begin(), offs.end(), 0,
    //                     [grad_ptr, num_grads](float acc, size_t o) {
    //                       return o >= num_grads ? acc : acc + abs(grad_ptr[o]);
    //                     });

    // int dscp = Encode(layer);
    // int ecn = sum > theta ? 1 : 0;
    // return dscp << 2 | ecn;
  }

  int Encode(int layer) {
    int num_layers = MLTGlobal::Get()->NumLayers();
    int num_queues = MLTGlobal::Get()->NumQueues();
    return layer * num_queues / num_layers;
  }

  std::vector<size_t> RandomSortedOffsets(int num_samples, int num_grads) {
    std::mt19937_64 rng;  /// this is much faster than mt19937
    std::vector<size_t> offs;
    std::generate_n(std::back_inserter(offs), num_samples,
                    [&rng, num_grads]() { return rng() % num_grads; });
    std::sort(offs.begin(), offs.end());
    return offs;
  }
};

#endif  // PRIO_FUNC_H_