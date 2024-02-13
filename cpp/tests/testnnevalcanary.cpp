#include "../tests/tests.h"

#include "../neuralnet/nneval.h"
#include "../dataio/sgf.h"

//------------------------
#include "../core/using.h"
//------------------------

void Tests::runCanaryTests(NNEvaluator* nnEval, int symmetry, bool print) {

}

bool Tests::runFP16Test(NNEvaluator* nnEval, NNEvaluator* nnEval32, Logger& logger, int boardSize, int maxBatchSizeCap, bool verbose, bool quickTest, bool& fp32BatchSuccessBuf) {
  return true;
}