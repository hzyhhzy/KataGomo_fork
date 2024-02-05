#include "VCFHashTable.h"

using namespace std;


VCFHashTable::VCFHashTable(int sizePowerOfTwo, int mutexPoolSizePowerOfTwo) {

  tableSize = ((uint64_t)1) << sizePowerOfTwo;
  tableMask = tableSize - 1;
  entries = new Entry[tableSize];
  uint32_t mutexPoolSize = ((uint32_t)1) << mutexPoolSizePowerOfTwo;
  mutexPoolMask = mutexPoolSize - 1;
  mutexPool = new MutexPool(mutexPoolSize);
}
VCFHashTable::~VCFHashTable() {
  delete[] entries;
  delete mutexPool;
}

int64_t VCFHashTable::get(Hash128 hash) {
  //Free ret BEFORE locking, to avoid any expensive operations while locked.

  uint64_t idx = hash.hash0 & tableMask;
  uint32_t mutexIdx = (uint32_t)idx & mutexPoolMask;
  Entry& entry = entries[idx];
#ifndef FORGOMOCUP
  std::mutex& mutex = mutexPool->getMutex(mutexIdx);

  std::lock_guard<std::mutex> lock(mutex);
#endif
  if (entry.hash == hash) {
    return entry.result;
  }
  return 0;
}

void VCFHashTable::set(Hash128 hash, int64_t result) {
  //Immediately copy p right now, before locking, to avoid any expensive operations while locked.

  uint64_t idx = hash.hash0 & tableMask;

#ifndef FORGOMOCUP
  uint32_t mutexIdx = (uint32_t)idx & mutexPoolMask;
#endif
  Entry& entry = entries[idx];
#ifndef FORGOMOCUP
  std::mutex& mutex = mutexPool->getMutex(mutexIdx);
#endif

  {
#ifndef FORGOMOCUP
    std::lock_guard<std::mutex> lock(mutex);
#endif
    //Perform a swap, to avoid any expensive free under the mutex.
    entry.hash = hash;
    entry.result = result;
  }

  //No longer locked, allow buf to fall out of scope now, will free whatever used to be present in the table.
}

