#include "../program/randomopening.h"

#include <sstream>

#include "../core/timer.h"
#include "../core/test.h"

using namespace std;

static double getBoardValue(Search* bot, const Board& board, const BoardHistory& hist, Player nextPlayer) {
  NNEvaluator* nnEval = bot->nnEvaluator;
  MiscNNInputParams nnInputParams;
  NNResultBuf buf;
  nnEval->evaluate(board, hist, nextPlayer, nnInputParams, buf, false, false);
  std::shared_ptr<NNOutput> nnOutput = std::move(buf.result);
  double value = nnOutput->whiteWinProb - nnOutput->whiteLossProb;
  if(nextPlayer == C_BLACK)
    return -value;
  else
    return value;
}

static Loc getOneRandomLegalLocation(const Board& board, Player pla, Rand& rand) {
  int tried = 0;
  while(true) {
    tried++;
    int x = rand.nextUInt(board.x_size);
    int y = rand.nextUInt(board.y_size);
    Loc loc = Location::getLoc(x, y, board.x_size);
    if(board.isLegal(loc, pla, false))
      return loc;
    if(tried > 1000)
      throw StringError("getOneRandomLegalLocation have tried too many times without finding a legal location");
  }
}

void RandomOpening::getRandomBalanceOpening(Search* bot, Board& board, Player& pla, Rand& rand) {
  pla = C_WHITE;
  if(board.numStonesOnBoard() > board.x_size * board.y_size / 4)
    throw StringError("Don't call RandomOpening::getRandomBalanceOpening when the board have too many stones");
  int maxInitialStones = int(board.x_size * board.y_size * 0.10);

  Board boardTmp = board;

  if(maxInitialStones == 0)
    return;

  vector<Loc> locSeq;
  vector<double> locNumList;  // which number of initial stones are possible
  vector<double> probList;  // more balanced winrate has higher prob

  //zero initialStones
  {
    int stoneNum = 0;
    BoardHistory hist(boardTmp, C_WHITE, Rules());
    double whiteValue = getBoardValue(bot, boardTmp, hist, C_WHITE);
    double prob = 0.001 + pow(1 - whiteValue * whiteValue, 4);

    locNumList.push_back(stoneNum);
    probList.push_back(prob);
  }

  for(int i = 0; i < maxInitialStones; i++) {
    Loc loc = getOneRandomLegalLocation(boardTmp, C_BLACK, rand);
    boardTmp.playMoveAssumeLegal(loc, C_BLACK);
    locSeq.push_back(loc);
    int stoneNum = i + 1;
    BoardHistory hist(boardTmp, C_WHITE, Rules());
    double whiteValue = getBoardValue(bot, boardTmp, hist, C_WHITE);
    double prob = 0.001 + pow(1 - whiteValue * whiteValue, 4);

    locNumList.push_back(stoneNum);
    probList.push_back(prob);
  }
  assert(locNumList.size() == probList.size());
  assert(locSeq.size() == locNumList[locNumList.size()-1]);

  int numToPlay = locNumList[rand.nextUInt(probList.data(), probList.size())];

  for(int i = 0; i < numToPlay; i++) {
    Loc locToPlay = locSeq[i];
    assert(board.isLegal(locToPlay, C_BLACK, false));
    board.playMoveAssumeLegal(locToPlay, C_BLACK);
  }

}
bool RandomOpening::getPredefinedOpening(Board& board, Player& pla, Rand& rand) {
  if(board.numStonesOnBoard() > 0)
    return false;
  string boardStr = "";
  if (board.x_size == 19 && board.y_size == 19)
  {
    std::string openingStrs19x[] = {
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . x . . x . . . x . . . x . . x . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . x . . x . . . . . . . . . . x . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . x . . . . . . x . . . . . . x . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . x . . . . . . . . . . x . . x . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . x . . x . . . x . . . x . . x . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . ",
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . x . . . . . . x . . . . . . x . . "
      ". . . x . . . . . x . . . . . x . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . x . . . . . . . . . "
      ". . x x . . . . x . x . . . . x x . . "
      ". . . . . . . . . x . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . x . . . . . x . . . . . x . . . "
      ". . x . . . . . . x . . . . . . x . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . ",
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . x . . . . . x . . . . . x . . . "
      ". . x . . . . . . x . . . . . . x . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . x . . . . . . . . . "
      ". . x x . . . . x . x . . . . x x . . "
      ". . . . . . . . . x . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . x . . . . . . x . . . . . . x . . "
      ". . . x . . . . . x . . . . . x . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . ",
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . x . . . . . x . . . . . x . . . "
      ". . x . x . . . . . . . . . x . x . . "
      ". . . x . . . . . . . . . . . x . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . x . . . . . . . . . "
      ". . x . . . . . . . . . . . . . x . . "
      ". . . . . . . . . x . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . x . . . . . . . . . . . x . . . "
      ". . x . x . . . . . . . . . x . x . . "
      ". . . x . . . . . x . . . . . x . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . ",
      "x x x x x x x x x x x x x x x x x x x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x x x x x x x x x x x x x x x x x x x ",
      "x x x x x x x x x x x x x x x x x x x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . o . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x x x x x x x x x x x x x x x x x x x ",
      "x x x x x x x x x x x x x x x x x x x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x x x x x x x x x x x x x x x x x x x ",
      "x x x x x x x x x x x x x x x x x x x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . o . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x . . . . . . . . . . . . . . . . . x "
      "x x x x x x x x x x x x x x x x x x x ",
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . x . . x . . x . . x . . x . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . x . . x . . x . . x . . x . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . x . . x . . x . . x . . x . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . x . . x . . x . . x . . x . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . x . . x . . x . . x . . x . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . ",

      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . x . . x . . x . . x . . x . . . "
      ". . x . . . . . . . . . . . . . x . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . x . . . . . . . . . . . . . x . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . x . . . . . . . . . . . . . x . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . x . . . . . . . . . . . . . x . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . x . . . . . . . . . . . . . x . . "
      ". . . x . . x . . x . . x . . x . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . ",
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . x . . . . . . x . . . . . . x . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . x . . . . x . . . . x . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . x . . . . . x . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . x . x . . . . . . . . . x . x . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . x . . . . . x . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . x . . . . x . . . . x . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . x . . . . . . x . . . . . . x . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "};

    const int numOpenings = sizeof(openingStrs19x) / sizeof(string);
    int openingID = rand.nextUInt(numOpenings);
    boardStr = openingStrs19x[openingID];
    
  } 
  else if(board.x_size == 10 && board.y_size == 10) {
    if(rand.nextBool(0.9))
      return false;
    std::string openingStrs10x[] = {
      ". . . . . . . . . . "
      "x x x x x x x x x x "
      ". . . . . . . . . x "
      ". . . . . . . . . x "
      ". . . . . . . . . x "
      ". . . . . . . . . x "
      ". . . . . . . . . x "
      ". . . . . . . . . x "
      ". . . . . . . . . x "
      ". . . . . . . . . x "
      ,
    };

    const int numOpenings = sizeof(openingStrs10x) / sizeof(string);
    int openingID = rand.nextUInt(numOpenings);
    boardStr = openingStrs10x[openingID];
  } 
  else if(board.x_size == 19 && board.y_size == 7) {
    if(rand.nextBool(0.8))
      return false;
    std::string openingStrs19x7[] = {
      ". . . . . . . . . . . . . . . . . . . "
      "x x x x x x x x x x x x x x x x x x x "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . "
      ". . . . . . . . . . . . . . . . . . . ",
    };

    const int numOpenings = sizeof(openingStrs19x7) / sizeof(string);
    int openingID = rand.nextUInt(numOpenings);
    boardStr = openingStrs19x7[openingID];
  } 

  if(boardStr == "")
    return false;
  assert(boardStr.size() == 2 * board.x_size * board.y_size + 1);

  for(int y = 0; y < board.y_size; y++)
    for(int x = 0; x < board.x_size; x++) {
      int pos = x + y * board.x_size;
      char c = boardStr[2 * pos];
      Color color = c == 'x' ? C_BLACK : c == 'o' ? C_WHITE : C_EMPTY;
      Loc loc = Location::getLoc(x, y, board.x_size);
      assert(board.isLegal(loc, color, false));
      board.playMoveAssumeLegal(loc, color);
    }
  return true;
}

void RandomOpening::randomizePredefinedOpening(Search* bot, Board& board, Player& pla, Rand& rand) {
  if(rand.nextBool(0.5))  // not randomize
    return;
  int stoneNum = board.numStonesOnBoard();
  if(stoneNum == 0)
    throw StringError("RandomOpening::randomizePredefinedOpening should not called for empty boards");

  double removeProb = 2.0 / stoneNum;  // averagely remove 2 stones
  for(int y = 0; y < board.y_size; y++)
    for(int x = 0; x < board.x_size; x++) {
      Loc loc = Location::getLoc(x, y, board.x_size);
      if(board.colors[loc] != C_EMPTY)
        if(rand.nextBool(removeProb)) {
          board.setStone(loc, C_EMPTY);
        }
    }


  //make it balanced
  getRandomBalanceOpening(bot, board, pla, rand);
}

void RandomOpening::getOpening(Search* bot, Board& board, Player& pla, double predefinedOpeningProb, Rand& rand) {
  bool usePredefinedOpening = false;
  if(rand.nextBool(predefinedOpeningProb))  // try to use predefinedOpening
  {
    usePredefinedOpening = getPredefinedOpening(board, pla, rand);
  }
  if(usePredefinedOpening)
    randomizePredefinedOpening(bot, board, pla, rand);
  else
    getRandomBalanceOpening(bot, board, pla, rand);
}




const bool EARLY_STAGE = true;
void RandomOpening::getRandomInitialOpeningOld(Board& board, Player& pla, Rand& rand) {
  double blackFillDensity = 22.0 / 361.0;
  if(EARLY_STAGE) {
    if(rand.nextBool(0.99))
      blackFillDensity += 0.1 * (rand.nextBool(0.5) ? 1.0 : -1.0) * rand.nextExponential();
  } 
  else {
    if(rand.nextBool(0.3))
      blackFillDensity += 0.02 * (rand.nextBool(0.5) ? 1.0 : -1.0) * rand.nextExponential();
  }
  int blackFillNum = int(blackFillDensity * board.x_size * board.y_size + rand.nextGaussian());
  if(blackFillNum < 1)
    blackFillNum = 1;
  if(blackFillNum > board.x_size * board.y_size / 2)
    blackFillNum = board.x_size * board.y_size / 2;

  for (int i = 0; i < blackFillNum; i++)
  {
    Loc loc = getOneRandomLegalLocation(board, C_BLACK, rand);
    board.playMoveAssumeLegal(loc, C_BLACK);
  }
  pla = C_WHITE;
}
