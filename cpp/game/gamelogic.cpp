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

//https://ieeexplore.ieee.org/abstract/document/9618991
static bool checkBlackWinAsIfPlayed(
  const Board& board,
  Loc loc0) {
  if(board.y_size != FIXED_BOARD_YSIZE || board.x_size < 4)
    throw StringError("this rule is not defined for height!=4 or width<4 boards");
  int xs = board.x_size;
  int x0 = Location::getX(loc0, xs);
  int y0 = Location::getY(loc0, xs);
  //horizontal lines
  {
    //left side
    if (x0 <= 3)
    {
      bool allBlack = true;
      for (int i = 0; i < 4; i++)
      {
        Loc loc = Location::getLoc(i, y0, xs);
        if(!board.isOnBoard(loc))
          throw StringError("bug in checkBlackWinAsIfPlayed");
        bool isBlack = (loc == loc0) || board.colors[loc] == C_BLACK; //as if black has played
        if (!isBlack)
        {
          allBlack = false;
          break;
        }
      }
      if(allBlack)
        return true;
    }
    // right side
    if(x0 >= xs - 4) {
      bool allBlack = true;
      for(int i = 0; i < 4; i++) {
        Loc loc = Location::getLoc(xs - 1 - i, y0, xs);
        if(!board.isOnBoard(loc))
          throw StringError("bug in checkBlackWinAsIfPlayed");
        bool isBlack = (loc == loc0) || board.colors[loc] == C_BLACK;  // as if black has played
        if(!isBlack) {
          allBlack = false;
          break;
        }
      }
      if(allBlack)
        return true;
    }
    // middle length 7
    for (int i = 0; i < 7; i++)
    {
      int xa = x0 - i, xb = x0 - i + 6;
      if(xa < 1)
        continue;
      if(xb >= xs - 1)
        continue;

      bool allBlack = true;
      for (int x = xa; x <= xb; x++) {
        Loc loc = Location::getLoc(x, y0, xs);
        if(!board.isOnBoard(loc))
          throw StringError("bug in checkBlackWinAsIfPlayed");
        bool isBlack = (loc == loc0) || board.colors[loc] == C_BLACK;  // as if black has played
        if(!isBlack) {
          allBlack = false;
          break;
        }
      }
      if(allBlack)
        return true;
    }
  }
   //vertical lines
  {
    bool allBlack = true;
    for(int y = 0; y < 4; y++) {
      Loc loc = Location::getLoc(x0, y, xs);
      if(!board.isOnBoard(loc))
        throw StringError("bug in checkBlackWinAsIfPlayed");
      bool isBlack = (loc == loc0) || board.colors[loc] == C_BLACK;  // as if black has played
      if(!isBlack) {
        allBlack = false;
        break;
      }
    }
    if(allBlack)
      return true;
  }
  // diagonal lines +x+y
  {
    if(x0 >= y0 && xs - x0 >= 4 - y0) {  // normal length-4 lines
      bool allBlack = true;
      for(int y = 0; y < 4; y++) {
        Loc loc = Location::getLoc(x0 - y0 + y, y, xs);
        if(!board.isOnBoard(loc))
          throw StringError("bug in checkBlackWinAsIfPlayed");
        bool isBlack = (loc == loc0) || board.colors[loc] == C_BLACK;  // as if black has played
        if(!isBlack) {
          allBlack = false;
          break;
        }
      }
      if(allBlack)
        return true;
    } 
    else if(y0 - x0 == 1)  // extra
    {
      bool allBlack = true;
      for(int y = 1; y < 4; y++) {
        Loc loc = Location::getLoc(x0 - y0 + y, y, xs);
        if(!board.isOnBoard(loc))
          throw StringError("bug in checkBlackWinAsIfPlayed");
        bool isBlack = (loc == loc0) || board.colors[loc] == C_BLACK;  // as if black has played
        if(!isBlack) {
          allBlack = false;
          break;
        }
      }
      if(allBlack)
        return true;
    } 
    else if(xs - x0 == 4 - y0 - 1)  // extra
    {
      bool allBlack = true;
      for(int y = 0; y < 3; y++) {
        Loc loc = Location::getLoc(x0 - y0 + y, y, xs);
        if(!board.isOnBoard(loc))
          throw StringError("bug in checkBlackWinAsIfPlayed");
        bool isBlack = (loc == loc0) || board.colors[loc] == C_BLACK;  // as if black has played
        if(!isBlack) {
          allBlack = false;
          break;
        }
      }
      if(allBlack)
        return true;
    } 
    else if(xs - x0 == 4 - y0 - 2)  // extra
    {
      bool allBlack = true;
      for(int y = 0; y < 2; y++) {
        Loc loc = Location::getLoc(x0 - y0 + y, y, xs);
        if(!board.isOnBoard(loc))
          throw StringError("bug in checkBlackWinAsIfPlayed");
        bool isBlack = (loc == loc0) || board.colors[loc] == C_BLACK;  // as if black has played
        if(!isBlack) {
          allBlack = false;
          break;
        }
      }
      if(allBlack)
        return true;
    }
  }
  // diagonal lines -x+y
  {
    if(xs - 1 - x0 >= y0 && 1 + x0 >= 4 - y0) {  // normal length-4 lines
      bool allBlack = true;
      for(int y = 0; y < 4; y++) {
        Loc loc = Location::getLoc(x0 + y0 - y, y, xs);
        if(!board.isOnBoard(loc))
          throw StringError("bug in checkBlackWinAsIfPlayed");
        bool isBlack = (loc == loc0) || board.colors[loc] == C_BLACK;  // as if black has played
        if(!isBlack) {
          allBlack = false;
          break;
        }
      }
      if(allBlack)
        return true;
    } 
    else if(y0 - xs + 1 + x0 == 1)  // extra
    {
      bool allBlack = true;
      for(int y = 1; y < 4; y++) {
        Loc loc = Location::getLoc(x0 + y0 - y, y, xs);
        if(!board.isOnBoard(loc))
          throw StringError("bug in checkBlackWinAsIfPlayed");
        bool isBlack = (loc == loc0) || board.colors[loc] == C_BLACK;  // as if black has played
        if(!isBlack) {
          allBlack = false;
          break;
        }
      }
      if(allBlack)
        return true;
    } 
    else if(1 + x0 == 4 - y0 - 1)  // extra
    {
      bool allBlack = true;
      for(int y = 0; y < 3; y++) {
        Loc loc = Location::getLoc(x0 + y0 - y, y, xs);
        if(!board.isOnBoard(loc))
          throw StringError("bug in checkBlackWinAsIfPlayed");
        bool isBlack = (loc == loc0) || board.colors[loc] == C_BLACK;  // as if black has played
        if(!isBlack) {
          allBlack = false;
          break;
        }
      }
      if(allBlack)
        return true;
    } 
    else if(1 + x0 == 4 - y0 - 2)  // extra
    {
      bool allBlack = true;
      for(int y = 0; y < 2; y++) {
        Loc loc = Location::getLoc(x0 + y0 - y, y, xs);
        if(!board.isOnBoard(loc))
          throw StringError("bug in checkBlackWinAsIfPlayed");
        bool isBlack = (loc == loc0) || board.colors[loc] == C_BLACK;  // as if black has played
        if(!isBlack) {
          allBlack = false;
          break;
        }
      }
      if(allBlack)
        return true;
    }
  }
  return false;
}


GameLogic::MovePriority GameLogic::getMovePriorityAssumeLegal(const Board& board, const BoardHistory& hist, Player pla, Loc loc) {
  if(loc == Board::PASS_LOC)
    return MP_NORMAL;
  MovePriority MP = MP_NORMAL;

  if(checkBlackWinAsIfPlayed(board, loc))
    if(pla == C_BLACK)
      return GameLogic::MP_SUDDEN_WIN;
    else
      return GameLogic::MP_ONLY_NONLOSE_MOVES;

  return MP;
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
  
  if(getMovePriorityAssumeLegal(board, hist, pla, loc) == MP_SUDDEN_WIN)
    return pla;

  if (board.movenum >= board.x_size * board.y_size)
  {
    return C_WHITE;
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


  // check five and four
  bool oppHasFour = false;
  Loc myLifeFourLoc = Board::NULL_LOC;
  for(int x = 0; x < board.x_size; x++)
    for(int y = 0; y < board.y_size; y++) {
      Loc loc = Location::getLoc(x, y, board.x_size);
      MovePriority mp = getMovePriority(board, hist, nextPlayer, loc);
      if(mp == MP_SUDDEN_WIN) {
        winner = nextPlayer;
        myOnlyLoc = loc;
        return;
      } else if(mp == MP_ONLY_NONLOSE_MOVES) {
        oppHasFour = true;
        myOnlyLoc = loc;
      }
    }




  return;
}
