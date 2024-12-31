#include "NNUEBoard.h"

#include <random>
using namespace NNUE;

NNUEBoard::NNUEBoard(const ModelWeight* weights)
  : moveCacheBlength(0), moveCacheWlength(0), blackEvaluator(weights), whiteEvaluator(weights),board(MaxBS,MaxBS), rules(){
  moveCacheBlength = 0;
  moveCacheWlength = 0;
}




void NNUEBoard::clear(const Board& b, const Rules& r) {
  moveCacheBlength = 0;
  moveCacheWlength = 0;
  blackEvaluator.clear();
  whiteEvaluator.clear();
  board = b;
  rules = r;

  //set the board inside the nnue evaluator
  for(int y = 0; y < board.y_size; y++)
    for (int x = 0; x < board.x_size; x++)
    {
      Loc loc = Location::getLoc(x, y, board.x_size);
      Color c = board.colors[c];
      if(loc == board.firstLoc)
        c = board.nextPla;
      if (c != C_EMPTY)
      {
        NU_Loc nloc = x + y * MaxBS;
        blackEvaluator.play(c, nloc);
        whiteEvaluator.play(getOpp(c), nloc);
      }
    }

}

void NNUEBoard::play(Color color, Loc loc) {
  if (color != board.nextPla)
    throw "wrong next player";
  addCache(false, color, loc);//nnue evaluator

  Loc kloc=Location::getLoc(loc%MaxBS,loc/MaxBS,board.x_size);
  board.playMoveAssumeLegal(kloc, color);


}

void NNUEBoard::undo(const UndoRecord& ur, Color color, NU_Loc loc) {
  addCache(true, color, loc);
  if (stage == 0)//remove the played two stones
  {
    if (loc >= 0 && loc < MaxBS * MaxBS) {
      setStone(C_EMPTY, loc);
    }
    NU_Loc firstLoc1 = ur.firstLoc;
    if (firstLoc1 >= 0 && firstLoc1 < MaxBS * MaxBS) {
      setStone(C_EMPTY, firstLoc1);
    }
  }
  // Restore the state from UndoRecord
  movenum = ur.movenum;
  stage = ur.stage;
  meanStoneX = ur.meanStoneX;
  meanStoneY = ur.meanStoneY;
  nextPla = ur.nextPla;
  firstLoc = ur.firstLoc;
  firstLocPriority = ur.firstLocPriority;
  blackPassNum = ur.blackPassNum;
  whitePassNum = ur.whitePassNum;
}

void NNUEBoard::clearCache(Color color) {
  if (color == C_BLACK)
  {
    for (int i = 0; i < moveCacheBlength; i++)
    {
      MoveCache move = moveCacheB[i];
      if(move.isUndo)blackEvaluator->undo(move.loc);
      else blackEvaluator->play(move.color,move.loc);
    }
    moveCacheBlength = 0;
  }
  else if (color == C_WHITE)
  {
    for (int i = 0; i < moveCacheWlength; i++)
    {
      MoveCache move = moveCacheW[i];
      if(move.isUndo)whiteEvaluator->undo(move.loc);
      else whiteEvaluator->play(getOpp(move.color),move.loc);
    }
    moveCacheWlength = 0;
  }
}

void NNUEBoard::addCache(bool isUndo, Color color, NU_Loc loc) {
  if(loc == NU_LOC_PASS || loc == NU_LOC_NULL)
    return;

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

bool NNUEBoard::isContraryMove(MoveCache a, MoveCache b) {
  if (a.isUndo == b.isUndo)return false;
  else
  {
    if (a.loc != b.loc)std::cout << "Evaluator::isContraryMove strange bugs";
    if (a.color != b.color)std::cout << "Evaluator::isContraryMove strange bugs";
    return true;
  }
}


NNUE::ValueType NNUEBoard::evaluateFull(Color color, NNUE::PolicyType* policy) {
  updateInputBuf(color);
  clearCache(color);
  if(color == C_BLACK)
    return blackEvaluator.evaluateFull(gfInputBuf, illegalMapBuf, policy);
  else
    return whiteEvaluator.evaluateFull(gfInputBuf, illegalMapBuf, policy);
}

NNUE::ValueType NNUEBoard::evaluateValue(Color color) {
  updateInputBuf(color);
  clearCache(color);
  if(color == C_BLACK)
    return blackEvaluator.evaluateValue(gfInputBuf, illegalMapBuf);
  else
    return whiteEvaluator.evaluateValue(gfInputBuf, illegalMapBuf);
}

void NNUEBoard::evaluatePolicy(const float* gf, Color color, NNUE::PolicyType* policy) {
  updateInputBuf(color);
  clearCache(color);
  if(color == C_BLACK)
    blackEvaluator->evaluatePolicy(gfInputBuf, illegalMapBuf, policy);
  else
    whiteEvaluator->evaluatePolicy(gfInputBuf, illegalMapBuf, policy);
}