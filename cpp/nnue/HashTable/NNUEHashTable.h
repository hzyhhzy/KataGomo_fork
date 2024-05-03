#pragma once
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include "../core/hash.h"
#include "../search/mutexpool.h"


#include "../NNUEglobal.h"


struct MCTSnode;

class NNUEHashTable
{
  struct Entry
  {
    uint64_t hash1;  // HASH相撞的概率极低，可以忽略
    NNUE::MCTSsureResult sureResult;
    int16_t        legalChildrennum;
    NNUE::ValueSum WRtotal;
    NU_Loc      locs[NNUE::MAX_MCTS_CHILDREN];
    uint16_t policy[NNUE::MAX_MCTS_CHILDREN];
    Entry() : hash1(0) {}
    ~Entry() {}
  };

  Entry* entries;
  MutexPool* mutexPool;
  uint64_t   tableSize;
  uint64_t   tableMask;
  uint32_t   mutexPoolMask;

public:
  static Hash128 ZOBRIST_loc[4][MaxBS * MaxBS];
  static Hash128 ZOBRIST_nextPlayer[3];
  static Hash128 ZOBRIST_boardH[MaxBS];
  static Hash128 ZOBRIST_boardW[MaxBS];
  static Hash128 ZOBRIST_basicRule[3];
  static Hash128 ZOBRIST_VCN[20];
  static Hash128 ZOBRIST_maxMoves[MaxBS * MaxBS * 2];
  static Hash128 ZOBRIST_firstPassWin;

  static Hash128 ZOBRIST_drawBlackWinlossrate_base;
  static Hash128 ZOBRIST_pda_base;

  static Hash128 ZOBRIST_movenum[MaxBS * MaxBS * 2];
  static Hash128 ZOBRIST_blackPassNum[MaxBS * MaxBS * 2];
  static Hash128 ZOBRIST_whitePassNum[MaxBS * MaxBS * 2];
  static void initHash(int64_t seed);



  NNUEHashTable(int sizePowerOfTwo, int mutexPoolSizePowerOfTwo);
  // NNUEHashTable()
  //{
  //  NNUEHashTable(20, 13);
  //}
  ~NNUEHashTable();

  NNUEHashTable(const NNUEHashTable& other) = delete;
  NNUEHashTable& operator=(const NNUEHashTable& other) = delete;

  // These are thread-safe. For get, ret will be set to nullptr upon a failure to find.
  bool get(Hash128 hash, MCTSnode& node);
  void set(Hash128 hash, const MCTSnode& node);
};
