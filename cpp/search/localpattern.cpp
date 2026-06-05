#include "../search/localpattern.h"

#include "../neuralnet/nninputs.h"

using namespace std;

LocalPatternHasher::LocalPatternHasher()
  : xSize(),
    ySize(),
    zobristLocalPattern(),
    zobristPla(),
    zobristAtari()
{}


void LocalPatternHasher::init(int x, int y, Rand& rand) {
  xSize = x;
  ySize = y;
  assert(xSize > 0 && xSize % 2 == 1);
  assert(ySize > 0 && ySize % 2 == 1);
  zobristLocalPattern.resize(NUM_BOARD_COLORS * xSize * ySize);
  zobristPla.resize(NUM_BOARD_COLORS);
  zobristAtari.resize(xSize * ySize);

  for(int i = 0; i<NUM_BOARD_COLORS; i++) {
    for(int dy = 0; dy<ySize; dy++) {
      for(int dx = 0; dx<xSize; dx++) {
        uint64_t h0 = rand.nextUInt64();
        uint64_t h1 = rand.nextUInt64();
        zobristLocalPattern[i * ySize*xSize + dy*xSize + dx] = Hash128(h0,h1);
      }
    }
  }
  for(int i = 0; i<NUM_BOARD_COLORS; i++) {
    uint64_t h0 = rand.nextUInt64();
    uint64_t h1 = rand.nextUInt64();
    zobristPla[i] = Hash128(h0,h1);
  }
  for(int dy = 0; dy<ySize; dy++) {
    for(int dx = 0; dx<xSize; dx++) {
      uint64_t h0 = rand.nextUInt64();
      uint64_t h1 = rand.nextUInt64();
      zobristAtari[dy*xSize + dx] = Hash128(h0,h1);
    }
  }
}

LocalPatternHasher::~LocalPatternHasher() {
}


Hash128 LocalPatternHasher::getHash(const Board& board, Loc loc, Player pla) const {
  (void)board;
  (void)loc;
  (void)pla;
  assert(false);
  throw StringError("Local pattern hashing is disabled for toroidal boards");
  return Hash128();
}

Hash128 LocalPatternHasher::getHashWithSym(const Board& board, Loc loc, Player pla, int symmetry, bool flipColors) const {
  (void)board;
  (void)loc;
  (void)pla;
  (void)symmetry;
  (void)flipColors;
  assert(false);
  throw StringError("Local pattern hashing with symmetry is disabled for toroidal boards");
  return Hash128();
}
