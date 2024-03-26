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

void RandomOpening::initializeSpecialOpening(Board& board, BoardHistory& hist, Player& nextPlayer, Rand& gameRand) {
  int r = gameRand.nextInt() % 100;
  if (r < 40)//Gale's game
  {
    for(int x = 0; x < board.x_size; x++)
      for(int y = 0; y < board.y_size; y++) {
        Loc loc = Location::getLoc(x, y, board.x_size);
        if(x % 2 == 0 && y % 2 == 1) {
          board.setStone(loc, C_BLACK);
        }
        if(x % 2 == 1 && y % 2 == 0) {
          board.setStone(loc, C_WHITE);
        }
      }

    //the first player has a simple winning strategy, so random play some stones to avoid this
    double fillProb = gameRand.nextExponential() * 0.02;
    randomFillBoard(board, gameRand, fillProb, fillProb);
    nextPlayer = gameRand.nextBool(0.5) ? C_BLACK : C_WHITE;
  }
  else if(r < 80)  //transfinite opening 1
  {
    for(int i = 0; i < board.x_size - 1; i++) {
      int x = i + 1;
      int y = 0;
      Loc loc = Location::getLoc(x, y, board.x_size);
      board.setStone(loc, C_BLACK);
    }
    for(int i = 0; i < board.y_size - 1; i++) {
      int x = board.x_size - 1;
      int y = i;
      Loc loc = Location::getLoc(x, y, board.x_size);
      board.setStone(loc, C_BLACK);
    }
    for(int i = 0; i < board.x_size - 1; i++) {
      int x = i + 1;
      int y = board.y_size - 1;
      Loc loc = Location::getLoc(x, y, board.x_size);
      board.setStone(loc, C_WHITE);
    }
    for(int i = 0; i < board.y_size - 1; i++) {
      int x = 0;
      int y = i;
      Loc loc = Location::getLoc(x, y, board.x_size);
      board.setStone(loc, C_WHITE);
    }

    {
      int x = 0;
      int y = board.y_size - 1;
      Loc loc = Location::getLoc(x, y, board.x_size);
      board.setStone(loc, C_BLACK);
    }
    {
      int x = 1;
      int y = board.y_size - 2;
      Loc loc = Location::getLoc(x, y, board.x_size);
      board.setStone(loc, C_BLACK);
    }


    nextPlayer = gameRand.nextBool(0.2) ? C_BLACK : C_WHITE;
  }
  else //rotated normal board
  {
    for(int i = 0; i < board.y_size - 1; i++) {
      int x = 0;
      int y = i;
      Loc loc = Location::getLoc(x, y, board.x_size);
      board.setStone(loc, C_BLACK);
    }
    for(int i = 0; i < board.x_size - 1; i++) {
      int x = i;
      int y = board.y_size - 1;
      Loc loc = Location::getLoc(x, y, board.x_size);
      board.setStone(loc, C_WHITE);
    }
    for(int i = 0; i < board.y_size - 1; i++) {
      int x = board.x_size - 1;
      int y = i + 1;
      Loc loc = Location::getLoc(x, y, board.x_size);
      board.setStone(loc, C_BLACK);
    }
    for(int i = 0; i < board.x_size - 1; i++) {
      int x = i + 1;
      int y = 0;
      Loc loc = Location::getLoc(x, y, board.x_size);
      board.setStone(loc, C_WHITE);
    }

    double fillProb = gameRand.nextExponential() * 0.005;
    randomFillBoard(board, gameRand, fillProb, fillProb);
    nextPlayer = gameRand.nextBool(0.5) ? C_BLACK : C_WHITE;
  }

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
