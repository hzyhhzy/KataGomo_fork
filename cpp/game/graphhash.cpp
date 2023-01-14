#include "../game/graphhash.h"

Hash128 GraphHash::getStateHash(const BoardHistory& hist, Player nextPlayer, double drawEquivalentWinsForWhite) {
  const Board& board = hist.getRecentBoard(0);
  Hash128 hash = BoardHistory::getSituationRulesAndKoHash(board, hist, nextPlayer, drawEquivalentWinsForWhite);

  // Fold in whether the game is over or not
  if(hist.isGameFinished)
    hash ^= Board::ZOBRIST_GAME_IS_OVER;

  return hash;
}

Hash128 GraphHash::getGraphHash(const BoardHistory& hist, Player nextPlayer, double drawEquivalentWinsForWhite) {
    return getStateHash(hist,nextPlayer,drawEquivalentWinsForWhite);
}

Hash128 GraphHash::getGraphHashFromScratch(const BoardHistory& histOrig, Player nextPlayer, double drawEquivalentWinsForWhite) {
  BoardHistory hist = histOrig.copyToInitial();
  Board board = hist.getRecentBoard(0);
  Hash128 graphHash = Hash128();

  for(size_t i = 0; i<histOrig.moveHistory.size(); i++) {
    bool suc = hist.makeBoardMoveTolerant(board, histOrig.moveHistory[i].loc, histOrig.moveHistory[i].pla);
    assert(suc);
  }
  assert(
    BoardHistory::getSituationRulesAndKoHash(board, hist, nextPlayer, drawEquivalentWinsForWhite) ==
    BoardHistory::getSituationRulesAndKoHash(histOrig.getRecentBoard(0), histOrig, nextPlayer, drawEquivalentWinsForWhite)
  );

  graphHash = getGraphHash(hist, nextPlayer, drawEquivalentWinsForWhite);
  return graphHash;
}

