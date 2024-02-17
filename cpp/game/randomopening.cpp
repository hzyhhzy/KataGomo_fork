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