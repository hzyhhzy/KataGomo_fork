#pragma once
#include "NNUEglobal.h"
class ExtraStates //including rules,passes,movenums...
{
public:
  //rules
  int H;
  int W;
  int   basicRule;
  int initialVCNRule;
  int  maxMoves;
  bool firstPassWin;

  float drawBlackWinlossrate;
  float pda;

  //stages
  int movenum;
  int blackPassNum;
  int whitePassNum;

  
  ExtraStates();
  void getGlobalFeatureInput_States(float* gf, Color nextPlayer);  // 3个棋盘大小相关gf[0:3]+katago的gf的13到37 gf[8:33]，还剩下VCF的gf[3:8](katago的gf的8到12)

};
