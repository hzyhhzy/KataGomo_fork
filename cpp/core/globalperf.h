#ifndef CORE_GLOBALPERF_H_
#define CORE_GLOBALPERF_H_

#include <cstdint>
#include <string>

namespace GlobalPerfProfile {
  enum class CudaStreamType {
    H2D,
    Infer,
    D2H
  };

  void setEnabled(bool enabled);
  bool isEnabled();

  void clear();
  void configureInferenceResources(bool singleSchedulerMode, int numInferenceSlots);

  void beginBenchmarkSample();
  void endBenchmarkSample();

  void recordSearchLoop(double processMilliseconds, double waitMilliseconds);
  void recordSchedulerBusySpan(int64_t startNs, int64_t endNs);
  void recordCudaStreamTask(
    CudaStreamType type,
    int streamIdx,
    int64_t cpuSubmitEndNs,
    double taskDurationMs
  );

  std::string makeReport();
}

#endif  // CORE_GLOBALPERF_H_
