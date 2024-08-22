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

  typedef char MovePriority;
  static const MovePriority MP_NORMAL = 126;
  static const MovePriority MP_SUDDEN_WIN = 1;//win after this move
  static const MovePriority MP_ONLY_NONLOSE_MOVES = 2;//the only non-lose moves
  static const MovePriority MP_WINNING = 3;//sure win, but not this move
  static const MovePriority MP_ILLEGAL = -1;//illegal moves

  //C_EMPTY = draw, C_WALL = not finished 
  Color checkWinnerAfterPlayed(const Board& board, const BoardHistory& hist, Player pla, Loc loc);


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
