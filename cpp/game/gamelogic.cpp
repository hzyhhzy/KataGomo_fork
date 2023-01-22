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

  if(board.stage == 0)  // choose a piece
  {
    return board.colors[loc] == pla;
  } 
  else if(board.stage == 1)  // place the piece
  {
    Color c = board.colors[loc];
    Color opp = getOpp(pla);
    Loc chosenMove = board.midLocs[0];
    int x0 = Location::getX(chosenMove, board.x_size);
    int y0 = Location::getY(chosenMove, board.x_size);
    int x1 = Location::getX(loc, board.x_size);
    int y1 = Location::getY(loc, board.x_size);
    int dy = y1 - y0;
    int dx = x1 - x0;
    if(!((pla == C_BLACK && dy == -1) || (pla == C_WHITE && dy == 1)))
      return false;
    if(dx == 1 || dx == -1)
      return c == opp || c == C_EMPTY;
    else if(dx == 0)
      return c == C_EMPTY;
    else
      return false;
  }
  ASSERT_UNREACHABLE;
  return false;
}

GameLogic::MovePriority GameLogic::getMovePriorityAssumeLegal(const Board& board, const BoardHistory& hist, Player pla, Loc loc) {
  if(loc == Board::PASS_LOC)
    return MP_NORMAL;

  int y = Location::getY(loc, board.x_size);
  if(board.stage == 0) {
    if((pla == C_BLACK && y == 1) || (pla == C_WHITE && y == board.y_size - 2))
      return MP_WINNING;
  }
  else if(board.stage == 1) {
    if((pla == C_BLACK && y == 0) || (pla == C_WHITE && y == board.y_size - 1))
      return MP_SUDDEN_WIN;
  }


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
  
  
  int y = Location::getY(loc, board.x_size);
  if((pla == C_BLACK && y == 0) || (pla == C_WHITE && y == board.y_size - 1))
      return pla;


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
