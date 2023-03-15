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

  if(loc != Board::PASS_LOC && (!board.isOnBoard(loc)))
    return false;

  if(board.stage == 0)  // choose or copy a piece
  {
    if(loc==Board::PASS_LOC) {
      return !hasLegalMoveAssumeStage0(board);
    }
    if(board.colors[loc] == pla)
      return hasLegalMoveAssumeStage1(board, loc);
    else if(board.colors[loc] == C_EMPTY) {
      for(int i = 0; i < 8; i++)
        if(board.colors[loc + board.adj_offsets[i]] == pla)
          return true;
      return false;
    } else
      return false;
  } 
  else if(board.stage == 1)  // place the piece
  {
    if(loc == Board::PASS_LOC) {
      return !hasLegalMoveAssumeStage1(board, board.midLocs[0]);
    }
    Loc chosenMove = board.midLocs[0];
    if(board.colors[loc] != C_EMPTY)
      return false;
    int disSqr = Location::euclideanDistanceSquared(chosenMove, loc, board.x_size);
    return disSqr <= 8 && disSqr >= 4;
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

bool GameLogic::hasLegalMoveAssumeStage0(const Board& board) {
  if(board.stage == 1)
    return true;//pass is never allowed in stage 1
  assert(board.stage == 0);
  Color pla = board.nextPla;

  for(int y = 0; y < board.y_size; y++) {
    for(int x = 0; x < board.x_size; x++) {
      Loc loc = Location::getLoc(x, y, board.x_size);
      if(board.colors[loc]==C_EMPTY) {
        for(int dy = -2; dy <= 2; dy++) {
          for(int dx = -2; dx <= 2; dx++) {
            int x1 = x + dx;
            int y1 = y + dy;
            if(x1 < 0 || x1 >= board.x_size || y1 < 0 || y1 >= board.y_size)
              continue;
            Loc loc1 = Location::getLoc(x1, y1, board.x_size);
            if(board.colors[loc1] == pla)
              return true;
          }
        }
      }
    }
  }
  return false;

}

bool GameLogic::hasLegalMoveAssumeStage1(const Board& board, Loc chosenLoc) {
  if(!board.isOnBoard(chosenLoc))
    chosenLoc = board.midLocs[0];
  if(!board.isOnBoard(chosenLoc))
    return false;
  int x = Location::getX(chosenLoc, board.x_size);
  int y = Location::getY(chosenLoc, board.x_size);
  // find place to jump
  for(int dy = -2; dy <= 2; dy++) {
    for(int dx = -2; dx <= 2; dx++) {
      if(dx * dx + dy * dy <= 2)
        continue;
      int x1 = x + dx;
      int y1 = y + dy;
      if(x1 < 0 || x1 >= board.x_size || y1 < 0 || y1 >= board.y_size)
        continue;
      Loc loc1 = Location::getLoc(x1, y1, board.x_size);
      if(board.colors[loc1] == C_EMPTY)
        return true;
    }
  }
  return false;
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
    assert(board.stage == 0);

    if(
      hist.rules.loopPassRule == Rules::LOOPDRAW_PASSSCORING ||
      hist.rules.loopPassRule == Rules::LOOPLOSE_PASSSCORING ||
      hist.rules.loopPassRule == Rules::LOOPSCORING_PASSSCORING) {
      int myScore = 2 * board.numPlaStonesOnBoard(pla) - board.boardArea();
      if(pla == C_BLACK)
        myScore -= hist.rules.komi;
      else
        myScore += hist.rules.komi;

      if(myScore > 0)
        return pla;
      else if(myScore < 0)
        return opp;
      else
        return C_EMPTY;
    } else if(hist.rules.loopPassRule == Rules::LOOPDRAW_PASSCONTINUE) {
      bool hasLegalMoveNextMove = hasLegalMoveAssumeStage0(board);
      if(!hasLegalMoveNextMove) {  // double pass, normal ending
        int whiteScore = board.numPlaStonesOnBoard(C_WHITE) - board.numPlaStonesOnBoard(C_BLACK);
        if(whiteScore > 0)
          return C_WHITE;
        else if(whiteScore < 0)
          return C_BLACK;
        else
          return C_EMPTY;
      }
    } else
      ASSERT_UNREACHABLE;
  } else if(loc == Board::PASS_LOC)
    return getOpp(pla);//illegal pass
  
  if(board.numPlaStonesOnBoard(getOpp(pla)) == 0)
    return pla;
  Hash128 h = board.pos_hash;
  assert(hist.posHashHistoryCount.count(h));

  //loop
  if(hist.posHashHistoryCount.at(h) >= 2) {
    if(
      hist.rules.loopPassRule == Rules::LOOPDRAW_PASSSCORING ||
      hist.rules.loopPassRule == Rules::LOOPDRAW_PASSCONTINUE) {
      return C_EMPTY;
    }
    else if(
      hist.rules.loopPassRule == Rules::LOOPLOSE_PASSSCORING) {
      return opp;
    }
    else if(hist.rules.loopPassRule == Rules::LOOPSCORING_PASSSCORING) {
      int myScore = 2 * board.numPlaStonesOnBoard(pla) - -board.boardArea();
      if(pla == C_BLACK)
        myScore -= hist.rules.komi;
      else
        myScore += hist.rules.komi;

      if(myScore > 0)
        return pla;
      else if(myScore < 0)
        return opp;
      else
        return C_EMPTY;
    }
    else ASSERT_UNREACHABLE;
  }

  // extremely long game, draw
  if(hist.moveHistory.size() > Board::MAX_ARR_SIZE * 100) {
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
  if(inited)
    return;
  inited = true;

  return;
}
