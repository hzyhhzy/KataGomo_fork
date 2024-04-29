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
  int r = gameRand.nextUInt(100);
  if (r < 10)//Gale's game
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
  else if(r < 40)  //transfinite opening 1
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
  else if(board.x_size >= 13 && r < 60)  // infinite template problem 1
  {
    for(int i = 0; i < board.y_size - 1; i++) {
      int x = board.x_size - 1;
      int y = i;
      Loc loc = Location::getLoc(x, y, board.x_size);
      board.setStone(loc, C_BLACK);
    }
    for(int i = 0; i < board.y_size - 1; i++) {
      int x = 0;
      int y = i;
      Loc loc = Location::getLoc(x, y, board.x_size);
      board.setStone(loc, C_BLACK);
    }
    for(int i = 0; i < board.x_size - 2; i++) {
      int x = i + 2;
      int y = board.y_size - 1;
      Loc loc = Location::getLoc(x, y, board.x_size);
      board.setStone(loc, C_WHITE);
    }

    {
      board.setStone(Location::getLoc(0, board.y_size - 1, board.x_size), C_WHITE);
      board.setStone(Location::getLoc(1, board.y_size - 2, board.x_size), C_WHITE);
      board.setStone(Location::getLoc(2, board.y_size - 3, board.x_size), C_WHITE);
      board.setStone(Location::getLoc(1, board.y_size - 1, board.x_size), C_BLACK);
      board.setStone(Location::getLoc(2, board.y_size - 2, board.x_size), C_BLACK);
    }
    if(gameRand.nextBool(0.5))
    {
      board.setStone(Location::getLoc(4, board.y_size - 3, board.x_size), C_WHITE);
      board.setStone(Location::getLoc(4, board.y_size - 4, board.x_size), C_WHITE);
      board.setStone(Location::getLoc(3, board.y_size - 2, board.x_size), C_BLACK);
      board.setStone(Location::getLoc(4, board.y_size - 2, board.x_size), C_BLACK);
    }

    {
      //find a balanced move
      double minAcceptRate = 0.03;

      Loc firstMove = Board::PASS_LOC;
      while(1) {
        double ymean = 1.6 * sqrt(board.x_size);
        int y = ymean *
                (gameRand.nextExponential() + gameRand.nextExponential() + gameRand.nextExponential() +
                 gameRand.nextExponential() + gameRand.nextExponential() + gameRand.nextExponential()) /
                6;
        if(y > board.y_size - 1)
          continue;
        firstMove = Location::getLoc(gameRand.nextUInt(board.x_size), board.y_size - 1 - y, board.x_size);

        if(board.colors[firstMove] != C_EMPTY)
          continue;
        Board boardCopy(board);
        boardCopy.setStone(firstMove, C_WHITE);
        BoardHistory histCopy(board, C_WHITE, hist.rules);


        NNResultBuf nnbuf;
        MiscNNInputParams nnInputParams;
        botW->nnEvaluator->evaluate(boardCopy, histCopy, C_WHITE, nnInputParams, nnbuf, false);
        std::shared_ptr<NNOutput> nnOutput = std::move(nnbuf.result);

        double winrate = nnOutput->whiteWinProb;
        double bias = 2 * winrate - 1;
        double dropPow = 2.0 ;
        double acceptRate = pow(1 - bias * bias, dropPow);
        acceptRate = std::min(acceptRate + minAcceptRate, 1.0);
        if(gameRand.nextBool(acceptRate))
          break;
      }

      board.setStone(firstMove, C_WHITE);
    }
    nextPlayer = C_WHITE;
  } 
  else if(board.x_size >= 25 && r < 80)  // 10th template
  {
    int templateRow = 10;
    for(int i = 0; i < templateRow; i++) {
      int x = i;
      int y = board.y_size - i - 2;
      Loc loc = Location::getLoc(x, y, board.x_size);
      board.setStone(loc, C_WHITE);
    }

    for(int i = templateRow; i < board.x_size; i++) {
      int x = i;
      int y = board.y_size - templateRow - 1;
      Loc loc = Location::getLoc(x, y, board.x_size);
      board.setStone(loc, C_WHITE);
    }
    int midX = (board.x_size + templateRow - 2) / 2;
    for(int i = 0; i < board.y_size - templateRow + 1; i++) {
      int x = midX;
      int y = i;
      Loc loc = Location::getLoc(x, y, board.x_size);
      board.setStone(loc, C_BLACK);
    }
    nextPlayer = C_WHITE;
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
