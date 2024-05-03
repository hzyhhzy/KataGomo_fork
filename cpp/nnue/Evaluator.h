#pragma once
#include "NNUEglobal.h"
#include "Eva_nnuev2.h"
#include "HashTable/NNUEHashTable.h"
class Evaluator
{
public:
  Eva_nnuev2 blackEvaluator;
  Eva_nnuev2 whiteEvaluator;



  Evaluator(const NNUEV2::ModelWeight* w);

  void clear();
  
  
  NNUE::ValueType evaluateFull(const float *gf, Color color, NNUE::PolicyType *policy)
  {
    clearCache(color);
    if (color == C_BLACK)
      return blackEvaluator.evaluateFull(gf, policy);
    else
      return whiteEvaluator.evaluateFull(gf, policy);
  }
  void evaluatePolicy(const float *gf, Color color, NNUE::PolicyType *policy)
  {
    clearCache(color);
    if (color == C_BLACK)
      blackEvaluator.evaluatePolicy(gf, policy);
    else
      whiteEvaluator.evaluatePolicy(gf,policy);
  }
  NNUE::ValueType evaluateValue(const float *gf, Color color)
  {
    clearCache(color);
    if (color == C_BLACK)
      return blackEvaluator.evaluateValue(gf);
    else
      return whiteEvaluator.evaluateValue(gf);
  }
  void play(Color color, NU_Loc loc); 
  void undo(Color color, NU_Loc loc);
  
  //Color* board() const { return blackEvaluator->board; }

private:

  //每次调用play或者undo时，先不在EvaluatorOneSide里面走，因为开销很大。先缓存。
  //MCTS的时候，经常“走回头路”，使用cache可以提速。
  struct MoveCache
  {
    bool isUndo;
    Color color;
    NU_Loc loc;
    MoveCache() :isUndo(false), color(C_EMPTY), loc(NNUE::NU_LOC_NULL){}
    MoveCache(bool isUndo,Color color,NU_Loc loc) :isUndo(isUndo), color(color), loc(loc){}
  };

  MoveCache moveCacheB[MaxBS * MaxBS], moveCacheW[MaxBS * MaxBS];
  int moveCacheBlength, moveCacheWlength;

  void clearCache(Color color);//把所有缓存的步数清空，使得evaluatorOneSide的board与这里的board相同
  void addCache(bool isUndo, Color color, NU_Loc loc);
  bool isContraryMove(MoveCache a, MoveCache b);//是不是可以抵消的一对操作
};

