#include "../tests/tests.h"

#include "../neuralnet/nneval.h"
#include "../dataio/sgf.h"
#include "../tests/tests.h"
#include "../command/commandline.h"
#include "../program/playutils.h"
#include "../main.h"

#include "../nnue/Eva_nnuev2.h"
//------------------------
#include "../core/using.h"
//------------------------

using namespace std;
using namespace NNUE;

int MainCmds::testnnue() {
  Board::initHash();
  Rand seedRand;

  ConfigParser cfg;
  string nnueModelFile;
  /*
  try {
    KataGoCommandLine cmd("Test NNUE.");
    cmd.addConfigFileArg(KataGoCommandLine::defaultGtpConfigFileName(), "nnue_example.cfg");
    TCLAP::ValueArg<int> boardSizeArg(
      "", "boardsize", "Size of board, default 19", false, 19, "SIZE");
    cmd.add(boardSizeArg);

    cmd.setShortUsageArgLimit();
    cmd.addOverrideConfigArg();

    cmd.parseArgs(args);

    boardSize = boardSizeArg.getValue();
    cmd.getConfig(cfg);

  } catch(TCLAP::ArgException& e) {
    cerr << "Error: " << e.error() << " for argument " << e.argId() << endl;
    return 1;
  }*/

  const bool logToStdoutDefault = true;
  const bool logToStderrDefault = false;
  const bool logTimeDefault = false;
  Logger logger(NULL, logToStdoutDefault, logToStderrDefault, logTimeDefault);

  string modelpath = "J:/gomtrain2024/connectsix/export/v2_64_highwd2.txt";

  Eva_nnuev2* eva = new Eva_nnuev2();
  eva->loadParam(modelpath);

  Board board(19, 19);
  string rootBoardSequence = "j10i8l10";
  vector<Loc> rootBoardLocSeq = Location::parseSequenceGom(rootBoardSequence, board);
  PlayUtils::playMoveLocSequence(board, board.nextPla, rootBoardLocSeq);
  eva->debug_print();
  return 0;


}
