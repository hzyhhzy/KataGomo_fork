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



Color GameLogic::checkWinnerAfterPlayed(
  const Board& board,
  const BoardHistory& hist,
  Player pla,
  Loc loc) {

  if(loc == Board::PASS_LOC)
    return getOpp(pla);  //pass is not allowed
  
  //write your own logic here
  for (int i = 0; i < 8; i++)
  {
    if(board.colors[loc + board.adj_offsets[i]] == pla)
      return getOpp(pla);
  }


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
