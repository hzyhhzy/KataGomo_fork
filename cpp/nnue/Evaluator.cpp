#include "Evaluator.h"

#include <random>
using namespace NNUE;

Evaluator::Evaluator(const NNUEV2::ModelWeight* w):
  blackEvaluator(w),whiteEvaluator(w)
{
  clear();
}




void Evaluator::clear()
{
  moveCacheBlength = 0;
  moveCacheWlength = 0;
  blackEvaluator.clear();
  whiteEvaluator.clear();
}

void Evaluator::play(Color color, NU_Loc loc)
{
  addCache(false, color, loc);
  //blackEvaluator->play(color, loc); 
  //whiteEvaluator->play(getOpp(color), loc); 
}

void Evaluator::undo(Color color, NU_Loc loc)
{
  addCache(true, color, loc);
  //blackEvaluator->undo(loc); 
  //whiteEvaluator->undo(loc); 
}

void Evaluator::clearCache(Color color)
{
  if (color == C_BLACK)
  {
    for (int i = 0; i < moveCacheBlength; i++)
    {
      MoveCache move = moveCacheB[i];
      if(move.isUndo)blackEvaluator.undo(move.loc);
      else blackEvaluator.play(move.color,move.loc);
    }
    moveCacheBlength = 0;
  }
  else if (color == C_WHITE)
  {
    for (int i = 0; i < moveCacheWlength; i++)
    {
      MoveCache move = moveCacheW[i];
      if(move.isUndo)whiteEvaluator.undo(move.loc);
      else whiteEvaluator.play(getOpp(move.color),move.loc);
    }
    moveCacheWlength = 0;
  }
}

void Evaluator::addCache(bool isUndo, Color color, NU_Loc loc)
{
  MoveCache newcache(isUndo, color, loc);

  if (moveCacheBlength == 0|| !isContraryMove(moveCacheB[moveCacheBlength-1],newcache))
  {
    moveCacheB[moveCacheBlength] = newcache;
    moveCacheBlength++;
  }
  else//可以消除一步
  {
    moveCacheBlength--;
  }

  if (moveCacheWlength == 0|| !isContraryMove(moveCacheW[moveCacheWlength-1],newcache))
  {
    moveCacheW[moveCacheWlength] = newcache;
    moveCacheWlength++;
  }
  else//可以消除一步
  {
    moveCacheWlength--;
  }
}

bool Evaluator::isContraryMove(MoveCache a, MoveCache b)
{
  if (a.loc != b.loc)return false;
  if (a.color != b.color)return false;
  if (a.isUndo != b.isUndo)return true;
  else std::cout<<"Evaluator::isContraryMove strange bugs";
  return false;
}
