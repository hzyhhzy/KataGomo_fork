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
  if(hist.rules.VCNRule != Rules::VCNRULE_NOVC && hist.rules.maxMoves != 0)
    throw StringError("ResultBeforeNN::init() can not support VCN and maxMoves simutaneously");
  bool willCalculateVCF = hasVCF && hist.rules.maxMoves == 0;
  if (inited&&(calculatedVCF || (!willCalculateVCF)))return;
  inited = true;

  Color opp = getOpp(nextPlayer);

  //check five and four
  bool oppHasFour = false;
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
        oppHasFour = true;
        myOnlyLoc = loc;
      }
      else if (mp == MP_MYLIFEFOUR)
      {
        IHaveLifeFour = true;
        myLifeFourLoc = loc;
      }
    }

  if (hist.rules.VCNRule != Rules::VCNRULE_NOVC)
  {
    int vcLevel = hist.rules.vcLevel()+ hist.blackPassNum + hist.whitePassNum;;
    Color vcSide = hist.rules.vcSide();
    if (vcSide == nextPlayer)
    {
      if (vcLevel == 5)
      {
        winner = opp;
        myOnlyLoc = Board::NULL_LOC;
        return;
      }
    }
    else if (vcSide == opp)
    {
      if (vcLevel == 5)
      {
        winner = nextPlayer;
        myOnlyLoc = Board::PASS_LOC;
        return;
      }
      else if (vcLevel == 4)
      {
        if (!oppHasFour)
        {
          winner = nextPlayer;
          myOnlyLoc = Board::PASS_LOC;
          return;
        }
      }
    }
  }

  //opp has four
  if (oppHasFour)return;

  //I have life four, opp has no four
  if (IHaveLifeFour&&(!oppHasFour))
  {
    int remainMovenum= hist.rules.maxMoves==0? 10000 : hist.rules.maxMoves-hist.getMovenum();
    if(remainMovenum>=3)winner = nextPlayer;
    myOnlyLoc = myLifeFourLoc;
    return;
  }

  if (!willCalculateVCF)return;

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
