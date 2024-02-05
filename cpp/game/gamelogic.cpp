#include "../game/gamelogic.h"
#include "../vcfsolver/VCFsolver.h"

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
  Player pla,
  bool isSixWin,
  Loc loc,
  short adj,
  bool& isLife) 
{
  Loc tmploc = loc;
  int conNum = 0;
  isLife = false;
  while(1) {
    tmploc += adj;
    if(!board.isOnBoard(tmploc))
      break;
    if(board.colors[tmploc] == pla)
      conNum++;
    else if(board.colors[tmploc] == C_EMPTY) {
      isLife = true;

      if(!isSixWin) {
        tmploc += adj;
        if(board.isOnBoard(tmploc) && board.colors[tmploc] == pla)
          isLife = false;
      }
      break;
    } 
    else
      break;
  }
  return conNum;
}

static GameLogic::MovePriority getMovePriorityOneDirectionAssumeLegal(
  const Board& board,
  Player pla,
  bool isSixWinMe,
  bool isSixWinOpp,
  Loc loc,
  int adj)  {
  Player opp = getOpp(pla);
  bool isMyLife1, isMyLife2, isOppLife1, isOppLife2;
  int myConNum = connectionLengthOneDirection(board, pla, isSixWinMe, loc, adj, isMyLife1) +
                 connectionLengthOneDirection(board, pla, isSixWinMe, loc, -adj, isMyLife2) + 1;
  int oppConNum = connectionLengthOneDirection(board, opp, isSixWinOpp, loc, adj, isOppLife1) +
                  connectionLengthOneDirection(board, opp, isSixWinOpp, loc, -adj, isOppLife2) + 1;
  if(myConNum == 5 || (myConNum > 5 && isSixWinMe))
    return GameLogic::MP_SUDDEN_WIN;

  if(oppConNum == 5 || (oppConNum > 5 && isSixWinOpp))
    return GameLogic::MP_ONLY_NONLOSE_MOVES;

  if(myConNum == 4 && isMyLife1 && isMyLife2)
    return GameLogic::MP_WINNING;

  return GameLogic::MP_NORMAL;
}

GameLogic::MovePriority GameLogic::getMovePriorityAssumeLegal(const Board& board, const BoardHistory& hist, Player pla, Loc loc) {
  if(loc == Board::PASS_LOC)
    return MP_NORMAL;
  MovePriority MP = MP_NORMAL;

  bool isSixWinMe = hist.rules.basicRule == Rules::BASICRULE_FREESTYLE  ? true
                    : hist.rules.basicRule == Rules::BASICRULE_STANDARD ? false
                    : hist.rules.basicRule == Rules::BASICRULE_RENJU    ? (pla == C_WHITE)
                                                                  : true;

  bool isSixWinOpp = hist.rules.basicRule == Rules::BASICRULE_FREESTYLE ? true
                     : hist.rules.basicRule == Rules::BASICRULE_STANDARD ? false
                     : hist.rules.basicRule == Rules::BASICRULE_RENJU    ? (pla == C_BLACK)
                                                                   : true;

  int adjs[4] = {1, (board.x_size + 1), (board.x_size + 1) + 1, (board.x_size + 1) - 1};// +x +y +x+y -x+y
  for(int i = 0; i < 4; i++) {
    MovePriority tmpMP = getMovePriorityOneDirectionAssumeLegal(board, pla, isSixWinMe, isSixWinOpp, loc, adjs[i]);
    if(tmpMP < MP)
      MP = tmpMP;
  }

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

  if(board.movenum >= board.x_size * board.y_size)
    return C_EMPTY;

  return C_WALL;
}

GameLogic::ResultsBeforeNN::ResultsBeforeNN() {
  inited = false;
  winner = C_WALL;
  myOnlyLoc = Board::NULL_LOC;
}

void GameLogic::ResultsBeforeNN::init(const Board& board, const BoardHistory& hist, Color nextPlayer, bool hasVCF) {
  if(hist.rules.VCNRule != Rules::VCNRULE_NOVC && hist.rules.maxMoves != 0)
    throw StringError("ResultBeforeNN::init() can not support VCN and maxMoves simutaneously");
  bool willCalculateVCF = hasVCF && hist.rules.maxMoves == 0;
  if(inited && (calculatedVCF || (!willCalculateVCF)))
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
        myOnlyLoc = loc;
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

  if(!willCalculateVCF)
    return;

  // check VCF
  calculatedVCF = true;
  uint16_t oppvcfloc;
  VCFsolver::run(board, hist.rules, getOpp(nextPlayer), oppVCFresult, oppvcfloc);

  uint16_t myvcfloc;
  VCFsolver::run(board, hist.rules, nextPlayer, myVCFresult, myvcfloc);
  if(myVCFresult == 1) {
    winner = nextPlayer;
    myOnlyLoc = myvcfloc;
    return;
  }
}
