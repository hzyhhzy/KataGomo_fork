#include "NNUEHashTable.h"
#include "../MCTSsearch.h"
#include <random>

using namespace NNUE;
using namespace std;


Hash128 NNUEHashTable::ZOBRIST_loc[4][MaxBS * MaxBS];
Hash128 NNUEHashTable::ZOBRIST_nextPlayer[3];
Hash128 NNUEHashTable::ZOBRIST_boardH[MaxBS];
Hash128 NNUEHashTable::ZOBRIST_boardW[MaxBS];
Hash128 NNUEHashTable::ZOBRIST_basicRule[3];
Hash128 NNUEHashTable::ZOBRIST_VCN[20];
Hash128 NNUEHashTable::ZOBRIST_maxMoves[MaxBS * MaxBS * 2];
Hash128 NNUEHashTable::ZOBRIST_firstPassWin;

Hash128 NNUEHashTable::ZOBRIST_drawBlackWinlossrate_base;
Hash128 NNUEHashTable::ZOBRIST_pda_base;

Hash128 NNUEHashTable::ZOBRIST_movenum[MaxBS * MaxBS * 2];
Hash128 NNUEHashTable::ZOBRIST_blackPassNum[MaxBS * MaxBS * 2];
Hash128 NNUEHashTable::ZOBRIST_whitePassNum[MaxBS * MaxBS * 2];
void NNUEHashTable::initHash(int64_t seed)
{
    std::mt19937_64 r(seed);
    r();
    r();
    for(int c =0;c<4;c++)
        for (int loc = 0; loc < MaxBS * MaxBS; loc++)
        {
            if (c == C_EMPTY)
                ZOBRIST_loc[c][loc] = Hash128();
            else
                ZOBRIST_loc[c][loc] = Hash128(r(), r());

        }

    for (int i = 0; i < 3; i++)
        ZOBRIST_nextPlayer[i] = Hash128(r(), r());
    for (int i = 0; i < MaxBS; i++)
        ZOBRIST_boardH[i] = Hash128(r(), r());
    for (int i = 0; i < MaxBS; i++)
        ZOBRIST_boardW[i] = Hash128(r(), r());
    for (int i = 0; i < 3; i++)
        ZOBRIST_basicRule[i] = Hash128(r(), r());
    for (int i = 0; i < 20; i++)
        ZOBRIST_VCN[i] = Hash128(r(), r());
    for (int i = 0; i < MaxBS * MaxBS * 2; i++)
        ZOBRIST_movenum[i] = Hash128(r(), r());
    for (int i = 0; i < MaxBS * MaxBS * 2; i++)
        ZOBRIST_maxMoves[i] = Hash128(r(), r());
    for (int i = 0; i < MaxBS * MaxBS * 2; i++)
        ZOBRIST_blackPassNum[i] = Hash128(r(), r());
    for (int i = 0; i < MaxBS * MaxBS * 2; i++)
        ZOBRIST_whitePassNum[i] = Hash128(r(), r());

    ZOBRIST_firstPassWin = Hash128(r(), r());
    ZOBRIST_drawBlackWinlossrate_base = Hash128(r(), r());
    ZOBRIST_pda_base = Hash128(r(), r());
}


NNUEHashTable::NNUEHashTable(int sizePowerOfTwo, int mutexPoolSizePowerOfTwo)
{
  NNUEHashTable::initHash(114514);
  tableSize              = ((uint64_t)1) << sizePowerOfTwo;
  tableMask              = tableSize - 1;
  entries                = new Entry[tableSize];
  uint32_t mutexPoolSize = ((uint32_t)1) << mutexPoolSizePowerOfTwo;
  mutexPoolMask          = mutexPoolSize - 1;
  mutexPool              = new MutexPool(mutexPoolSize);
}
NNUEHashTable::~NNUEHashTable()
{
  delete[] entries;
  delete mutexPool;
}

bool NNUEHashTable::get(Hash128 hash, MCTSnode &node)
{
  // Free ret BEFORE locking, to avoid any expensive operations while locked.

  uint64_t idx      = hash.hash0 & tableMask;
  uint32_t mutexIdx = (uint32_t)idx & mutexPoolMask;
  Entry &  entry    = entries[idx];
#ifndef FORGOMOCUP
  std::mutex &mutex = mutexPool->getMutex(mutexIdx);

  std::lock_guard<std::mutex> lock(mutex);
#endif
  if (entry.hash1 == hash.hash1) {
    node.sureResult       = entry.sureResult;
    node.legalChildrennum = entry.legalChildrennum;
    node.WRtotal = entry.WRtotal;
    for (int child = 0; child < MAX_MCTS_CHILDREN; child++) {
      node.children[child].loc = entry.locs[child];
      node.children[child].policy = entry.policy[child];
    }
    return true;
  }
  else return false;
}

void NNUEHashTable::set(Hash128 hash, const MCTSnode &node)
{
  // Immediately copy p right now, before locking, to avoid any expensive operations while
  // locked.

  uint64_t idx = hash.hash0 & tableMask;

#ifndef FORGOMOCUP
  uint32_t mutexIdx = (uint32_t)idx & mutexPoolMask;
#endif
  Entry &entry = entries[idx];
#ifndef FORGOMOCUP
  std::mutex &mutex = mutexPool->getMutex(mutexIdx);
#endif

  {
#ifndef FORGOMOCUP
    std::lock_guard<std::mutex> lock(mutex);
#endif
    // Perform a swap, to avoid any expensive free under the mutex.
    entry.hash1           = hash.hash1;
    entry.sureResult       = node.sureResult;
    entry.legalChildrennum = node.legalChildrennum;
    entry.WRtotal = node.WRtotal;
    for (int child = 0; child < MAX_MCTS_CHILDREN; child++) {
      entry.locs[child] = node.children[child].loc;
      entry.policy[child] = node.children[child].policy;
    }
  }

  // No longer locked, allow buf to fall out of scope now, will free whatever used to be
  // present in the table.
}
