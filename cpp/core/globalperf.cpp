#include "../core/globalperf.h"

#include "../core/global.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <sstream>
#include <vector>

using namespace std;

namespace {
  using Clock = std::chrono::steady_clock;
  static constexpr double BENCHMARK_SAMPLE_TRIM_SECONDS = 0.100;

  struct CountDurationSegment {
    double durationSeconds;
    int count;
  };

  struct TimedSearchLoop {
    double offsetSeconds;
    double processMs;
    double waitMs;
  };

  struct TimedInferencePhases {
    double offsetSeconds;
    double preprocessMs;
    double h2dMs;
    double waitGpuMs;
    double d2hMs;
    double postprocessMs;
    int batchSize;
  };

  struct TimedScalar {
    double offsetSeconds;
    double value;
  };

  struct GlobalPerfState {
    mutex stateMutex;

    vector<double> searchLoopProcessMs;
    vector<double> searchLoopWaitMs;

    vector<double> inferencePreprocessMs;
    vector<double> inferenceH2DMs;
    vector<double> inferenceWaitGpuMs;
    vector<double> inferenceD2HMs;
    vector<double> inferencePostprocessMs;
    vector<double> inferenceLaunchIntervalMs;

    vector<double> gpuWaitGpuMsByBatchSize;

    vector<double> queueLengthSeconds;
    int currentQueueLength = 0;

    vector<double> inferenceThreadActiveSeconds;
    int currentInferenceThreadActiveCount = 0;

    bool benchmarkSampleActive = false;
    Clock::time_point benchmarkSampleStart = Clock::now();
    Clock::time_point sampleQueueLastUpdate = Clock::now();
    Clock::time_point sampleInferenceThreadLastUpdate = Clock::now();
    vector<CountDurationSegment> sampleQueueSegments;
    vector<CountDurationSegment> sampleInferenceThreadSegments;
    vector<TimedSearchLoop> sampleSearchLoops;
    vector<TimedInferencePhases> sampleInferencePhases;
    vector<TimedScalar> sampleLaunchIntervals;
  };

  atomic<bool> g_enabled(false);
  GlobalPerfState g_state;

  static void ensureSize(vector<double>& values, size_t size) {
    if(values.size() < size)
      values.resize(size, 0.0);
  }

  static void resetStateLocked(GlobalPerfState& state) {
    state.searchLoopProcessMs.clear();
    state.searchLoopWaitMs.clear();
    state.inferencePreprocessMs.clear();
    state.inferenceH2DMs.clear();
    state.inferenceWaitGpuMs.clear();
    state.inferenceD2HMs.clear();
    state.inferencePostprocessMs.clear();
    state.inferenceLaunchIntervalMs.clear();
    state.gpuWaitGpuMsByBatchSize.clear();
    state.queueLengthSeconds.clear();
    state.currentQueueLength = 0;
    state.inferenceThreadActiveSeconds.clear();
    state.currentInferenceThreadActiveCount = 0;
    state.benchmarkSampleActive = false;
    state.benchmarkSampleStart = Clock::now();
    state.sampleQueueLastUpdate = Clock::now();
    state.sampleInferenceThreadLastUpdate = Clock::now();
    state.sampleQueueSegments.clear();
    state.sampleInferenceThreadSegments.clear();
    state.sampleSearchLoops.clear();
    state.sampleInferencePhases.clear();
    state.sampleLaunchIntervals.clear();
  }

  static double sampleOffsetSeconds(const GlobalPerfState& state, Clock::time_point now) {
    return std::chrono::duration<double>(now - state.benchmarkSampleStart).count();
  }

  static void updateQueueSampleSegmentsLocked(GlobalPerfState& state, Clock::time_point now) {
    if(!state.benchmarkSampleActive)
      return;
    double elapsedSeconds = std::chrono::duration<double>(now - state.sampleQueueLastUpdate).count();
    if(elapsedSeconds > 0) {
      state.sampleQueueSegments.push_back(CountDurationSegment{
        elapsedSeconds,
        state.currentQueueLength
      });
    }
    state.sampleQueueLastUpdate = now;
  }

  static void updateInferenceThreadSampleSegmentsLocked(GlobalPerfState& state, Clock::time_point now) {
    if(!state.benchmarkSampleActive)
      return;
    double elapsedSeconds = std::chrono::duration<double>(now - state.sampleInferenceThreadLastUpdate).count();
    if(elapsedSeconds > 0) {
      state.sampleInferenceThreadSegments.push_back(CountDurationSegment{
        elapsedSeconds,
        state.currentInferenceThreadActiveCount
      });
    }
    state.sampleInferenceThreadLastUpdate = now;
  }

  static void accumulateTrimmedCountSegments(
    const vector<CountDurationSegment>& segments,
    vector<double>& outBuckets,
    double trimmedStart,
    double trimmedEnd
  ) {
    double cursor = 0.0;
    for(const CountDurationSegment& seg: segments) {
      double segStart = cursor;
      double segEnd = cursor + seg.durationSeconds;
      double clippedStart = std::max(segStart, trimmedStart);
      double clippedEnd = std::min(segEnd, trimmedEnd);
      if(clippedEnd > clippedStart) {
        ensureSize(outBuckets, (size_t)seg.count + 1);
        outBuckets[seg.count] += (clippedEnd - clippedStart);
      }
      cursor = segEnd;
    }
  }

  static double percentile(vector<double> values, double p) {
    if(values.empty())
      return 0.0;
    sort(values.begin(), values.end());
    double idx = p * (values.size() - 1);
    size_t lo = (size_t)floor(idx);
    size_t hi = (size_t)ceil(idx);
    if(lo == hi)
      return values[lo];
    double frac = idx - lo;
    return values[lo] * (1.0 - frac) + values[hi] * frac;
  }

  static string formatPercentileLine(const string& label, const vector<double>& values) {
    if(values.empty())
      return "  " + label + ": n/a";
    return Global::strprintf(
      "  %s: P50=%.3f P95=%.3f P99=%.3f",
      label.c_str(),
      percentile(values, 0.50),
      percentile(values, 0.95),
      percentile(values, 0.99)
    );
  }

  static string formatShareHistogram(const string& label, const string& prefix, const vector<double>& buckets) {
    double total = 0.0;
    for(double value: buckets)
      total += value;
    if(total <= 0.0)
      return "  " + label + ": n/a";

    ostringstream out;
    out << "  " << label << ": ";
    bool first = true;
    int printed = 0;
    for(size_t i = 0; i < buckets.size(); i++) {
      if(buckets[i] <= 0.0)
        continue;
      if(!first) {
        if(printed % 8 == 0)
          out << "\n    ";
        else
          out << ", ";
      }
      out << prefix << i << "=" << Global::strprintf("%.2f%%", buckets[i] * 100.0 / total);
      first = false;
      printed++;
    }
    return out.str();
  }

  static string formatDecileLine(const string& label, const vector<double>& values) {
    if(values.empty())
      return "  " + label + ": n/a";
    ostringstream out;
    out << "  " << label << ": ";
    for(int decile = 1; decile <= 9; decile++) {
      if(decile > 1)
        out << " ";
      double p = (double)decile / 10.0;
      out << "D" << (decile * 10) << "=" << Global::strprintf("%.3f", percentile(values, p));
    }
    return out.str();
  }
}

void GlobalPerfProfile::setEnabled(bool enabled) {
  g_enabled.store(enabled, std::memory_order_release);
  if(!enabled) {
    lock_guard<mutex> lock(g_state.stateMutex);
    resetStateLocked(g_state);
  }
}

bool GlobalPerfProfile::isEnabled() {
  return g_enabled.load(std::memory_order_acquire);
}

void GlobalPerfProfile::clear() {
  if(!isEnabled())
    return;
  lock_guard<mutex> lock(g_state.stateMutex);
  resetStateLocked(g_state);
}

void GlobalPerfProfile::beginBenchmarkSample() {
  if(!isEnabled())
    return;
  Clock::time_point now = Clock::now();
  lock_guard<mutex> lock(g_state.stateMutex);
  g_state.benchmarkSampleActive = true;
  g_state.benchmarkSampleStart = now;
  g_state.sampleQueueLastUpdate = now;
  g_state.sampleInferenceThreadLastUpdate = now;
  g_state.sampleQueueSegments.clear();
  g_state.sampleInferenceThreadSegments.clear();
  g_state.sampleSearchLoops.clear();
  g_state.sampleInferencePhases.clear();
  g_state.sampleLaunchIntervals.clear();
}

void GlobalPerfProfile::endBenchmarkSample() {
  if(!isEnabled())
    return;
  Clock::time_point now = Clock::now();
  lock_guard<mutex> lock(g_state.stateMutex);
  if(!g_state.benchmarkSampleActive)
    return;

  updateQueueSampleSegmentsLocked(g_state, now);
  updateInferenceThreadSampleSegmentsLocked(g_state, now);

  double totalSeconds = sampleOffsetSeconds(g_state, now);
  double trimmedStart = BENCHMARK_SAMPLE_TRIM_SECONDS;
  double trimmedEnd = totalSeconds - BENCHMARK_SAMPLE_TRIM_SECONDS;
  if(trimmedEnd > trimmedStart) {
    for(const TimedSearchLoop& sample: g_state.sampleSearchLoops) {
      if(sample.offsetSeconds >= trimmedStart && sample.offsetSeconds <= trimmedEnd) {
        g_state.searchLoopProcessMs.push_back(sample.processMs);
        g_state.searchLoopWaitMs.push_back(sample.waitMs);
      }
    }

    for(const TimedInferencePhases& sample: g_state.sampleInferencePhases) {
      if(sample.offsetSeconds >= trimmedStart && sample.offsetSeconds <= trimmedEnd) {
        g_state.inferencePreprocessMs.push_back(sample.preprocessMs);
        g_state.inferenceH2DMs.push_back(sample.h2dMs);
        g_state.inferenceWaitGpuMs.push_back(sample.waitGpuMs);
        g_state.inferenceD2HMs.push_back(sample.d2hMs);
        g_state.inferencePostprocessMs.push_back(sample.postprocessMs);
        if(sample.batchSize >= 0) {
          ensureSize(g_state.gpuWaitGpuMsByBatchSize, (size_t)sample.batchSize + 1);
          g_state.gpuWaitGpuMsByBatchSize[sample.batchSize] += sample.waitGpuMs;
        }
      }
    }

    for(const TimedScalar& sample: g_state.sampleLaunchIntervals) {
      if(sample.offsetSeconds >= trimmedStart && sample.offsetSeconds <= trimmedEnd)
        g_state.inferenceLaunchIntervalMs.push_back(sample.value);
    }

    accumulateTrimmedCountSegments(g_state.sampleQueueSegments, g_state.queueLengthSeconds, trimmedStart, trimmedEnd);
    accumulateTrimmedCountSegments(g_state.sampleInferenceThreadSegments, g_state.inferenceThreadActiveSeconds, trimmedStart, trimmedEnd);
  }

  g_state.benchmarkSampleActive = false;
  g_state.sampleQueueSegments.clear();
  g_state.sampleInferenceThreadSegments.clear();
  g_state.sampleSearchLoops.clear();
  g_state.sampleInferencePhases.clear();
  g_state.sampleLaunchIntervals.clear();
}

void GlobalPerfProfile::recordSearchLoop(double processMilliseconds, double waitMilliseconds) {
  if(!isEnabled())
    return;
  Clock::time_point now = Clock::now();
  lock_guard<mutex> lock(g_state.stateMutex);
  if(!g_state.benchmarkSampleActive)
    return;
  g_state.sampleSearchLoops.push_back(TimedSearchLoop{
    sampleOffsetSeconds(g_state, now),
    processMilliseconds,
    waitMilliseconds
  });
}

void GlobalPerfProfile::noteQueueLength(int queueLength) {
  if(!isEnabled())
    return;
  if(queueLength < 0)
    queueLength = 0;
  Clock::time_point now = Clock::now();
  lock_guard<mutex> lock(g_state.stateMutex);
  updateQueueSampleSegmentsLocked(g_state, now);
  g_state.currentQueueLength = queueLength;
}

void GlobalPerfProfile::changeInferenceThreadActiveCount(int delta) {
  if(!isEnabled())
    return;
  Clock::time_point now = Clock::now();
  lock_guard<mutex> lock(g_state.stateMutex);
  updateInferenceThreadSampleSegmentsLocked(g_state, now);
  g_state.currentInferenceThreadActiveCount += delta;
  if(g_state.currentInferenceThreadActiveCount < 0)
    g_state.currentInferenceThreadActiveCount = 0;
}

void GlobalPerfProfile::recordInferencePhases(
  double preprocessMs,
  double h2dMs,
  double waitGpuMs,
  double d2hMs,
  double postprocessMs,
  int batchSize
) {
  if(!isEnabled())
    return;
  if(batchSize < 0)
    batchSize = 0;
  Clock::time_point now = Clock::now();
  lock_guard<mutex> lock(g_state.stateMutex);
  if(!g_state.benchmarkSampleActive)
    return;
  g_state.sampleInferencePhases.push_back(TimedInferencePhases{
    sampleOffsetSeconds(g_state, now),
    preprocessMs,
    h2dMs,
    waitGpuMs,
    d2hMs,
    postprocessMs,
    batchSize
  });
}

void GlobalPerfProfile::recordInferenceLaunchInterval(double launchIntervalMs) {
  if(!isEnabled())
    return;
  Clock::time_point now = Clock::now();
  lock_guard<mutex> lock(g_state.stateMutex);
  if(!g_state.benchmarkSampleActive)
    return;
  g_state.sampleLaunchIntervals.push_back(TimedScalar{
    sampleOffsetSeconds(g_state, now),
    launchIntervalMs
  });
}

string GlobalPerfProfile::makeReport() {
  if(!isEnabled())
    return "";

  vector<double> searchProcessMs;
  vector<double> searchWaitMs;
  vector<double> queueLengthSeconds;
  vector<double> inferencePreprocessMs;
  vector<double> inferenceH2DMs;
  vector<double> inferenceWaitGpuMs;
  vector<double> inferenceD2HMs;
  vector<double> inferencePostprocessMs;
  vector<double> inferenceLaunchIntervalMs;
  vector<double> gpuWaitGpuMsByBatchSize;
  vector<double> inferenceThreadActiveSeconds;

  {
    lock_guard<mutex> lock(g_state.stateMutex);
    searchProcessMs = g_state.searchLoopProcessMs;
    searchWaitMs = g_state.searchLoopWaitMs;
    queueLengthSeconds = g_state.queueLengthSeconds;
    inferencePreprocessMs = g_state.inferencePreprocessMs;
    inferenceH2DMs = g_state.inferenceH2DMs;
    inferenceWaitGpuMs = g_state.inferenceWaitGpuMs;
    inferenceD2HMs = g_state.inferenceD2HMs;
    inferencePostprocessMs = g_state.inferencePostprocessMs;
    inferenceLaunchIntervalMs = g_state.inferenceLaunchIntervalMs;
    gpuWaitGpuMsByBatchSize = g_state.gpuWaitGpuMsByBatchSize;
    inferenceThreadActiveSeconds = g_state.inferenceThreadActiveSeconds;
  }

  ostringstream out;
  out << "globalPerfProfile\n";
  out << formatPercentileLine("search_process_ms", searchProcessMs) << "\n";
  out << formatPercentileLine("search_wait_nn_ms", searchWaitMs) << "\n";
  out << formatShareHistogram("queue_length_time_share", "q", queueLengthSeconds) << "\n";
  out << formatPercentileLine("inference_preprocess_ms", inferencePreprocessMs) << "\n";
  out << formatPercentileLine("inference_h2d_ms", inferenceH2DMs) << "\n";
  out << formatPercentileLine("inference_wait_gpu_ms", inferenceWaitGpuMs) << "\n";
  out << formatPercentileLine("inference_d2h_ms", inferenceD2HMs) << "\n";
  out << formatPercentileLine("inference_postprocess_ms", inferencePostprocessMs) << "\n";
  out << formatDecileLine("inference_launch_interval_ms", inferenceLaunchIntervalMs) << "\n";
  out << formatShareHistogram("inference_thread_time_share", "active", inferenceThreadActiveSeconds) << "\n";
  out << formatShareHistogram("gpu_batch_time_share", "b", gpuWaitGpuMsByBatchSize) << "\n";
  return out.str();
}
