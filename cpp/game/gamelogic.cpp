#include "../game/gamelogic.h"

/*
 * gamelogic.cpp
 * Logics of game rules
 * Some other game logics are in board.h/cpp
 *
 * Gomoku as a representive
 */

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

using namespace std;


static void nextLocOfCircle(int bs, int& x, int& y, int& dx, int& dy, bool& hasCircle) {
  x = x + dx;
  y = y + dy;
  if(x < 0 || y < 0 || x >= bs || y >= bs)
    hasCircle = true;
  if(x < 0) {
    assert(dx == -1);
    assert(dy == 0);
    if(y < bs / 2) {
      x = y;
      y = 0;
      dx = 0;
      dy = 1;
    } else {
      x = bs - y - 1;
      y = bs - 1;
      dx = 0;
      dy = -1;
    }
  }
  else if(y < 0) {
    assert(dy == -1);
    assert(dx == 0);
    if(x < bs / 2) {
      y = x;
      x = 0;
      dy = 0;
      dx = 1;
    } else {
      y = bs - x - 1;
      x = bs - 1;
      dy = 0;
      dx = -1;
    }
  }
  else if(x >= bs) {
    assert(dx == 1);
    assert(dy == 0);
    if(y < bs / 2) {
      x = bs - y - 1;
      y = 0;
      dx = 0;
      dy = 1;
    } else {
      x = y;
      y = bs - 1;
      dx = 0;
      dy = -1;
    }
  } 
  else if(y >= bs) {
    assert(dy == 1);
    assert(dx == 0);
    if(x < bs / 2) {
      y = bs - x - 1;
      x = 0;
      dy = 0;
      dx = 1;
    } else {
      y = x;
      x = bs - 1;
      dy = 0;
      dx = -1;
    }
  }
}
static Loc findCaptureLocOneDirection(const Board& board, Player pla, Loc startLoc,int dx,int dy) {
  assert(board.x_size == board.y_size);
  int bs = board.x_size;
  int x = Location::getX(startLoc, bs);
  int y = Location::getY(startLoc, bs);
  bool hasCircle = false;
  if((x == 0 || x == bs - 1) && (y == 0 || y == bs - 1))
    return Board::NULL_LOC;
  for(int step=0;step<4*bs;step++) {
    nextLocOfCircle(bs, x, y, dx, dy, hasCircle);
    if((x == 0 || x == bs - 1) && (y == 0 || y == bs - 1))
      return Board::NULL_LOC;
    Loc loc = Location::getLoc(x, y, bs);
    assert(board.isOnBoard(loc));
    if(loc != startLoc && board.colors[loc] != C_EMPTY) {
      if(!hasCircle || board.colors[loc] == pla)
        return Board::NULL_LOC;
      else
        return loc;
    }
  }
  return Board::NULL_LOC;
}

static bool isLegalCapture(const Board& board, Player pla, Loc startLoc, Loc targetLoc) {
  static const int dxs[4] = {1, -1, 0, 0};
  static const int dys[4] = { 0, 0, 1, -1};
  for(int i = 0; i < 4; i++)
    if(findCaptureLocOneDirection(board, pla, startLoc, dxs[i], dys[i]) == targetLoc)
      return true;
  return false;
}

bool GameLogic::isLegal(const Board& board, Player pla, Loc loc) {
  if(pla != board.nextPla) {
    std::cout << "Error next player ";
    return false;
  }

  if(loc == Board::PASS_LOC)  // pass is lose, but not illegal
    return true;

  if(!board.isOnBoard(loc))
    return false;

  if(board.stage == 0)  // choose a piece
  {
    return board.colors[loc] == pla;
  } 
  else if(board.stage == 1)  // place the piece
  {
    Color opp = getOpp(pla);
    Loc chosenMove = board.midLocs[0];
    Color c = board.colors[loc];
    if(c == C_EMPTY)
      return Location::euclideanDistanceSquared(loc, chosenMove, board.x_size) <= 2;
    else if(c == opp)  // capture
      return isLegalCapture(board, pla, chosenMove, loc);
    else
      return false;
  }
  ASSERT_UNREACHABLE;
  return false;
}

GameLogic::MovePriority GameLogic::getMovePriorityAssumeLegal(const Board& board, const BoardHistory& hist, Player pla, Loc loc) {
  return MP_NORMAL;
}

GameLogic::MovePriority GameLogic::getMovePriority(const Board& board, const BoardHistory& hist, Player pla, Loc loc) {
  if(loc == Board::PASS_LOC)
    return MP_NORMAL;
  if(!board.isLegal(loc, pla))
    return MP_ILLEGAL;
  MovePriority MP = getMovePriorityAssumeLegal(board, hist, pla, loc);
  return MP;
}




Color GameLogic::checkWinnerAfterPlayed(
  const Board& board,
  const BoardHistory& hist,
  Player pla,
  Loc loc) {
  if(loc == Board::PASS_LOC)
    return getOpp(pla);  //pass is not allowed
  
  if(board.numPlaStonesOnBoard(getOpp(pla)) == 0)
    return pla;
  Hash128 h = board.pos_hash;
  assert(hist.posHashHistoryCount.count(h));
  if(hist.posHashHistoryCount.at(h) >= 2 || hist.moveHistory.size() > Board::MAX_ARR_SIZE * 100) {
    if(hist.moveHistory.size() > Board::MAX_ARR_SIZE * 100)
      cout << "Game too long without loop" << endl;
    int whiteScore = board.numPlaStonesOnBoard(C_WHITE) - board.numPlaStonesOnBoard(C_BLACK);
    if(whiteScore > 0)
      return C_WHITE;
    else if(whiteScore < 0)
      return C_BLACK;
    else
      return C_EMPTY;
  }


  return C_WALL;
}

GameLogic::ResultsBeforeNN::ResultsBeforeNN() {
  inited = false;
  winner = C_WALL;
  myOnlyLoc = Board::NULL_LOC;
}

void GameLogic::ResultsBeforeNN::init(const Board& board, const BoardHistory& hist, Color nextPlayer) {
  if(inited)
    return;
  inited = true;

  return;
}
