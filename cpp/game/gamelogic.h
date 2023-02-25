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



  //C_EMPTY = draw, C_WALL = not finished 
  Color checkWinnerAfterPlayed(const Board& board, const BoardHistory& hist, Player pla, Loc loc);

  
  int checkCaptureIfPlay( //check whether it is a capture if played in stage=0, maybe not a win move because it does not make sure whether can make any moves in stage=1
    const Board& board,
    Player pla,
    Loc loc);  

  int maybeCapture(
    Board& board,
    Player pla);  // if symmetry, capture them and return stonenum





}




#endif // GAME_RULELOGIC_H_
