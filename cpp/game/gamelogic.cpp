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
  const BoardHistory& hist,
  Player pla,
  Loc loc,
  short adj,
  bool& isLife) {
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
      break;
    } else
      break;
  }
  return conNum;
}

static GameLogic::MovePriority getMovePriorityOneDirectionAssumeLegal(
  const Board& board,
  const BoardHistory& hist,
  Player pla,
  Loc loc,
  int adj)  {
  Player opp = getOpp(pla);
  bool isMyLife1, isMyLife2, isOppLife1, isOppLife2;
  int myConNum = connectionLengthOneDirection(board, hist, pla, loc, adj, isMyLife1) +
                 connectionLengthOneDirection(board, hist, pla, loc, -adj, isMyLife2) + 1;
  int oppConNum = connectionLengthOneDirection(board, hist, opp, loc, adj, isOppLife1) +
                  connectionLengthOneDirection(board, hist, opp, loc, -adj, isOppLife2) + 1;
  if(myConNum >= 5)
    return GameLogic::MP_SUDDEN_WIN;

  if(oppConNum >= 5)
    return GameLogic::MP_ONLY_NONLOSE_MOVES;

  if(myConNum == 4 && isMyLife1 && isMyLife2)
    return GameLogic::MP_WINNING;

  return GameLogic::MP_NORMAL;
}

GameLogic::MovePriority GameLogic::getMovePriorityAssumeLegal(const Board& board, const BoardHistory& hist, Player pla, Loc loc) {
  if(loc == Board::PASS_LOC)
    return MP_NORMAL;
  MovePriority MP = MP_NORMAL;

  int adjs[4] = {1, (board.x_size + 1), (board.x_size + 1) + 1, (board.x_size + 1) - 1};// +x +y +x+y -x+y
  for(int i = 0; i < 4; i++) {
    MovePriority tmpMP = getMovePriorityOneDirectionAssumeLegal(board, hist, pla, loc, adjs[i]);
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

void GameLogic::ResultsBeforeNN::init(const Board& board, const BoardHistory& hist, Color nextPlayer) {
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
      if(mp == MP_SUDDEN_WIN) {
        winner = nextPlayer;
        myOnlyLoc = loc;
        return;
      } else if(mp == MP_ONLY_NONLOSE_MOVES) {
        oppHasFour = true;
        myOnlyLoc = loc;
      } else if(mp == MP_WINNING) {
        IHaveLifeFour = true;
        myLifeFourLoc = loc;
      }
    }

  // opp has four
  if(oppHasFour)
    return;

  // I have life four, opp has no four
  if(IHaveLifeFour) {
    winner = nextPlayer;
    myOnlyLoc = myLifeFourLoc;
    return;
  }


  return;
}
