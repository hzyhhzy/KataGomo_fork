#include "ExtraStates.h"
using namespace NNUE;

ExtraStates::ExtraStates() { 
	H = MaxBS; 
	W = MaxBS;
  basicRule = DEFAULT_RULE;
  initialVCNRule   = Rules::VCNRULE_NOVC;
  maxMoves  = 0;
  firstPassWin = false;

  drawBlackWinlossrate = 0;
  pda                  = 0;


  movenum      = 0;
  blackPassNum = 0;
  whitePassNum = 0;
}

void ExtraStates::getGlobalFeatureInput_States(float* gf, Color nextPlayer)
{
  //std::vector<float> gf(28,0);
  //=katago-10

  double boardArea = H * W;
  gf[0]            = boardArea / 225.0 - 1;
  gf[1]            = sqrt(boardArea / 225.0) - 1;
  gf[2]            = (H - W) * (H - W) / boardArea;

  for (int i = 8; i < 33; i++)
    gf[i] = 0;


  int myPassNum = nextPlayer == C_WHITE ? whitePassNum : blackPassNum;
  int oppPassNum = nextPlayer == C_BLACK ? whitePassNum : blackPassNum;
  if (initialVCNRule == Rules::VCNRULE_NOVC && !firstPassWin) {
    gf[8] = nextPlayer == C_BLACK  ? drawBlackWinlossrate : -drawBlackWinlossrate;
    gf[9] = oppPassNum>0;
  }

  gf[10] = pda != 0;
  gf[11] = nextPlayer == C_BLACK ? 0.5*pda : -0.5*pda;

  if (firstPassWin) {
    gf[12] = 1.0;
    gf[13] = myPassNum > 0;
    gf[14] = oppPassNum > 0;
  }

  if (initialVCNRule != Rules::VCNRULE_NOVC) {
    static_assert(Rules::VCNRULE_VC4_W == 14);
    Color VCside = 1 + initialVCNRule / 10;
    int VCLevel  = ((VCside == nextPlayer) ? oppPassNum:myPassNum) + initialVCNRule % 10;
    if (VCLevel >= 1 && VCLevel <= 5) {
      if (VCside == nextPlayer)
        gf[14 + VCLevel] = 1.0;
      else
        gf[19 + VCLevel] = 1.0;
    }
    else 
      std::cout << "illegal VCN rule in nninput:" << VCLevel << " " << initialVCNRule
                << std::endl;
  }

  
  if (maxMoves != 0) {
    gf[25]    = 1.0;
    double boardArea = H * W;
    gf[26]    = maxMoves / boardArea;
    gf[27]    = movenum / boardArea;
    gf[28]    = exp(-(maxMoves - movenum) / 50.0);
    gf[29]    = exp(-(maxMoves - movenum) / 15.0);
    gf[30]    = exp(-(maxMoves - movenum) / 5.0);
    gf[31]    = exp(-(maxMoves - movenum) / 1.5);
    gf[32]    = 2 * ((int(maxMoves - movenum)) % 2) - 1;
  }

}
