#include "resultbeforenn.h"
#include "../vcfsolver/VCFsolver.h"
ResultBeforeNN::ResultBeforeNN()
{
  inited = false;
  calculatedVCF = false;
  winner = C_EMPTY;
  myOnlyLoc = Board::NULL_LOC;
  myVCFresult = 0;
  oppVCFresult = 0;
}

void ResultBeforeNN::init(const Board& board, const BoardHistory& hist, Color nextPlayer,bool hasVCF)
{
  if (inited&&(calculatedVCF || (!hasVCF)))return;
  inited = true;

  Color opp = getOpp(nextPlayer);

  //check five and four
  bool oppHasFour = false;
  bool oppHasDoubleFour = false;
  bool IHaveLifeFour = false;
  Loc myLifeFourLoc = Board::NULL_LOC;
  for (int x = 0; x < board.x_size; x++)
    for (int y = 0; y < board.y_size; y++)
    {
      Loc loc = Location::getLoc(x, y, board.x_size);
      MovePriority mp = board.getMovePriority(nextPlayer, loc, hist.rules);
      if (mp == MP_FIVE)
      {
        winner = nextPlayer;
        myOnlyLoc = loc;
        return;
      }
      else if (mp == MP_OPPOFOUR)
      {
        if (oppHasFour)oppHasDoubleFour = true;
        oppHasFour = true;
        myOnlyLoc = loc;
      }
      else if (mp == MP_MYLIFEFOUR)
      {
        IHaveLifeFour = true;
        myLifeFourLoc = loc;
      }
    }
  //I have no five
  if (oppHasDoubleFour)
  {
    winner = opp;
    return;
  }
  //I have no five, opp has no four
  if (IHaveLifeFour&&(!oppHasFour))
  {
    winner = nextPlayer;
    myOnlyLoc = myLifeFourLoc;
    return;
  }

  if (!hasVCF)return;

  //check VCF
  calculatedVCF = true;
  uint16_t oppvcfloc;
  VCFsolver::run(board,hist.rules, getOpp(nextPlayer), oppVCFresult, oppvcfloc);

  uint16_t myvcfloc;
  VCFsolver::run(board,hist.rules, nextPlayer, myVCFresult, myvcfloc);
  if (myVCFresult == 1)
  {
    winner = nextPlayer;
    myOnlyLoc = myvcfloc;
    return;
  }
}
