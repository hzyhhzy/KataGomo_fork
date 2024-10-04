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
  int minInitialStones = int(board.x_size * board.y_size * 0.03);
  int maxInitialStones = int(board.x_size * board.y_size * 0.10);

  Board boardTmp = board;

  if(maxInitialStones == 0)
    return;

  vector<Loc> locSeq;
  vector<double> locNumList;  // which number of initial stones are possible
  vector<double> probList;  // more balanced winrate has higher prob
  for(int i = 0; i < maxInitialStones; i++) {
    Loc loc = getOneRandomLegalLocation(boardTmp, C_BLACK, rand);
    boardTmp.playMoveAssumeLegal(loc, C_BLACK);
    locSeq.push_back(loc);
    int stoneNum = i + 1;
    if (stoneNum >= minInitialStones)
    {
      BoardHistory hist(boardTmp, C_WHITE, Rules());
      double whiteValue = getBoardValue(bot, boardTmp, hist, C_WHITE);
      double prob = 0.001 + pow(1 - whiteValue * whiteValue, 4);

      locNumList.push_back(stoneNum);
      probList.push_back(prob);
    }
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
void RandomOpening::getOpening(Search* bot, Board& board, Player& pla, Rand& rand) {
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
