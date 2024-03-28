#include "../game/gamelogic.h"
#include "../game/board.h"

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




// Check if moving here is illegal.
bool Board::isLegal(Loc loc, Player pla) const {
  if(pla != nextPla) {
    std::cout << "Error next player ";
    return false;
  }

  if(loc == Board::PASS_LOC)  // pass is lose, but not illegal
    return true;

  if(!isOnBoard(loc))
    return false;

  if(colors[loc] != C_EMPTY)
    return false;

  int x = Location::getX(loc, x_size);
  int y = Location::getY(loc, x_size);
  if((x + y) % 2 == 0)
    return false;//node or area
  return true;
}


bool Board::isSurrounded(Loc loc) const {
  if(!isOnBoard(loc))
    return false;
  for (int i = 0; i < 4; i++)
  {
    Loc l = loc + adj_offsets[i];
    assert(isOnBoard(l));
    if(colors[l] != C_BLACK)
      return false;
  }
  return true;
}

// Plays the specified move, assuming it is legal.
void Board::playMoveAssumeLegal(Loc loc, Player pla) {
  if(pla != nextPla) {
    std::cout << "Error next player ";
  }

  pos_hash ^= ZOBRIST_MOVENUM_HASH[movenum];
  movenum++;
  pos_hash ^= ZOBRIST_MOVENUM_HASH[movenum];

  if(!isOnBoard(loc))
    return;
  setStone(loc, C_BLACK);
  int x = Location::getX(loc, x_size);
  int y = Location::getY(loc, x_size);

  int newAreaNum = 0;
  if(x % 2 == 0 && y % 2 == 1)  // vertical line
  {
    newAreaNum += isSurrounded(loc + adj_offsets[1]);
    newAreaNum += isSurrounded(loc + adj_offsets[2]);
  }
  else if(x % 2 == 1 && y % 2 == 0)  // horizontal line
  {
    newAreaNum += isSurrounded(loc + adj_offsets[0]);
    newAreaNum += isSurrounded(loc + adj_offsets[3]);
  } 
  else
    ASSERT_UNREACHABLE;



  if(newAreaNum == 0)  
  {
    nextPla = getOpp(nextPla);
    pos_hash ^= ZOBRIST_NEXTPLA_HASH[getOpp(nextPla)];
    pos_hash ^= ZOBRIST_NEXTPLA_HASH[nextPla];
  } 
  else //continue playing
  {
    int scoreChange = pla == P_BLACK ? newAreaNum : -newAreaNum;
    setScore(currentScoreBlackMinusWhite + scoreChange);
  }
}



Color GameLogic::checkWinnerAfterPlayed(
  const Board& board,
  const BoardHistory& hist,
  Player pla,
  Loc loc) {
  if(loc == Board::PASS_LOC)
    return getOpp(pla);  //pass is not allowed
  
  int stoneNum = board.numStonesOnBoard();
  if (stoneNum == (board.x_size * board.y_size - 1) / 2)
  {
    int finalScore = board.currentScoreBlackMinusWhite - board.komi;
    if(finalScore > 0)
      return C_BLACK;
    if(finalScore < 0)
      return C_WHITE;
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
