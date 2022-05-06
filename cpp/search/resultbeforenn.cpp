#include "resultbeforenn.h"
ResultBeforeNN::ResultBeforeNN()
{
  inited = false;
  winner = C_EMPTY;
  myOnlyLoc = Board::NULL_LOC;
}

void ResultBeforeNN::init(const Board& board, const BoardHistory& hist, Color nextPlayer)
{
  if(hist.rules.VCNRule != Rules::VCNRULE_NOVC && hist.rules.maxMoves != 0)
    throw StringError("ResultBeforeNN::init() can not support VCN and maxMoves simutaneously");
  inited = true;

  Color opp = getOpp(nextPlayer);

  //check five and four
  bool oppHasFour = false;
  for (int x = 0; x < board.x_size; x++)
    for (int y = 0; y < board.y_size; y++)
    {
      Loc loc = Location::getLoc(x, y, board.x_size);
      MovePriority mp = board.getMovePriority(nextPlayer, loc, hist.rules);
      MovePriority oppmp = board.getMovePriority(getOpp(nextPlayer), loc, hist.rules);
      if (mp == MP_FIVE)
      {
        winner = nextPlayer;
        myOnlyLoc = loc;
        return;
      }
      else if (oppmp == MP_FIVE)
      {
        oppHasFour = true;
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

  
}
