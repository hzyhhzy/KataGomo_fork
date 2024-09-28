#ifndef TESTS_H
#define TESTS_H

#include <sstream>

#include "../core/global.h"
#include "../core/logger.h"
#include "../core/rand.h"
#include "../core/test.h"
#include "../game/board.h"
#include "../game/rules.h"
#include "../game/boardhistory.h"

class NNEvaluator;

namespace Tests {
  //testboardbasic.cpp
  //testnn.cpp
  void runNNLayerTests();
  void runNNSymmetryTests();  
  //testnnevalcanary.cpp
  void runCanaryTests(NNEvaluator* nnEval, int symmetry, bool print);
  bool runBackendErrorTest(
    NNEvaluator* nnEval,
    NNEvaluator* nnEval32,
    Logger& logger,
    int boardSize,
    int maxBatchSizeCap,
    bool verbose,
    bool quickTest,
    bool& fp32BatchSuccessBuf,
    //Values on disk to compare correctness. We consider the pure-cpu float32 Eigen implementation of the neural network
    //to be the source of truth, since it is more likely to be stable and doesn't depend special hardware or drivers like
    //GPUs or other accelerators.
    //When running with Eigen backend, will overwrite this file with Eigen's results.
    const std::string& referenceFileName
  );

}

namespace TestCommon {
  bool boardsSeemEqual(const Board& b1, const Board& b2);

  constexpr int MIN_BENCHMARK_SGF_DATA_SIZE = 7;
  constexpr int MAX_BENCHMARK_SGF_DATA_SIZE = 19;
  constexpr int DEFAULT_BENCHMARK_SGF_DATA_SIZE = std::min(Board::DEFAULT_LEN,MAX_BENCHMARK_SGF_DATA_SIZE);
  std::string getBenchmarkSGFData(int boardSize);

  std::vector<std::string> getMultiGameSize9Data();
  std::vector<std::string> getMultiGameSize13Data();
  std::vector<std::string> getMultiGameSize19Data();

  void overrideForBackends(bool& inputsNHWC, bool& useNHWC);
}

#endif
