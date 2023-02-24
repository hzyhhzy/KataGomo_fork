/*
 * gamelogic.h
 * Logics of game rules
 * Some other game logics are in board.h/cpp
 * 
 * Gomoku as a representive
 */

#ifndef GAME_GAMELOGIC_H_
#define GAME_GAMELOGIC_H_

#include "../game/boardhistory.h"

/*
* Other game logics:
* Board::
*/

namespace GameLogic {
  //game params

  const int MinCaptureNum = 6;
  const int CaptureNumMinusScore = 5;
  const int DefaultTargetScore = 4;



  typedef char MovePriority;
  static const MovePriority MP_NORMAL = 126;
  static const MovePriority MP_SUDDEN_WIN = 1;//win after this move
  static const MovePriority MP_ONLY_NONLOSE_MOVES = 2;//the only non-lose moves
  static const MovePriority MP_WINNING = 3;//sure win, but not this move
  static const MovePriority MP_ILLEGAL = -1;//illegal moves

  MovePriority getMovePriorityAssumeLegal(const Board& board, const BoardHistory& hist, Player pla, Loc loc);
  MovePriority getMovePriority(const Board& board, const BoardHistory& hist, Player pla, Loc loc);

  //C_EMPTY = draw, C_WALL = not finished 
  Color checkWinnerAfterPlayed(const Board& board, const BoardHistory& hist, Player pla, Loc loc);

  
  int checkCaptureIfPlay(
    const Board& board,
    Player pla,
    Loc loc);  // no matter whether played, assume it is already played

  int maybeCapture(
    Board& board,
    Player pla,
    Loc loc);  // if symmetry, capture them and return stonenum

  int removeStoneAndMaybeCapture(Board& board, Loc loc) //

  void markConnectedStonesRecursive(
    std::vector<bool>& visited,
    const Board& board,
    Player pla,
    int x,
    int y,
    Loc locAssumed,  
    Color colorAssumed,  //assume the color of locAssumed is always colorAssumed, no matter what is it on the board. If not used, locAssumed=NULL_LOC
    int& minX,
    int& maxX,
    int& minY,
    int& maxY,
    int& stoneNum);  // visited[x+y*x_size]

  bool isSymmetry(
    std::vector<bool>& stones,
    int x_size,
    int minX,
    int maxX,
    int minY,
    int maxY);  // stones[x+y*x_size]



  //some results calculated before calculating NN
  //part of NN input, and then change policy/value according to this
  struct ResultsBeforeNN {
    bool inited;
    Color winner;
    Loc myOnlyLoc;
    ResultsBeforeNN();
    void init(const Board& board, const BoardHistory& hist, Color nextPlayer);
  };
}




#endif // GAME_RULELOGIC_H_
