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

  if(board.whiteRemainScore == 0)
    return C_WHITE;
  if(board.blackRemainScore == 0)
    return C_BLACK;

  if(board.numStonesOnBoard() == board.x_size * board.y_size)
    return C_EMPTY;

  if(board.movenum >= 5 * board.x_size * board.y_size + 100) {
    cout << "very long game";
    return C_EMPTY;
  }


  return C_WALL;
}



static void markConnectedStonesRecursive( 
  std::vector<bool>& visited, // visited[x+y*x_size]
  const Board& board,
  Player pla,
  int x,
  int y,
  Loc locAssumed,
  Color colorAssumed, //assume the color of locAssumed is always colorAssumed, no matter what is it on the board. If not used, locAssumed=NULL_LOC
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
        visited, board, pla, x + dxs[i], y + dys[i], locAssumed, colorAssumed, minX, maxX, minY, maxY, stoneNum);



}

static bool isSymmetry(
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
      int x = minX + dx, y = minY + dy;
      int xs = maxX - dx, ys = minY + dy;
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
      int x = minX + dx, y = minY + dy;
      int xs = minX + dx, ys = maxY - dy;
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
      int x = minX + dx, y = minY + dy;
      int xs = minX + dy, ys = minY + dx;
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
      int x = minX + dx, y = maxY - dy;
      int xs = minX + dy, ys = maxY - dx;
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


int GameLogic::maybeCapture(Board& board, Player pla) {
  vector<bool> checked(board.x_size * board.y_size, false);

  int totalScore = 0;

  for(int y0 = 0; y0 < board.y_size; y0++) {
    for(int x0 = 0; x0 < board.x_size; x0++) {
      int pos0 = y0 * board.x_size + x0;
      int loc0 = Location::getLoc(x0, y0, board.x_size);
      if(board.colors[loc0] != pla || checked[pos0])
        continue;

      vector<bool> visited(board.x_size * board.y_size, false);
      int minX = x0, maxX = x0, minY = y0, maxY = y0, stoneNum = 0;
      markConnectedStonesRecursive(
        visited, board, pla, x0, y0, Board::NULL_LOC, C_EMPTY, minX, maxX, minY, maxY, stoneNum);

      for(int dy = 0; dy <= maxY - minY; dy++) {
        for(int dx = 0; dx <= maxX - minX; dx++) {
          int x = minX + dx, y = minY + dy;
          int pos = y * board.x_size + x;
          if(visited[pos]) {
            checked[pos] = true;
          }
        }
      }

      if(stoneNum >= MinCaptureNum && isSymmetry(visited, board.x_size, minX, maxX, minY, maxY)) {
        totalScore += stoneNum - CaptureNumMinusScore;
        for(int dy = 0; dy <= maxY - minY; dy++) {
          for(int dx = 0; dx <= maxX - minX; dx++) {
            int x = minX + dx, y = minY + dy;
            if(visited[y * board.x_size + x]) {
              Loc loc = Location::getLoc(x, y, board.x_size);
              board.setStone(loc, C_EMPTY);
            }
          }
        }
      }
    }
  }
  if(pla == C_BLACK) {
    board.pos_hash ^= Board::ZOBRIST_REMAIN_SCORE_BLACK_HASH[board.blackRemainScore];
    board.blackRemainScore -= totalScore;
    if(board.blackRemainScore < 0)
      board.blackRemainScore = 0;
    board.pos_hash ^= Board::ZOBRIST_REMAIN_SCORE_BLACK_HASH[board.blackRemainScore];
  } else if(pla == C_WHITE) {
    board.pos_hash ^= Board::ZOBRIST_REMAIN_SCORE_WHITE_HASH[board.whiteRemainScore];
    board.whiteRemainScore -= totalScore;
    if(board.whiteRemainScore < 0)
      board.whiteRemainScore = 0;
    board.pos_hash ^= Board::ZOBRIST_REMAIN_SCORE_WHITE_HASH[board.whiteRemainScore];
  }
  return totalScore;
}


int GameLogic::checkCaptureIfPlay(  // only check the surrounding of locAssumed
  const Board& board,
  Player pla,
  Loc loc)
{
  if(!board.isOnBoard(loc))
    return 0;
  if(board.colors[loc]==pla)//移子
  {
    int totalScore = 0;
    for(int i=0;i<8;i++) {
      Loc loc1 = loc + board.adj_offsets[i];
      if(board.isOnBoard(loc1) && board.colors[loc1] == pla) {
        vector<bool> visited(board.x_size * board.y_size, false);
        int x = Location::getX(loc1, board.x_size);
        int y = Location::getY(loc1, board.x_size);
        int minX = x, maxX = x, minY = y, maxY = y, stoneNum = 0;
        markConnectedStonesRecursive(visited, board, pla, x, y, loc, C_EMPTY, minX, maxX, minY, maxY, stoneNum);
        if(stoneNum >= MinCaptureNum && isSymmetry(visited, board.x_size, minX, maxX, minY, maxY))
          totalScore += stoneNum - CaptureNumMinusScore;
      }
    }
    return totalScore;

  } else { //落子或推子
    vector<bool> visited(board.x_size * board.y_size, false);
    int x = Location::getX(loc, board.x_size);
    int y = Location::getY(loc, board.x_size);
    int minX = x, maxX = x, minY = y, maxY = y, stoneNum = 0;
    markConnectedStonesRecursive(visited, board, pla, x, y, loc, pla, minX, maxX, minY, maxY, stoneNum);
    if(stoneNum >= MinCaptureNum && isSymmetry(visited, board.x_size, minX, maxX, minY, maxY))
      return stoneNum - CaptureNumMinusScore;
    else
      return 0;
  }

}