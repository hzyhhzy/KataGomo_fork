#pragma once
#include <iostream>
#include <vector>
#include "VCFHashTable.h"
#include "../core/hash.h"
#include "../core/global.h"
#include "../game/board.h"


// 屎山代码，不建议修改

using namespace std;

/*
const uint8_t C_EMPTY = 0;
const uint8_t C_BLACK = 1;
const uint8_t C_WHITE = 2;
*/


class VCFsolver
{
public:
  static const int sz = Board::MAX_LEN;
  static const uint8_t C_EM = 0;//空
  static const uint8_t C_MY = 1;//自己的棋子
  static const uint8_t C_OPP = 2;//对手的棋子

  static uint64_t MAXNODE ;

  static Hash128 zob_board[2][sz][sz];//pla-1,y,x
  static const Hash128 zob_plaWhite;
  static const Hash128 zob_plaBlack;

  //hashTable
  static VCFHashTable hashtable;

  //RULE
  Rules rules;
  uint8_t forbiddenSide;//如果自己是黑棋则为0，否则为1

  //board
  int xsize, ysize;
  uint8_t rootboard[sz][sz];//board[y][x]
  uint8_t board[sz][sz];//board[y][x]
  int32_t movenum;//己方总棋子个数
  Hash128 boardhash;
  int64_t oppFourPos;//对手冲四点，如果没有则为-1

  //把棋盘分成许多的五子组

  uint8_t mystonecount[4][sz][sz];//依次是下面4个
  uint8_t oppstonecount[4][sz][sz];//依次是下面4个
  //uint8_t mystonecount1[ysize][xsize - 4];//x
  //uint8_t mystonecount2[ysize - 4][xsize];//y
  //uint8_t mystonecount3[ysize - 4][xsize - 4];//+x+y
  //uint8_t mystonecount4[ysize - 4][xsize - 4];//+x-y
  vector<int64_t> threes;
  //mystonecount[t][y][x]对应(t*sz*sz+y*sz+x)<<32|pos1<<16|pos2,   pos=y*sz+x
  //眠三表。可能有被堵死的眠三，但初始时的死眠三不计
  uint64_t threeCount;//表示当前threes的前多少个有效。为了降低计算量，不删除threes里面多余的项

  uint64_t nodenum;

  //result
  int32_t rootresultpos;
  int32_t bestmovenum;//最短解回合数，到连五截止
  //result:   0 有待计算，1可以vcf或者可以连五，2不可以vcf，3不知道能不能vcf
  //resultpos:  取胜点 pos=y*sz+x
  //hashtable里面的result=(resultpos<<32)|result

  static uint64_t totalAborted;
  static uint64_t totalSolved;
  static uint64_t totalnodenum;

  static void init();
  VCFsolver(const Rules rules):rules(rules){ threes.resize(4 * sz * sz); }
  void solve(const Board& board, uint8_t pla,uint8_t& res,uint16_t& loc);
  void print();
  void printRoot();
  static void run(const Board& board,const Rules& rules, uint8_t pla, uint8_t& res, uint16_t& loc)
  {
#ifndef NOVCF
    VCFsolver solver(rules);
    solver.solve(board, pla, res, loc);
#else
    res = 2;
    loc = Board::NULL_LOC;
#endif
  }

//private:
public:
  int32_t setBoard(const Board& board, uint8_t pla);//如果无需搜索就有胜负，则返回值是结果，否则是0


  uint32_t findEmptyPos(int t, int y, int x);//找到一个组里面的空地方，pos1<<16|pos2（2个空位） 或 pos1（1个空位）
  uint32_t findDefendPosOfFive(int y, int x);//找己方冲四的防守点，只在个别地方用

//For Renju and Standard
  void addNeighborSix(int y, int x, uint8_t pla, int factor);//factor是一次加多少，取+6是加6（落子），取-6是undo


  int32_t solveIter(bool isRoot);//递归求解,结果是返回值
  //获胜是10000减步数，必败是-10000，未知是0,被剪枝是-x，代表最少可能取胜的回合数是x
  int32_t play(int x, int y, uint8_t pla,bool updateHash);
  void undo(int x, int y, int64_t oppFourPos, uint64_t threeCount, bool updateHash);

//For Renju
  bool isForbiddenMove(int y, int x,bool fiveForbidden = false);
  bool checkLife3(int y, int x, int t);//检查是否是活三
  void printForbiddenMap();
};


