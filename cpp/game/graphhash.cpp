#include "../game/graphhash.h"

Hash128 GraphHash::getStateHash(const BoardHistory& hist, Player nextPlayer) {
  const Board& board = hist.getRecentBoard(0);
  Hash128 hash = BoardHistory::getSituationRulesHash(board, hist, nextPlayer);

  // Fold in whether the game is over or not
  if(hist.isGameFinished)
    hash ^= Board::ZOBRIST_GAME_IS_OVER;

  return hash;
}

Hash128 GraphHash::getGraphHash(const BoardHistory& hist, Player nextPlayer) {
  return getStateHash(hist, nextPlayer);
}

Hash128
GraphHash::getGraphHashFromScratch(const BoardHistory& histOrig, Player nextPlayer) {
  BoardHistory hist = histOrig.copyToInitial();
  Board board = hist.getRecentBoard(0);
  Hash128 graphHash = Hash128();

  for(size_t i = 0; i<histOrig.moveHistory.size(); i++) {
    bool suc = hist.makeBoardMoveTolerant(board, histOrig.moveHistory[i].loc, histOrig.moveHistory[i].pla);
    assert(suc);
  }
  assert(
    BoardHistory::getSituationRulesHash(board, hist, nextPlayer) ==
    BoardHistory::getSituationRulesHash(
      histOrig.getRecentBoard(0), histOrig, nextPlayer)
  );

  graphHash = getGraphHash(hist, nextPlayer);
  return graphHash;
}

