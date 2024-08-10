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

int Board::findFour(Color color, Loc& loc1, Loc& loc2) const {

  int bestConnection = 0;
  loc1 = NULL_LOC;
  loc2 = NULL_LOC;

  auto checkOne = [&](Loc loc0, int16_t adj) -> int {
    Loc emptyloc1 = Board::NULL_LOC, emptyloc2 = Board::NULL_LOC;
    int emptyCount = 0;
    for (int i = 0; i < 6; i++)
    {
      Loc loc = loc0 + i * adj;
      assert(isOnBoard(loc));
      Color c = loc == firstLoc ? nextPla : colors[loc];
      if(c == getOpp(color))
        return 0;
      else if (c == C_EMPTY)
      {
        emptyCount += 1;
        if(emptyCount == 1) {
          emptyloc1 = loc;
        } else if(emptyCount == 2) {
          emptyloc2 = loc;
        } else
          return 0;
      }
      else if (c == color)
      {

      } 
      else
        ASSERT_UNREACHABLE;
    }
    //return 6 - emptyCount;
    int conNum = 6 - emptyCount;
    if(conNum > bestConnection) {
      bestConnection = conNum;
      loc1 = emptyloc1;
      loc2 = emptyloc2;
    }
  };

  
  //+x direction
  { 
    int adj = 1;
    for(int y = 0; y < y_size; y++)
      for (int x = 0; x < x_size - 5; x++)
      {
        Loc loc0 = Location::getLoc(x, y, x_size);
        int conNum = checkOne(loc0, adj);
      }
  }
  //+y direction
  {
    int adj = x_size + 1;
    for(int y = 0; y < y_size - 5; y++)
      for(int x = 0; x < x_size; x++) {
        Loc loc0 = Location::getLoc(x, y, x_size);
        int conNum = checkOne(loc0, adj);
      }
  }

  //+x+y direction
  {
    int adj = x_size + 1 + 1;
    for(int y = 0; y < y_size - 5; y++)
      for(int x = 0; x < x_size - 5; x++) {
        Loc loc0 = Location::getLoc(x, y, x_size);
        int conNum = checkOne(loc0, adj);
      }
  }

  //-x+y direction
  {
    int adj = x_size + 1 - 1;
    for(int y = 0; y < y_size - 5; y++)
      for(int x = 5; x < x_size; x++) {
        Loc loc0 = Location::getLoc(x, y, x_size);
        int conNum = checkOne(loc0, adj);
      }
  }

  return bestConnection;
}

int Board::findFiveConsideringFirstLoc(Color color, Loc& loc1) const {
  int bestConnection = 0;
  loc1 = NULL_LOC;

  auto checkOne = [&](Loc loc0, int16_t adj) -> int {
    Loc emptyloc1 = Board::NULL_LOC;
    int emptyCount = 0;
    for(int i = 0; i < 6; i++) {
      Loc loc = loc0 + i * adj;
      assert(isOnBoard(loc));
      Color c = loc == firstLoc ? nextPla : colors[loc];
      if(c == getOpp(color))
        return 0;
      else if(c == C_EMPTY) {
        emptyCount += 1;
        if(emptyCount == 1) 
          emptyloc1 = loc;
        else
          return 0;
      } else if(c == color) {
      } else
        ASSERT_UNREACHABLE;
    }
    // return 6 - emptyCount;
    int conNum = 6 - emptyCount;
    if(conNum > bestConnection) {
      bestConnection = conNum;
      loc1 = emptyloc1;
    }
  };

  //+x direction
  {
    int adj = 1;
    for(int y = 0; y < y_size; y++)
      for(int x = 0; x < x_size - 5; x++) {
        Loc loc0 = Location::getLoc(x, y, x_size);
        int conNum = checkOne(loc0, adj);
      }
  }
  //+y direction
  {
    int adj = x_size + 1;
    for(int y = 0; y < y_size - 5; y++)
      for(int x = 0; x < x_size; x++) {
        Loc loc0 = Location::getLoc(x, y, x_size);
        int conNum = checkOne(loc0, adj);
      }
  }

  //+x+y direction
  {
    int adj = x_size + 1 + 1;
    for(int y = 0; y < y_size - 5; y++)
      for(int x = 0; x < x_size - 5; x++) {
        Loc loc0 = Location::getLoc(x, y, x_size);
        int conNum = checkOne(loc0, adj);
      }
  }

  //-x+y direction
  {
    int adj = x_size + 1 - 1;
    for(int y = 0; y < y_size - 5; y++)
      for(int x = 5; x < x_size; x++) {
        Loc loc0 = Location::getLoc(x, y, x_size);
        int conNum = checkOne(loc0, adj);
      }
  }

  return bestConnection;
}

bool Board::isSix(Color color, Loc loc) const {
  if(!isOnBoard(loc))
    return false;
  auto checkOne = [&](int x0, int y0, int dx, int dy) -> bool {
    for(int i = 0; i < 6; i++) {
      int x = x0 + dx * i;
      int y = y0 + dy * i;
      if(x < 0 || y < 0 || x >= x_size || y >= y_size)
        return false;
      Loc loc0 = Location::getLoc(x, y, x_size);
      Color c = loc0 == firstLoc ? nextPla : colors[loc0];
      if(c != color) {
        return false;
      }
    }
    return true;
  };
  int xa = Location::getX(loc, x_size);
  int ya = Location::getY(loc, x_size);
  //+x direction
  { 
    int dx = 1, dy = 0;
    for(int i = 0; i < 6; i++) {
      int x = xa - 5 + i;
      int y = ya;
      if(checkOne(x, y, dx, dy))
        return true;
    }
  }
  //+y direction
  {
    int dx = 0, dy = 1;
    for(int i = 0; i < 6; i++) {
      int x = xa;
      int y = ya - 5 + i;
      if(checkOne(x, y, dx, dy))
        return true;
    }
  }

  //+x+y direction
  {
    int dx = 1, dy = 1;
    for(int i = 0; i < 6; i++) {
      int x = xa - 5 + i;
      int y = ya - 5 + i;
      if(checkOne(x, y, dx, dy))
        return true;
    }
  }

  //-x+y direction
  {
    int dx = -1, dy = 1;
    for(int i = 0; i < 6; i++) {
      int x = xa + 5 - i;
      int y = ya - 5 + i;
      if(checkOne(x, y, dx, dy))
        return true;
    }
  }

  return false;
}





Color GameLogic::checkWinnerAfterPlayed(
  const Board& board,
  const BoardHistory& hist,
  Player pla,
  Loc loc1,
  Loc loc2
  ) {

  if(board.stage == 1) //only check winner after a full move
    return C_WALL;
  
  if((hist.rules.maxMoves != 0 || hist.rules.VCNRule != Rules::VCNRULE_NOVC) && hist.rules.firstPassWin) {
    throw StringError("GameLogic::checkWinnerAfterPlayed: firstPassWin should not be with VCN or maxMoves");
  }

  Player opp = getOpp(pla);
// connection judge
  if(board.isSix(pla, loc1)) {
    return pla;
  }
  if(board.isSix(pla, loc2)) {
    return pla;
  }

  // if the player want to pass one of the two moves, the pass should be the second
  if(loc1 == Board::PASS_LOC && board.isOnBoard(loc2))
    return opp;
  
  int myPassNum = pla == C_BLACK ? board.blackPassNum : board.whitePassNum;
  int oppPassNum = pla == C_WHITE ? board.blackPassNum : board.whitePassNum;

  if(loc1 == Board::PASS_LOC || loc2 == Board::PASS_LOC) {
    if(hist.rules.VCNRule == Rules::VCNRULE_NOVC) {
      if(oppPassNum > 0) {
        if(!hist.rules.firstPassWin)  //normal draw
        {
          return C_EMPTY;
        } else  // 对方先pass
        {
          return opp;
        }
      }
    } else {
      Color VCside = hist.rules.vcSide();
      int VClevel = hist.rules.vcLevel();

      if(VCside == pla)  // VCN不允许己方pass
      {
        return opp;
      } else  // pass次数足够则判胜
      {
        if(myPassNum >= 7 - VClevel) {
          return pla;
        }
      }
    }
  }



  // maxmoves判定
  if(hist.rules.maxMoves != 0 && board.movenum >= hist.rules.maxMoves) {
    if(hist.rules.VCNRule == Rules::VCNRULE_NOVC) {
      return C_EMPTY;
    } else  // 和棋判进攻方负
    {
      static_assert(Rules::VCNRULE_VC1_W == Rules::VCNRULE_VC1_B + 10, "Ensure VCNRule%10==N, VCNRule/10+1==color");
      Color VCside = hist.rules.vcSide();
      return getOpp(VCside);
    }
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

  if(board.stage == 1 && board.firstLoc == Board::PASS_LOC) // if the first move is pass, the second move must also be pass, no need to calculate
    return;

  //find whether the player can connect 6 in a single move
  if(board.stage == 0)  // find four
  {
    Loc loc1, loc2;
    int conNum = board.findFour(nextPlayer, loc1, loc2);
    if (conNum >= 6)
    {
      throw StringError("should not call ResultsBeforeNN::init after game finished");
    }
    if(conNum == 5) {
      assert(board.isOnBoard(loc1));
      winner = nextPlayer;
      myOnlyLoc = loc1;
    }
    if(conNum == 4) {
      assert(board.isOnBoard(loc1));
      assert(board.isOnBoard(loc2));
      winner = nextPlayer;
      if(board.getLocationPriority(loc1) <= board.getLocationPriority(loc2))
        myOnlyLoc = loc1;
      else
        myOnlyLoc = loc2;
    }
  }
  else if(board.stage == 1)  // find five
  {
    Loc loc1;
    int conNum = board.findFiveConsideringFirstLoc(nextPlayer, loc1);
    if(conNum >= 6) {
      //have win in the first move, just pass
      winner = nextPlayer;
      myOnlyLoc = Board::PASS_LOC;
    }
    if(conNum == 5) {
      assert(board.isOnBoard(loc1));
      winner = nextPlayer;
      myOnlyLoc = loc1;
    }
  }

  //if using VCN rule and opponent cant make a six, pass
  if (hist.rules.vcSide() == getOpp(nextPlayer)) {
    int VClevel = hist.rules.vcLevel();
    int myPassNum = nextPlayer == C_BLACK ? board.blackPassNum : board.whitePassNum;
    int requirePass = 7 - VClevel - myPassNum;
    if(board.stage == 1 && board.firstLoc != Board::PASS_LOC)
      requirePass += 1;
    if(requirePass <= 2)  // this move will win
    {
      winner = nextPlayer;
      myOnlyLoc = Board::PASS_LOC;
    }
    else if(requirePass <= 4)  // if opponent has no four, pass will win
    {
      Loc loc1, loc2;
      int conNum = board.findFour(getOpp(nextPlayer), loc1, loc2);
      if(conNum == 0) {
        winner = nextPlayer;
        myOnlyLoc = Board::PASS_LOC;
      }
    }
  }



  return;
}
