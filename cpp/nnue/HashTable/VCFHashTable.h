#pragma once
#include <iostream>
#include <vector>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include "../core/hash.h"
#include "../search/mutexpool.h"
#include "../NNUEglobal.h"
/*
class MutexPool {
  std::mutex* mutexes;
  uint32_t numMutexes;

public:
  MutexPool(uint32_t n);
  ~MutexPool();

  uint32_t getNumMutexes() const;
  std::mutex& getMutex(uint32_t idx);
};*/

class VCFHashTable_NNUE {
  struct Entry {
    uint64_t hash1;//HASH相撞的概率极低，可以忽略
    int64_t result;
    Entry():hash1(),result(0){}
    ~Entry(){}
  };

  Entry* entries;
  MutexPool* mutexPool;
  uint64_t tableSize;
  uint64_t tableMask;
  uint32_t mutexPoolMask;

public:
  VCFHashTable_NNUE(int sizePowerOfTwo, int mutexPoolSizePowerOfTwo);
  //VCFHashTable()
  //{
  //  VCFHashTable(20, 13);
  //}
  ~VCFHashTable_NNUE();

  VCFHashTable_NNUE(const VCFHashTable_NNUE& other) = delete;
  VCFHashTable_NNUE& operator=(const VCFHashTable_NNUE& other) = delete;

  //These are thread-safe. For get, ret will be set to nullptr upon a failure to find.
  int64_t get(Hash128 hash);
  void set(Hash128 hash, int64_t result);
};
