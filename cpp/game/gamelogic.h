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
#include "../nnue/MCTSsearch.h"
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

  MovePriority getMovePriorityAssumeLegal(const Board& board, const BoardHistory& hist, Player pla, Loc loc);
  MovePriority getMovePriority(const Board& board, const BoardHistory& hist, Player pla, Loc loc);

  //C_EMPTY = draw, C_WALL = not finished 
  Color checkWinnerAfterPlayed(const Board& board, const BoardHistory& hist, Player pla, Loc loc);


  //some results calculated before calculating NN
  //part of NN input, and then change policy/value according to this
  struct ResultsBeforeNN {
    bool inited;
    bool calculatedVCF;
    bool calculatedNNUE;
    Color winner;
    Loc myOnlyLoc;

    uint8_t myVCFresult;
    uint8_t oppVCFresult;

    int64_t nnueVisitsTotal;
    double nnueRootValue;
    double nnueRootDraw;
    Loc nnueBestLoc;
    float nnueValueMap[MaxBS * MaxBS + 1];
    float nnueDrawMap[MaxBS * MaxBS + 1];
    float nnueVisits[MaxBS * MaxBS + 1];

    ResultsBeforeNN();
    void initRBN(const Board& board, const BoardHistory& hist, Color nextPlayer, bool hasVCF, int nnueSearchN, NNUE::MCTSsearch* nnueSearch);
  };
}




#endif // GAME_RULELOGIC_H_
