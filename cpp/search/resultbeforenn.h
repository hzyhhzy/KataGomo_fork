#ifndef SEARCH_RESULTBEFORENN_H_
#define SEARCH_RESULTBEFORENN_H_

#include "../core/global.h"
#include "../game/boardhistory.h"

//Results before calculating NN. Including VCF, and NNUE result in the future.
class ResultBeforeNN
{
public:
  bool inited;
  Color winner;
  Loc myOnlyLoc;


  ResultBeforeNN();
  //ResultBeforeNN(const Board& board, const BoardHistory& hist, Color nextPlayer,bool hasVCF):ResultBeforeNN() { init(board, hist, nextPlayer,hasVCF); }
  void init(const Board& board, const BoardHistory& hist, Color nextPlayer);
  //bool shouldSkipNN();
};



#endif // SEARCH_RESULTBEFORENN_H_