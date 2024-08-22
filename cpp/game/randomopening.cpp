#include "../game/randomopening.h"
#include "../game/gamelogic.h"
#include "../core/rand.h"
#include "../search/asyncbot.h"
using namespace RandomOpening;

void RandomOpening::initializeSpecialOpening(
  Board& board,
  BoardHistory& hist,
  Player& nextPlayer,
  Rand& gameRand) {
  if(board.x_size != 19 || board.y_size != 19 || (hist.rules.maxMoves > 0 && hist.rules.maxMoves < 20))
    return;
  int r = gameRand.nextUInt(100);
  if(r < 30)  // J10(Center)
  {
    hist.makeBoardMoveAssumeLegal(board,Location::getLoc(9, 9, board.x_size), board.nextPla);
    nextPlayer = board.nextPla;
  } 
  else if(r < 50)  // J10 K11-J8 J12-K12
  {
    hist.makeBoardMoveAssumeLegal(board,Location::getLoc(9, 9, board.x_size), board.nextPla);
    hist.makeBoardMoveAssumeLegal(board,Location::getLoc(10, 10, board.x_size), board.nextPla);
    hist.makeBoardMoveAssumeLegal(board,Location::getLoc(9, 7, board.x_size), board.nextPla);
    hist.makeBoardMoveAssumeLegal(board,Location::getLoc(9, 11, board.x_size), board.nextPla);
    hist.makeBoardMoveAssumeLegal(board,Location::getLoc(10, 11, board.x_size), board.nextPla);
    nextPlayer = board.nextPla;
  } 
  else if(r < 65)  // J10 K11-J8 J12-K12 I12-J13 I10-H11
  {
    hist.makeBoardMoveAssumeLegal(board,Location::getLoc(9, 9, board.x_size), board.nextPla);
    hist.makeBoardMoveAssumeLegal(board,Location::getLoc(10, 10, board.x_size), board.nextPla);
    hist.makeBoardMoveAssumeLegal(board,Location::getLoc(9, 7, board.x_size), board.nextPla);
    hist.makeBoardMoveAssumeLegal(board,Location::getLoc(9, 11, board.x_size), board.nextPla);
    hist.makeBoardMoveAssumeLegal(board,Location::getLoc(10, 11, board.x_size), board.nextPla);
    hist.makeBoardMoveAssumeLegal(board,Location::getLoc(8, 11, board.x_size), board.nextPla);
    hist.makeBoardMoveAssumeLegal(board,Location::getLoc(9, 12, board.x_size), board.nextPla);
    hist.makeBoardMoveAssumeLegal(board,Location::getLoc(8, 9, board.x_size), board.nextPla);
    hist.makeBoardMoveAssumeLegal(board,Location::getLoc(7, 10, board.x_size), board.nextPla);
    nextPlayer = board.nextPla;
  } 
  else if(r < 80)  // J10 I9-K9 
  {
    hist.makeBoardMoveAssumeLegal(board,Location::getLoc(9, 9, board.x_size), board.nextPla);
    hist.makeBoardMoveAssumeLegal(board,Location::getLoc(8, 8, board.x_size), board.nextPla);
    hist.makeBoardMoveAssumeLegal(board,Location::getLoc(10, 8, board.x_size), board.nextPla);
    nextPlayer = board.nextPla;
  } 
  auto rules = hist.rules;
  hist.clear(board, nextPlayer, rules);
}
