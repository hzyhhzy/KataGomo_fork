#ifndef GAME_BOARDHISTORY_H_
#define GAME_BOARDHISTORY_H_

#include "../core/global.h"
#include "../core/hash.h"
#include "../game/board.h"
#include "../game/rules.h"

struct KoHashTable;

//A data structure enabling checking of move legality, including optionally superko,
//and implements scoring and support for various rulesets (see rules.h)
struct BoardHistory {
  Rules rules;

  //Chronological history of moves
  std::vector<Move> moveHistory;

  //The board and player to move as of the very start, before moveHistory.
  Board initialBoard;
  Player initialPla;
  //The "turn number" as of the initial board. Does not affect any rules, but possibly uses may
  //care about this number, for cases where we set up a position from midgame.
  int initialTurnNumber;

  static const int NUM_RECENT_BOARDS = 6;
  Board recentBoards[NUM_RECENT_BOARDS];
  int currentRecentBoardIdx;
  Player presumedNextMovePla;





  //Is the game supposed to be ended now?
  bool isGameFinished;
  //Winner of the game if the game is supposed to have ended now, C_EMPTY if it is a draw or isNoResult.
  Player winner;
  //Score difference of the game if the game is supposed to have ended now, does NOT take into account whiteKomiAdjustmentForDrawUtility
  //Always an integer or half-integer.

  //不删，以后可能用
  float finalWhiteMinusBlackScore;

  //True if this game is supposed to be ended with a score
  bool isScored;
  //True if this game is supposed to be ended but there is no result
  bool isNoResult;
  //True if this game is supposed to be ended but it was by resignation rather than an actual end position
  bool isResignation;

  BoardHistory();
  ~BoardHistory();

  BoardHistory(const Board& board, Player pla, const Rules& rules);

  BoardHistory(const BoardHistory& other);
  BoardHistory& operator=(const BoardHistory& other);

  BoardHistory(BoardHistory&& other) noexcept;
  BoardHistory& operator=(BoardHistory&& other) noexcept;

  //Clears all history and status and bonus points, sets encore phase and rules
  void clear(const Board& board, Player pla, const Rules& rules);
  //Set only the komi field of the rules, does not clear history, does recompute game score if game is over.
  void setKomi(float newKomi);
  //Set the initial turn number. Affects nothing else.
  void setInitialTurnNumber(int n);

  //Returns a copy of this board history rewound to the initial board, pla, etc, with other fields
  //(such as setInitialTurnNumber, setAssumeMultipleStartingBlackMovesAreHandicap) set identically.
  BoardHistory copyToInitial() const;

  float whiteKomiAdjustmentForDraws(double drawEquivalentWinsForWhite) const;
  float currentSelfKomi(Player pla, double drawEquivalentWinsForWhite) const;

  //Returns a reference a recent board state, where 0 is the current board, 1 is 1 move ago, etc.
  //Requires that numMovesAgo < NUM_RECENT_BOARDS
  const Board& getRecentBoard(int numMovesAgo) const;

  //Check if a move on the board is legal, taking into account the full game state and superko
  bool isLegal(const Board& board, Loc moveLoc, Player movePla) const;

  //For all of the below, rootKoHashTable is optional and if provided will slightly speedup superko searches
  //This function should behave gracefully so long as it is pseudolegal (board.isLegal, but also still ok if the move is on board.ko_loc)
  //even if the move violates superko or encore ko recapture prohibitions, or is past when the game is ended.
  //This allows for robustness when this code is being used for analysis or with external data sources.
  //preventEncore artifically prevents any move from entering or advancing the encore phase when using territory scoring.
  void makeBoardMoveAssumeLegal(Board& board, Loc moveLoc, Player movePla);
  void maybeFinishGame(Board& board,Player lastPla,Loc lastLoc);
  //Make a move with legality checking, but be mostly tolerant and allow moves that can still be handled but that may not technically
  //be legal. This is intended for reading moves from SGFs and such where maybe we're getting moves that were played in a different
  //ruleset than ours. Returns true if successful, false if was illegal even unter tolerant rules.
  bool makeBoardMoveTolerant(Board& board, Loc moveLoc, Player movePla);
  bool isLegalTolerant(const Board& board, Loc moveLoc, Player movePla) const;


  void setWinnerByResignation(Player pla);
  void setWinner(Player pla);
  void setScore(float whiteScore);

  void printBasicInfo(std::ostream& out, const Board& board) const;
  void printDebugInfo(std::ostream& out, const Board& board) const;


  //Compute a hash that takes into account the full situation, the rules, discretized komi, and any immediate ko prohibitions.
  static Hash128 getSituationRulesHash(const Board& board, const BoardHistory& hist, Player nextPlayer, double drawEquivalentWinsForWhite);

private:
};


#endif  // GAME_BOARDHISTORY_H_
