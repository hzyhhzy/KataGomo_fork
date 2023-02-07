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

//Is src to dst a Queen's Move ?
static bool isQueenMove(const Board& board, Loc src, Loc dst) {
  int x0 = Location::getX(src, board.x_size);
  int y0 = Location::getY(src, board.x_size);
  int x1 = Location::getX(dst, board.x_size);
  int y1 = Location::getY(dst, board.x_size);
  int dx = x1 - x0;
  int dy = y1 - y0;
  if(dx != 0 && dy != 0 && dx != dy && dx != -dy)  //Not straight or diagonal
    return false;

  dx = dx > 0 ? 1 : dx < 0 ? -1 : 0;
  dy = dy > 0 ? 1 : dy < 0 ? -1 : 0;
  //现在dx和dy是走一格

  assert(dx != 0 || dy != 0);

  int count = 0;
  for(int i = 1;; i++) {
    Loc loc = Location::getLoc(x0 + i * dx, y0 + i * dy, board.x_size);
    assert(board.isOnBoard(loc));
    if(loc == dst)
      return true;
    if(board.colors[loc] != C_EMPTY)
      return false;
  }
  ASSERT_UNREACHABLE;
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

  Player opp = getOpp(pla);
  if(board.stage == 0)  // choose a piece
  {
    return board.colors[loc] == C_EMPTY || board.colors[loc] == opp;
  } 
  else if(board.stage == 1)  // place the piece
  {
    if(board.colors[loc] != C_EMPTY)
      return false;
    Color opp = getOpp(pla);
    Loc chosenMove = board.midLocs[0];
    assert(board.colors[chosenMove] == opp);
    return isQueenMove(board, chosenMove, loc);
  }
  ASSERT_UNREACHABLE;
  return false;
}

static int connectionLengthOneDirection(
  const Board& board,
  const BoardHistory& hist,
  Player pla,
  Loc loc,
  short adj,
  bool& isLife) {
  (void)hist;
  Loc tmploc = loc;
  int conNum = 0;
  isLife = false;
  while(1) {
    tmploc += adj;
    if(!board.isOnBoard(tmploc))
      break;
    if(board.colors[tmploc] == pla)
      conNum++;
    else if(board.colors[tmploc] == C_EMPTY) {
      isLife = true;
      break;
    } else
      break;
  }
  return conNum;
}

static bool isFiveIfPlayedOneDirection(const Board& board, const BoardHistory& hist, Player pla, Loc loc, int adj) {
  Player opp = getOpp(pla);
  bool isMyLife1, isMyLife2;
  int myConNum = connectionLengthOneDirection(board, hist, pla, loc, adj, isMyLife1) +
                 connectionLengthOneDirection(board, hist, pla, loc, -adj, isMyLife2) + 1;
  if(myConNum >= 5)
    return true;


  return false;
}

static bool isFiveIfPlayed(const Board& board, const BoardHistory& hist, Player pla, Loc loc) {
  if(loc == Board::PASS_LOC)
    return false;

  int adjs[4] = {1, (board.x_size + 1), (board.x_size + 1) + 1, (board.x_size + 1) - 1};  // +x +y +x+y -x+y
  for(int i = 0; i < 4; i++) {
    if(isFiveIfPlayedOneDirection(board, hist, pla, loc, adjs[i]))
      return true;
  }

  return false;
}



GameLogic::MovePriority GameLogic::getMovePriority(const Board& board, const BoardHistory& hist, Player pla, Loc loc) {
  if(loc == Board::PASS_LOC)
    return MP_NORMAL;
  if(!board.isLegal(loc, pla))
    return MP_ILLEGAL;
  if(board.stage==1)
    if(isFiveIfPlayed(board, hist, getOpp(pla), loc))
      return MP_SUDDEN_WIN;
  return MP_NORMAL;
}




Color GameLogic::checkWinnerAfterPlayed(
  const Board& board,
  const BoardHistory& hist,
  Player pla,
  Loc loc) {
  if(loc == Board::PASS_LOC)
    return getOpp(pla);  //pass is not allowed
  
  Color maybeFiveColor = board.colors[loc];
  assert(maybeFiveColor != C_EMPTY);
  if(isFiveIfPlayed(board, hist, maybeFiveColor, loc))
    return getOpp(maybeFiveColor);


  if(board.numStonesOnBoard() == board.x_size * board.y_size)
    return C_EMPTY;


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


  for(int x = 0; x < board.x_size; x++)
    for(int y = 0; y < board.y_size; y++) {
      Loc loc = Location::getLoc(x, y, board.x_size);
      MovePriority mp = getMovePriority(board, hist, nextPlayer, loc);
      if(mp == MP_SUDDEN_WIN || mp == MP_WINNING) {
        winner = nextPlayer;
        myOnlyLoc = loc;
        return;
      }
    }



  return;
}
