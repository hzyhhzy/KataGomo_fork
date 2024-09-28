
#include <sstream>
#include "../core/global.h"
#include "../core/bsearch.h"
#include "../core/rand.h"
#include "../core/elo.h"
#include "../core/fancymath.h"
#include "../core/config_parser.h"
#include "../core/datetime.h"
#include "../core/fileutils.h"
#include "../core/base64.h"
#include "../core/timer.h"
#include "../core/threadtest.h"
#include "../game/board.h"
#include "../game/rules.h"
#include "../game/boardhistory.h"
#include "../neuralnet/nninputs.h"
#include "../program/gtpconfig.h"
#include "../program/setup.h"
#include "../tests/tests.h"
#include "../tests/tinymodel.h"
#include "../command/commandline.h"
#include "../main.h"

using namespace std;

int MainCmds::runtests(const vector<string>& args) {
  (void)args;
  testAssert(sizeof(size_t) == 8);
  Board::initHash();
  ScoreValue::initTables();

  BSearch::runTests();
  Rand::runTests();
  DateTime::runTests();
  FancyMath::runTests();
  ComputeElos::runTests();
  Base64::runTests();
  ThreadTest::runTests();

  ScoreValue::freeTables();



  cout << "All tests passed" << endl;
  return 0;
}
