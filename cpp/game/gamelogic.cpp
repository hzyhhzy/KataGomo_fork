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


static int connectionLengthOneDirection(
  const Board& board,
  const Rules& rules,
  Player pla,
  Loc loc,
  short adj,
  int& terminalType) 
  //terminalType
  // 0:dead xxxxo
  // 1:wall xxxx# 
  // 2:gap+dead xxxx.o 
  // 3:life xxxx.. or xxxx.#
  // 4:long connection for sixwinrule_never xxxx.x
{
  Loc tmploc = loc;
  int conNum = 0;
  terminalType = 0;
  Color nextColor;
  while(1) {
    tmploc += adj;
    nextColor = board.isOnBoard(tmploc) ? board.colors[tmploc] : C_WALL;
    if(nextColor != pla)
      break;
    conNum++;
  }
  if(nextColor == getOpp(pla))
    terminalType = 0;
  else if(nextColor == C_WALL)
    terminalType = rules.wallBlock ? 0 : 1;
  else if(nextColor == C_EMPTY) {
    tmploc += adj;
    Color nextnextColor = board.isOnBoard(tmploc) ? board.colors[tmploc] : C_WALL;
    if(nextnextColor == getOpp(pla))
      terminalType = 2;
    else if(nextnextColor == C_WALL)
      terminalType = rules.wallBlock ? 2 : 3;
    else if(nextnextColor == C_EMPTY)
      terminalType = 3;
    else if(nextnextColor == pla) { //xxxx.x
      if(rules.sixWinRule == Rules::SIXWINRULE_ALWAYS)
        terminalType = 3;
      else if(rules.sixWinRule == Rules::SIXWINRULE_NEVER)
        terminalType = 4;
      else if (rules.sixWinRule == Rules::SIXWINRULE_CARO) {
        // check the next terminal
        // xxxx.xxo is 2
        // xxxx.xx., xxxx.xx# are 3
        Color nnnColor;
        while(1) {
          tmploc += adj;
          nnnColor = board.isOnBoard(tmploc) ? board.colors[tmploc] : C_WALL;
          if(nnnColor != pla)
            break;
        }
        if(nnnColor == getOpp(pla) || (nnnColor == C_WALL && rules.wallBlock))
          terminalType = 2;
        else
          terminalType = 3;

      }
    } 
    else
      ASSERT_UNREACHABLE;
  } 
  else
    ASSERT_UNREACHABLE;

  return conNum;
}




static bool isFive_oneLine(
  const Board& board,
  const Rules& rules,
  Player pla,
  Loc loc,
  int adj)  {
  int t1, t2;
  int myConNum = connectionLengthOneDirection(board, rules, pla, loc, adj, t1) +
                 connectionLengthOneDirection(board, rules, pla, loc, -adj, t2) + 1;
  if(myConNum < 5)
    return false;
  if (myConNum == 5)
  {
    return t1 != 0 || t2 != 0;  // oxxxxxo
  }
  if (myConNum > 5) {
    if(rules.sixWinRule == Rules::SIXWINRULE_ALWAYS)
      return true;
    else if(rules.sixWinRule == Rules::SIXWINRULE_NEVER)
      return false;
    else
      return t1 != 0 || t2 != 0; 
  }

  return false;
}

static bool isLifeFour_oneLine(const Board& board, const Rules& rules, Player pla, Loc loc, int adj) {
  int t1, t2;
  int myConNum = connectionLengthOneDirection(board, rules, pla, loc, adj, t1) +
                 connectionLengthOneDirection(board, rules, pla, loc, -adj, t2) + 1;
  if(myConNum != 4)
    return false;

  return t1 == 3 && t2 == 3;
}

MovePriority GameLogic::getMovePriorityAssumeLegal(const Board& board, const BoardHistory& hist, Player pla, Loc loc) {
  if(loc == Board::PASS_LOC)
    return MP_NORMAL;
  MovePriority MP = MP_NORMAL;

  int adjs[4] = {1, (board.x_size + 1), (board.x_size + 1) + 1, (board.x_size + 1) - 1};// +x +y +x+y -x+y
  for(int i = 0; i < 4; i++) {
    MovePriority tmpMP = MP_NORMAL;
    if(isFive_oneLine(board, hist.rules, pla, loc, adjs[i]))
      tmpMP = MP_FIVE;
    else if(isFive_oneLine(board, hist.rules, getOpp(pla), loc, adjs[i]))
      tmpMP = MP_OPPOFOUR;
    else if(isLifeFour_oneLine(board, hist.rules, pla, loc, adjs[i]))
      tmpMP = MP_MYLIFEFOUR;




    if(tmpMP < MP)
      MP = tmpMP;
  }

  return MP;
}

MovePriority GameLogic::getMovePriority(const Board& board, const BoardHistory& hist, Player pla, Loc loc) {
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



  
  if((hist.rules.maxMoves != 0 || hist.rules.VCNRule != Rules::VCNRULE_NOVC) && hist.rules.firstPassWin) {
    throw StringError("GameLogic::checkWinnerAfterPlayed This rule is not supported");
  }

  Player opp = getOpp(pla);
  int myPassNum = pla == C_BLACK ? board.blackPassNum : board.whitePassNum;
  int oppPassNum = pla == C_WHITE ? board.blackPassNum : board.whitePassNum;

  if(loc == Board::PASS_LOC) {
    if(hist.rules.VCNRule == Rules::VCNRULE_NOVC) {
      if(oppPassNum > 0) {
        if(!hist.rules.firstPassWin)  // 常规和棋
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
        if(myPassNum >= 6 - VClevel) {
          return pla;
        }
      }
    }
  }

  // 连五判定
  if(getMovePriorityAssumeLegal(board, hist, pla, loc) == MP_FIVE) {
    return pla;
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
  if(hist.rules.VCNRule != Rules::VCNRULE_NOVC && hist.rules.maxMoves != 0)
    throw StringError("ResultBeforeNN::init() can not support VCN and maxMoves simutaneously");
  if(inited)
    return;
  inited = true;

  Color opp = getOpp(nextPlayer);

  // check five and four
  bool oppHasFour = false;
  bool IHaveLifeFour = false;
  Loc myLifeFourLoc = Board::NULL_LOC;
  for(int x = 0; x < board.x_size; x++)
    for(int y = 0; y < board.y_size; y++) {
      Loc loc = Location::getLoc(x, y, board.x_size);
      MovePriority mp = getMovePriority(board, hist, nextPlayer, loc);
      if(mp == MP_FIVE) {
        winner = nextPlayer;
        myOnlyLoc = loc;
        return;
      } else if(mp == MP_OPPOFOUR) {
        oppHasFour = true;
        //myOnlyLoc = loc;
      } else if(mp == MP_MYLIFEFOUR) {
        IHaveLifeFour = true;
        myLifeFourLoc = loc;
      }
    }

  if(hist.rules.VCNRule != Rules::VCNRULE_NOVC) {
    int vcLevel = hist.rules.vcLevel() + board.blackPassNum + board.whitePassNum;

    Color vcSide = hist.rules.vcSide();
    if(vcSide == nextPlayer) {
      if(vcLevel == 5) {
        winner = opp;
        myOnlyLoc = Board::NULL_LOC;
        return;
      }
    } else if(vcSide == opp) {
      if(vcLevel == 5) {
        winner = nextPlayer;
        myOnlyLoc = Board::PASS_LOC;
        return;
      } else if(vcLevel == 4) {
        if(!oppHasFour) {
          winner = nextPlayer;
          myOnlyLoc = Board::PASS_LOC;
          return;
        }
      }
    }
  }

  // opp has four
  if(oppHasFour)
    return;

  // I have life four, opp has no four
  if(IHaveLifeFour && (!oppHasFour)) {
    int remainMovenum = hist.rules.maxMoves == 0 ? 10000 : hist.rules.maxMoves - board.movenum;
    if(remainMovenum >= 3)
      winner = nextPlayer;
    myOnlyLoc = myLifeFourLoc;
    return;
  }

}
