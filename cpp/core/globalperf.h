#ifndef CORE_GLOBALPERF_H_
#define CORE_GLOBALPERF_H_

#include <string>

namespace GlobalPerfProfile {
  void setEnabled(bool enabled);
  bool isEnabled();

  void clear();

  void beginBenchmarkSample();
  void endBenchmarkSample();

  void recordSearchLoop(double processMilliseconds, double waitMilliseconds);
  void noteQueueLength(int queueLength);
  void changeInferenceThreadActiveCount(int delta);
  void recordInferencePhases(
    double preprocessMs,
    double h2dMs,
    double waitGpuMs,
    double d2hMs,
    double postprocessMs,
    int batchSize
  );
  void recordInferenceLaunchInterval(double launchIntervalMs);

  std::string makeReport();
}

#endif  // CORE_GLOBALPERF_H_
