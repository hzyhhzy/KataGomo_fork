#include "../game/randomopening.h"
#include "../game/gamelogic.h"
#include "../core/rand.h"
#include "../search/asyncbot.h"
using namespace RandomOpening;

void RandomOpening::initializeBalancedRandomOpening(
  Search* botB,
  Search* botW,
  Board& board,
  BoardHistory& hist,
  Player& nextPlayer,
  Rand& gameRand,
  bool forSelfplay) {


  double makeOpeningFairRate = forSelfplay ? 0.98 : 1.0;
  double minAcceptRate = forSelfplay ? 0.005 : 0.001;

  if(gameRand.nextBool(makeOpeningFairRate))  // make game fair
  {
    int firstx, firsty;
    while(1) {
      firstx = gameRand.nextUInt(board.x_size), firsty = gameRand.nextUInt(board.y_size);

      Board boardCopy(board);
      BoardHistory histCopy(hist);
      Loc firstMove = Location::getLoc(firstx, firsty, board.x_size);
      histCopy.makeBoardMoveAssumeLegal(boardCopy, firstMove, C_BLACK);

      NNResultBuf nnbuf;
      MiscNNInputParams nnInputParams;
      botW->nnEvaluator->evaluate(boardCopy, histCopy, C_WHITE, nnInputParams, nnbuf, false);
      std::shared_ptr<NNOutput> nnOutput = std::move(nnbuf.result);

      double winrate = nnOutput->whiteWinProb;
      double bias = 2 * winrate - 1;
      double dropPow = forSelfplay ? 6.0 : 20.0;
      double acceptRate = pow(1 - bias * bias, dropPow);
      acceptRate = std::max(acceptRate, minAcceptRate);
      if(gameRand.nextBool(acceptRate))
        break;
    }

    Loc firstMove = Location::getLoc(firstx, firsty, board.x_size);
    hist.makeBoardMoveAssumeLegal(board, firstMove, nextPlayer);
    nextPlayer = getOpp(nextPlayer);
  }


}

void RandomOpening::initializeSpecialOpening(
  Search* botB,
  Search* botW,
  Board& board,
  BoardHistory& hist,
  Player& nextPlayer,
  Rand& gameRand) {

  for(int i = 0; i < board.y_size - 1; i++) {
    int x = i;
    int y = board.y_size - i - 2;
    Loc loc = Location::getLoc(x, y, board.x_size);
    board.setStone(loc, C_WHITE);
  }

  for(int i = board.y_size - 1; i < board.x_size; i++) {
    int x = i;
    int y = 0;
    Loc loc = Location::getLoc(x, y, board.x_size);
    board.setStone(loc, C_WHITE);
  }
  int midX = (board.x_size + board.y_size - 1 - 2) * 0.5 + 0.7 * gameRand.nextGaussianTruncated(5.0) + 0.5;
  if(midX >= board.x_size)
    midX = board.x_size - 1;
  if(midX < board.y_size - 1)
    midX = board.y_size - 1;
  for(int i = 0; i < 2; i++) {
    int x = midX;
    int y = i;
    Loc loc = Location::getLoc(x, y, board.x_size);
    board.setStone(loc, C_BLACK);
  }
  nextPlayer = C_WHITE;
  

  auto rules = hist.rules;
  hist.clear(board, nextPlayer, rules);
}

void RandomOpening::initializeCompletelyRandomOpening(
  Board& board,
  BoardHistory& hist,
  Player& nextPlayer,
  Rand& gameRand,
  double areaPropAvg) {
  double fillProb = gameRand.nextExponential() * areaPropAvg;
  randomFillBoard(board, gameRand, fillProb, fillProb);
  nextPlayer = gameRand.nextBool(0.5) ? C_BLACK : C_WHITE;
  auto rules = hist.rules;
  hist.clear(board, nextPlayer, rules);
}

void RandomOpening::randomFillBoard(Board& board, Rand& gameRand, double bProb, double wProb) {
  if(bProb > 0.5)
    bProb = 0.5;
  if(wProb + bProb > 1)
    wProb = 1 - bProb;

  for(int x = 0; x < board.x_size; x++)
    for(int y = 0; y < board.y_size; y++) {
      Loc loc = Location::getLoc(x, y, board.x_size);
      if (board.colors[loc] == C_EMPTY)
      {
        Color c = C_EMPTY;
        double r = gameRand.nextDouble();
        if(r < bProb)
          c = C_BLACK;
        else if(r < bProb + wProb)
          c = C_WHITE;
        board.setStone(loc, c);
      }
    }
}
