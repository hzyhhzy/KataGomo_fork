#include "../game/graphhash.h"

Hash128 GraphHash::getStateHash(const BoardHistory& hist, Player nextPlayer, double noResultUtilityForWhite) {
  const Board& board = hist.getRecentBoard(0);
  Hash128 hash = BoardHistory::getSituationRulesHash(board, hist, nextPlayer, noResultUtilityForWhite);

  // Fold in whether the game is over or not
  if(hist.isGameFinished)
    hash ^= Board::ZOBRIST_GAME_IS_OVER;

  // Fold in consecutive pass count. Probably usually redundant with history tracking. Use some standard LCG constants.
  //static constexpr uint64_t CONSECPASS_MULT0 = 2862933555777941757ULL;
  //static constexpr uint64_t CONSECPASS_MULT1 = 3202034522624059733ULL;
  //hash.hash0 += CONSECPASS_MULT0 * (uint64_t)hist.consecutiveEndingPasses;
  //hash.hash1 += CONSECPASS_MULT1 * (uint64_t)hist.consecutiveEndingPasses;
  return hash;
}

Hash128 GraphHash::getGraphHash(Hash128 prevGraphHash, const BoardHistory& hist, Player nextPlayer, int repBound, double noResultUtilityForWhite) {
  (void)prevGraphHash;
  (void)repBound;
  return getStateHash(hist,nextPlayer,noResultUtilityForWhite);
}

Hash128 GraphHash::getGraphHashFromScratch(const BoardHistory& histOrig, Player nextPlayer, int repBound, double noResultUtilityForWhite) {
 // BoardHistory hist = histOrig.copyToInitial();
 // Board board = hist.getRecentBoard(0);
 // Hash128 graphHash = Hash128();

 // for(size_t i = 0; i<histOrig.moveHistory.size(); i++) {
 //   graphHash = getGraphHash(graphHash, hist, histOrig.moveHistory[i].pla, repBound, drawEquivalentWinsForWhite);
 //   bool suc = hist.makeBoardMoveTolerant(board, histOrig.moveHistory[i].loc, histOrig.moveHistory[i].pla);
 //   assert(suc);
 // }
 // assert(
 //   BoardHistory::getSituationRulesAndKoHash(board, hist, nextPlayer, drawEquivalentWinsForWhite) ==
 //   BoardHistory::getSituationRulesAndKoHash(histOrig.getRecentBoard(0), histOrig, nextPlayer, drawEquivalentWinsForWhite)
 // );
  (void)repBound;
  Hash128 graphHash = getGraphHash(Hash128(), histOrig, nextPlayer, repBound, noResultUtilityForWhite);
  return graphHash;
}

