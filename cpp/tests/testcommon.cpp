#include "../tests/tests.h"

using namespace std;

bool TestCommon::boardsSeemEqual(const Board& b1, const Board& b2) {
  for(int i = 0; i<Board::MAX_ARR_SIZE; i++)
    if(b1.colors[i] != b2.colors[i])
      return false;
  return true;
}

string TestCommon::getBenchmarkSGFData(int boardSize) {
  string sgfData;
  static_assert(MIN_BENCHMARK_SGF_DATA_SIZE == 5, "TestCommon::getBenchmarkSGFData MIN_BENCHMARK_SGF_DATA_SIZE == 7");
  static_assert(MAX_BENCHMARK_SGF_DATA_SIZE == 51, "TestCommon::getBenchmarkSGFData MAX_BENCHMARK_SGF_DATA_SIZE == 19");

  if (boardSize >= 5 && boardSize <= 51) {
    sgfData = "(;FF[4]GM[1]SZ[" + to_string(boardSize) + "]HA[0]KM[0];B[ce])";
  }
  else
  {
    throw StringError("getBenchmarkSGFData: unsupported board size: " + Global::intToString(boardSize));
  }
  return sgfData;
}


void TestCommon::overrideForBackends(bool& inputsNHWC, bool& useNHWC) {
#if defined(USE_OPENCL_BACKEND)
  if(inputsNHWC != false) {
    cout << "Backend is opencl, ignoring args and forcing inputsNHWC=false" << endl;
    inputsNHWC = false;
  }
  if(useNHWC != false) {
    cout << "Backend is opencl, ignoring args and forcing useNHWC=false" << endl;
    useNHWC = false;
  }
#elif defined(USE_TENSORRT_BACKEND)
  if(inputsNHWC != false) {
    cout << "Backend is TensorRT, ignoring args and forcing inputsNHWC=false" << endl;
    inputsNHWC = false;
  }
  if(useNHWC != false) {
    cout << "Backend is TensorRT, ignoring args and forcing useNHWC=false" << endl;
    useNHWC = false;
  }
#else
  (void)inputsNHWC;
  (void)useNHWC;
#endif
}
