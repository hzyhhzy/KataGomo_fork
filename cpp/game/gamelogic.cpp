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




bool GameLogic::isLegal(const Board& board, Player pla, Loc loc) {
  if(pla != board.nextPla) {
    std::cout << "Error next player ";
    return false;
  }

  if(loc == Board::PASS_LOC)  // pass is lose, but not illegal
    return true;

  if(!board.isOnBoard(loc))
    return false;

  assert(board.stage == 0);  // choose a piece

  return board.colors[BOARD_LAYERS - 1][loc] == C_EMPTY;
   
}

static bool isFour(const Board& board, Player pla, Loc loc, bool alreadyPlayed)
{
  //static_assert(BOARD_LAYERS == 4 || BOARD_LAYERS == 5, "");
  const int s = BOARD_LAYERS - 1;
  assert(board.x_size == BOARD_LAYERS && board.y_size == BOARD_LAYERS);
  int h0 = 0;
  for (h0 = 0; h0 < BOARD_LAYERS; h0++)
  {
    if(board.colors[h0][loc] == C_EMPTY)
      break;
  }
  if(alreadyPlayed)
    h0 -= 1;
  assert(h0 >= 0 && h0 < BOARD_LAYERS);
  int x0 = Location::getX(loc, board.x_size);
  int y0 = Location::getY(loc, board.x_size);

  bool hasFour = true;
  //x line
  hasFour = true;
  for(int d = 0; d < BOARD_LAYERS; d++) {
    int x = d;
    int y = y0;
    int h = h0;
    Loc loc1 = Location::getLoc(x, y, board.x_size);
    if(x != x0 && board.colors[h][loc1] != pla)
      hasFour = false;
  }
  if(hasFour)
    return true;
  
  //y line
  hasFour = true;
  for(int d = 0; d < BOARD_LAYERS; d++) {
    int x = x0;
    int y = d;
    int h = h0;
    Loc loc1 = Location::getLoc(x, y, board.x_size);
    if(y != y0 && board.colors[h][loc1] != pla)
      hasFour = false;
  }
  if(hasFour)
    return true;

  // h line
  hasFour = true;
  for(int d = 0; d < BOARD_LAYERS; d++) {
    int x = x0;
    int y = y0;
    int h = d;
    Loc loc1 = Location::getLoc(x, y, board.x_size);
    if(h != h0 && board.colors[h][loc1] != pla)
      hasFour = false;
  }
  if(hasFour)
    return true;


  // +x+y line
  if(x0 == y0) {
    hasFour = true;
    for(int d = 0; d < BOARD_LAYERS; d++) {
      int x = d;
      int y = d;
      int h = h0;
      Loc loc1 = Location::getLoc(x, y, board.x_size);
      if(x != x0 && board.colors[h][loc1] != pla)
        hasFour = false;
    }
    if(hasFour)
      return true;
  }

  // +x-y line
  if(x0 == s - y0) {
    hasFour = true;
    for(int d = 0; d < BOARD_LAYERS; d++) {
      int x = s - d;
      int y = d;
      int h = h0;
      Loc loc1 = Location::getLoc(x, y, board.x_size);
      if(x != x0 && board.colors[h][loc1] != pla)
        hasFour = false;
    }
    if(hasFour)
      return true;
  }

  // +x+h line
  if(x0 == h0) {
    hasFour = true;
    for(int d = 0; d < BOARD_LAYERS; d++) {
      int x = d;
      int y = y0;
      int h = d;
      Loc loc1 = Location::getLoc(x, y, board.x_size);
      if(x != x0 && board.colors[h][loc1] != pla)
        hasFour = false;
    }
    if(hasFour)
      return true;
  }

  // +x-h line
  if(x0 == s - h0) {
    hasFour = true;
    for(int d = 0; d < BOARD_LAYERS; d++) {
      int x = s - d;
      int y = y0;
      int h = d;
      Loc loc1 = Location::getLoc(x, y, board.x_size);
      if(x != x0 && board.colors[h][loc1] != pla)
        hasFour = false;
    }
    if(hasFour)
      return true;
  }

  // +y+h line
  if(y0 == h0) {
    hasFour = true;
    for(int d = 0; d < BOARD_LAYERS; d++) {
      int x = x0;
      int y = d;
      int h = d;
      Loc loc1 = Location::getLoc(x, y, board.x_size);
      if(y != y0 && board.colors[h][loc1] != pla)
        hasFour = false;
    }
    if(hasFour)
      return true;
  }

  // +y-h line
  if(y0 == s - h0) {
    hasFour = true;
    for(int d = 0; d < BOARD_LAYERS; d++) {
      int x = x0;
      int y = s - d;
      int h = d;
      Loc loc1 = Location::getLoc(x, y, board.x_size);
      if(y != y0 && board.colors[h][loc1] != pla)
        hasFour = false;
    }
    if(hasFour)
      return true;
  }

  // +x+y+h line
  if(x0 == y0 && y0 == h0) {
    hasFour = true;
    for(int d = 0; d < BOARD_LAYERS; d++) {
      int x = d;
      int y = d;
      int h = d;
      Loc loc1 = Location::getLoc(x, y, board.x_size);
      if(x != x0 && board.colors[h][loc1] != pla)
        hasFour = false;
    }
    if(hasFour)
      return true;
  }
  // +x+y-h line
  if(x0 == y0 && y0 == s - h0) {
    hasFour = true;
    for(int d = 0; d < BOARD_LAYERS; d++) {
      int x = d;
      int y = d;
      int h = s - d;
      Loc loc1 = Location::getLoc(x, y, board.x_size);
      if(x != x0 && board.colors[h][loc1] != pla)
        hasFour = false;
    }
    if(hasFour)
      return true;
  }

  // +x-y+h line
  if(x0 == s - y0 && y0 == s - h0) {
    hasFour = true;
    for(int d = 0; d < BOARD_LAYERS; d++) {
      int x = d;
      int y = s - d;
      int h = d;
      Loc loc1 = Location::getLoc(x, y, board.x_size);
      if(x != x0 && board.colors[h][loc1] != pla)
        hasFour = false;
    }
    if(hasFour)
      return true;
  }

  // +x-y-h line
  if(x0 == s - y0 && y0 == h0) {
    hasFour = true;
    for(int d = 0; d < BOARD_LAYERS; d++) {
      int x = d;
      int y = s - d;
      int h = s - d;
      Loc loc1 = Location::getLoc(x, y, board.x_size);
      if(x != x0 && board.colors[h][loc1] != pla)
        hasFour = false;
    }
    if(hasFour)
      return true;
  }
  return false;
}


GameLogic::MovePriority GameLogic::getMovePriorityAssumeLegal(const Board& board, const BoardHistory& hist, Player pla, Loc loc) {
  if(loc == Board::PASS_LOC)
    return MP_NORMAL;

  if(isFour(board, pla, loc, false))
    return MP_SUDDEN_WIN;

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
  
  if(isFour(board, pla, loc, true))
    return pla;

  if(board.numStonesOnBoard() >= board.x_size * board.y_size * BOARD_LAYERS)
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
