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


int GameLogic::checkCaptureIfPlay(const Board& board, Player pla, Loc loc) {
  vector<bool> visited(board.x_size * board.y_size, false);
  int x = Location::getX(loc, board.x_size);
  int y = Location::getY(loc, board.x_size);
  int minX = x, maxX = x, minY = y, maxY = y, stoneNum = 0;
  markConnectedStonesRecursive(visited, board, pla, x, y, true, minX, maxX, minY, maxY, stoneNum);
  if(stoneNum >= MinCaptureNum && isSymmetry(visited, board.x_size, minX, maxX, minY, maxY))
      return stoneNum;
  else
      return 0;
}

int GameLogic::maybeCapture(Board& board, Player pla, Loc loc) {
  assert(board.isOnBoard[loc]);
  assert(board.colors[loc] = pla);
  vector<bool> visited(board.x_size * board.y_size, false);
  int x0 = Location::getX(loc, board.x_size);
  int y0 = Location::getY(loc, board.x_size);
  int minX = x0, maxX = x0, minY = y0, maxY = y0, stoneNum = 0;
  markConnectedStonesRecursive(visited, board, pla, x0, y0, false, minX, maxX, minY, maxY, stoneNum);
  if(stoneNum < MinCaptureNum || (!isSymmetry(visited, board.x_size, minX, maxX, minY, maxY)))
      return 0;

  for(int dy = 0; dy <= maxY - minY; dy++) {
    for(int dx = 0; dx <= maxX - minX; dx++) {
      int x = minX + x0, y = minY + y0;
      if(visited[y * board.x_size + x]) {
        Loc loc = Location::getLoc(x, y, board.x_size);
        board.setStone(loc, C_EMPTY);
      }
    }
  }
  int score = stoneNum - CaptureNumMinusScore;
  if(pla == C_BLACK) {
    board.pos_hash ^= Board::ZOBRIST_REMAIN_SCORE_BLACK_HASH[board.blackRemainScore];
    board.blackRemainScore -= score;
    if(board.blackRemainScore < 0)
      board.blackRemainScore = 0;
    board.pos_hash ^= Board::ZOBRIST_REMAIN_SCORE_BLACK_HASH[board.blackRemainScore];
  }
  else if(pla == C_WHITE) {
    board.pos_hash ^= Board::ZOBRIST_REMAIN_SCORE_WHITE_HASH[board.whiteRemainScore];
    board.whiteRemainScore -= score;
    if(board.whiteRemainScore < 0)
      board.whiteRemainScore = 0;
    board.pos_hash ^= Board::ZOBRIST_REMAIN_SCORE_WHITE_HASH[board.whiteRemainScore];
  }
  return stoneNum;
}

void GameLogic::markConnectedStonesRecursive(
  std::vector<bool>& visited,
  const Board& board,
  Player pla,
  int x,
  int y,
  Loc locAssumed,
  Color colorAssumed, 
  int& minX,
  int& maxX,
  int& minY,
  int& maxY,
  int& stoneNum
  ) {
  if(x < 0 || x >= board.x_size || y < 0 || y >= board.y_size)
      return;
  int pos = y * board.x_size + x;
  if(visited[pos])
      return;
  Loc loc = Location::getLoc(x, y, board.x_size);
  Color color = locAssumed == loc ? colorAssumed : board.colors[loc];
  if(color!= pla)
      return;
  visited[pos] = true;
  stoneNum += 1;
  if(x < minX)
      minX = x;
  if(x > maxX)
      maxX = x;
  if(y < minY)
      minY = y;
  if(y > maxY)
      maxY = y;
  const int dxs[8] = {- 1, 0, 1, -1, 1, -1, 0, 1};
  const int dys[8] = {-1, -1, -1, 0, 0, 1, 1, 1};
  for(int i = 0; i < 8; i++)
      markConnectedStonesRecursive(
        visited, board, pla, x + dxs[i], y + dys[i], false, minX, maxX, minY, maxY, stoneNum);



}

bool GameLogic::isSymmetry(
  std::vector<bool>& stones,
  int x_size,
  int minX,
  int maxX,
  int minY,
  int maxY) {
  
  bool sym;
  
  //x symmetry
  sym = true;
  for(int dy = 0; dy <= maxY - minY; dy++) {
    for(int dx = 0; dx <= maxX - minX; dx++) {
      int x = minX + x, y = minY + y;
      int xs = maxX - x, ys = minY + y;
      if(stones[y * x_size + x] ^ stones[ys * x_size + xs]) {
        sym = false;
        break;
      }
    }
    if(!sym)
      break;
  }
  if(sym)
    return true;

  
  // y symmetry
  sym = true;
  for(int dy = 0; dy <= maxY - minY; dy++) {
    for(int dx = 0; dx <= maxX - minX; dx++) {
      int x = minX + x, y = minY + y;
      int xs = minX + x, ys = maxY - y;
      if(stones[y * x_size + x] ^ stones[ys * x_size + xs]) {
        sym = false;
        break;
      }
    }
    if(!sym)
      break;
  }
  if(sym)
    return true;


  if(maxX - minX != maxY - minY)
    return false;

  
  // +x+y symmetry
  sym = true;
  for(int dy = 0; dy <= maxY - minY; dy++) {
    for(int dx = 0; dx <= maxX - minX; dx++) {
      int x = minX + x, y = minY + y;
      int xs = minX + y, ys = minY + x;
      if(stones[y * x_size + x] ^ stones[ys * x_size + xs]) {
        sym = false;
        break;
      }
    }
    if(!sym)
      break;
  }
  if(sym)
    return true;

  // +x-y symmetry
  sym = true;
  for(int dy = 0; dy <= maxY - minY; dy++) {
    for(int dx = 0; dx <= maxX - minX; dx++) {
      int x = minX + x, y = maxY - y;
      int xs = minX + y, ys = maxY - x;
      if(stones[y * x_size + x] ^ stones[ys * x_size + xs]) {
        sym = false;
        break;
      }
    }
    if(!sym)
      break;
  }
  if(sym)
    return true;

  return false;

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
