#ifndef TESTS_H
#define TESTS_H

#include <sstream>

#include "../core/global.h"
#include "../core/rand.h"
#include "../core/test.h"
#include "../game/board.h"
#include "../game/rules.h"
#include "../game/boardhistory.h"

class NNEvaluator;

namespace TestCommon {
  bool boardsSeemEqual(const Board& b1, const Board& b2);

  constexpr int MIN_BENCHMARK_SGF_DATA_SIZE = 5;
  constexpr int MAX_BENCHMARK_SGF_DATA_SIZE = 51;
  constexpr int DEFAULT_BENCHMARK_SGF_DATA_SIZE = std::min(Board::DEFAULT_LEN,MAX_BENCHMARK_SGF_DATA_SIZE);
  std::string getBenchmarkSGFData(int boardSize);

  void overrideForBackends(bool& inputsNHWC, bool& useNHWC);
}

#endif
