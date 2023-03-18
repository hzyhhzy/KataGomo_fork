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

void Board::setInitialStones() {
  for(int y = 0; y < y_size; y++)
    for(int x = 0; x < x_size; x++) {
      Color c = (x + y) % 2 == 1 ? C_BLACK : C_WHITE;
      setStone(Location::getLoc(0, 0, x_size), c);
    }
}

bool Board::isLegal(Loc loc, Player pla) const{
  if(pla != nextPla) {
    std::cout << "Error next player ";
    return false;
  }

  //regarding pass as a lose is often more convenient than regarding pass as illegal
  if(loc == Board::PASS_LOC)
    return true;

  if(!isOnBoard(loc))
    return false;

  if(stage == 0)  // choose a piece
  {
    if(colors[loc] == pla)
      return true;
    else
      return false;
  } 
  else if(stage == 1)  // place the piece
  {
    Loc chosenMove = midLocs[0];
    if(chosenMove == loc)
      return false;
    if(colors[loc] != getOpp(pla)) //for Clobber, only capture moves are legal
      return false;
    int disSqr = Location::euclideanDistanceSquared(chosenMove, loc, x_size);
    return disSqr <= 1; //for Clobber, only horizontal and vertical moves are legal
  }
  ASSERT_UNREACHABLE;
  return false;
}


// Plays the specified move, assuming it is legal.
void Board::playMoveAssumeLegal(Loc loc, Player pla) {
  if(pla != nextPla) {
    std::cout << "Error next player ";
  }

  if(loc == PASS_LOC) {
    pos_hash ^= ZOBRIST_STAGENUM_HASH[stage];
    stage = 0;
    pos_hash ^= ZOBRIST_STAGENUM_HASH[stage];

    setNextPlayer(getOpp(nextPla));

    return;
  }
  assert(isOnBoard(loc));

  Player opp = getOpp(pla);

  if(stage == 0)  // choose
  {
    stage = 1;
    pos_hash ^= ZOBRIST_STAGENUM_HASH[0];
    pos_hash ^= ZOBRIST_STAGENUM_HASH[1];

    midLocs[0] = loc;
    pos_hash ^= ZOBRIST_STAGELOC_HASH[loc][0];
  } else if(stage == 1)  // place
  {
    stage = 0;
    pos_hash ^= ZOBRIST_STAGENUM_HASH[1];
    pos_hash ^= ZOBRIST_STAGENUM_HASH[0];

    Loc chosenLoc = midLocs[0];
    setStone(chosenLoc, C_EMPTY);
    setStone(loc, pla);

    for(int i = 0; i < STAGE_NUM_EACH_PLA - 1; i++) {
      pos_hash ^= ZOBRIST_STAGELOC_HASH[midLocs[i]][i];
      midLocs[i] = Board::NULL_LOC;
    }
    setNextPlayer(getOpp(nextPla));
  } else
    ASSERT_UNREACHABLE;
}


GameLogic::MovePriority GameLogic::getMovePriorityAssumeLegal(const Board& board, const BoardHistory& hist, Player pla, Loc loc) {
  //if it's easy to judge whether one move will end the game and win, you can write the logics here, then use this function in resultsBeforeNN
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
  Loc loc,
  bool isLegalPass) {

  Player opp = getOpp(pla);
  if(isLegalPass) { 
    assert(loc == Board::PASS_LOC);
    //just continue
  } else if(loc == Board::PASS_LOC)
    return getOpp(pla);//illegal pass, lose
  
  Hash128 h = board.pos_hash;
  assert(hist.posHashHistoryCount.count(h));

  //loop
  if(hist.posHashHistoryCount.at(h) >= 2) {
    //the position repeats, maybe you should handle this situation
    return C_EMPTY;
  }

  // extremely long game, draw
  if(hist.moveHistory.size() > board.x_size * board.y_size * 30) {
    cout << "Game too long without loop" << endl;
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
  //if there is something need to be input to NN, write it here 
  if(inited)
    return;
  inited = true;

  return;
}
