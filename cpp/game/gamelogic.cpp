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


static int connectionLengthOneDirection(const Board& board, Player pla, bool isLongWin, Loc loc, short adj) {
  Loc tmploc = loc;
  int conNum = 0;
  while(1) {
    tmploc += adj;
    if(!board.isOnBoard(tmploc))
      break;
    if(board.colors[tmploc] == pla)
      conNum++;
    else if(board.colors[tmploc] == C_EMPTY) {
      break;
    } else
      break;
  }
  return conNum;
}

static bool isFourOneDirection(
  const Board& board,
  Player pla, bool isLongWin,
  Loc loc,
  int adj) {
  int conNum = connectionLengthOneDirection(board, pla, isLongWin, loc, adj) +
                 connectionLengthOneDirection(board, pla, isLongWin, loc, -adj) + 1;
  if(conNum == 4 || (conNum > 4 && isLongWin))
    return true;

  return false;
}

static bool isFour(const Board& board, Player pla, bool isLongWin, Loc loc) {
  int adjs[4] = {1, (board.x_size + 1), (board.x_size + 1) + 1, (board.x_size + 1) - 1};  // +x +y +x+y -x+y
  for(int i = 0; i < 4; i++)
    if(isFourOneDirection(board, pla, LONGWIN, loc, adjs[i]))
      return true;
  return false;
}

Color GameLogic::checkWinnerAfterPlayed(
  const Board& board,
  const BoardHistory& hist,
  Player pla,
  Loc loc) {

  if(loc == Board::PASS_LOC)
    return getOpp(pla);  //pass is not allowed
  if(isFour(board, pla, LONGWIN, loc))
    return pla;


  if(board.movenum >= board.x_size * board.y_size)
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
  //write your own logic

  return;
}

int8_t Board::movePriority(Loc loc, Player pla) const {
  if(!isLegal(loc, pla))
    return -2;
  if(loc == PASS_LOC)
    return -1;
  if(isFour(*this, pla, LONGWIN, loc))
    return 2;
  if(isFour(*this, getOpp(pla), LONGWIN, loc))
    return 1;
  return 0;
}