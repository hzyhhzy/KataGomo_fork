#include "../game/boardhistory.h"

#include <algorithm>

using namespace std;


BoardHistory::BoardHistory()
  :rules(),
   moveHistory(),
   initialBoard(),
   initialPla(P_BLACK),
   initialTurnNumber(0),
   recentBoards(),
   currentRecentBoardIdx(0),
   presumedNextMovePla(P_BLACK),
   isGameFinished(false),winner(C_EMPTY),finalWhiteMinusBlackScore(0.0f),
   isScored(false),isNoResult(false),isResignation(false)
{
}

BoardHistory::~BoardHistory()
{}

BoardHistory::BoardHistory(const Board& board, Player pla, const Rules& r)
  :rules(r),
   moveHistory(),
   initialBoard(),
   initialPla(),
   initialTurnNumber(0),
   recentBoards(),
   currentRecentBoardIdx(0),
   presumedNextMovePla(pla),
   isGameFinished(false),winner(C_EMPTY),finalWhiteMinusBlackScore(0.0f),
   isScored(false),isNoResult(false),isResignation(false)
{

  clear(board,pla,rules);
}

BoardHistory::BoardHistory(const BoardHistory& other)
  :rules(other.rules),
   moveHistory(other.moveHistory),
   initialBoard(other.initialBoard),
   initialPla(other.initialPla),
   initialTurnNumber(other.initialTurnNumber),
   recentBoards(),
   currentRecentBoardIdx(other.currentRecentBoardIdx),
   presumedNextMovePla(other.presumedNextMovePla),
   isGameFinished(other.isGameFinished),winner(other.winner),finalWhiteMinusBlackScore(other.finalWhiteMinusBlackScore),
   isScored(other.isScored),isNoResult(other.isNoResult),isResignation(other.isResignation)
{
  std::copy(other.recentBoards, other.recentBoards+NUM_RECENT_BOARDS, recentBoards);
}


BoardHistory& BoardHistory::operator=(const BoardHistory& other)
{
  if(this == &other)
    return *this;
  rules = other.rules;
  moveHistory = other.moveHistory;
  initialBoard = other.initialBoard;
  initialPla = other.initialPla;
  initialTurnNumber = other.initialTurnNumber;
  std::copy(other.recentBoards, other.recentBoards+NUM_RECENT_BOARDS, recentBoards);
  currentRecentBoardIdx = other.currentRecentBoardIdx;
  presumedNextMovePla = other.presumedNextMovePla;
  isGameFinished = other.isGameFinished;
  winner = other.winner;
  finalWhiteMinusBlackScore = other.finalWhiteMinusBlackScore;
  isScored = other.isScored;
  isNoResult = other.isNoResult;
  isResignation = other.isResignation;

  return *this;
}

BoardHistory::BoardHistory(BoardHistory&& other) noexcept
 :rules(other.rules),
  moveHistory(std::move(other.moveHistory)),
  initialBoard(other.initialBoard),
  initialPla(other.initialPla),
  initialTurnNumber(other.initialTurnNumber),
  recentBoards(),
  currentRecentBoardIdx(other.currentRecentBoardIdx),
  presumedNextMovePla(other.presumedNextMovePla),
  isGameFinished(other.isGameFinished),winner(other.winner),finalWhiteMinusBlackScore(other.finalWhiteMinusBlackScore),
  isScored(other.isScored),isNoResult(other.isNoResult),isResignation(other.isResignation)
{
  std::copy(other.recentBoards, other.recentBoards+NUM_RECENT_BOARDS, recentBoards);
}

BoardHistory& BoardHistory::operator=(BoardHistory&& other) noexcept
{
  rules = other.rules;
  moveHistory = std::move(other.moveHistory);
  initialBoard = other.initialBoard;
  initialPla = other.initialPla;
  initialTurnNumber = other.initialTurnNumber;
  std::copy(other.recentBoards, other.recentBoards+NUM_RECENT_BOARDS, recentBoards);
  currentRecentBoardIdx = other.currentRecentBoardIdx;
  presumedNextMovePla = other.presumedNextMovePla;
  isGameFinished = other.isGameFinished;
  winner = other.winner;
  finalWhiteMinusBlackScore = other.finalWhiteMinusBlackScore;
  isScored = other.isScored;
  isNoResult = other.isNoResult;
  isResignation = other.isResignation;

  return *this;
}

void BoardHistory::clear(const Board& board, Player pla, const Rules& r) {
  rules = r;
  moveHistory.clear();

  initialBoard = board;
  initialPla = pla;
  initialTurnNumber = 0;

  //This makes it so that if we ask for recent boards with a lookback beyond what we have a history for,
  //we simply return copies of the starting board.
  for(int i = 0; i<NUM_RECENT_BOARDS; i++)
    recentBoards[i] = board;
  currentRecentBoardIdx = 0;

  presumedNextMovePla = pla;

  isGameFinished = false;
  winner = C_EMPTY;
  finalWhiteMinusBlackScore = 0.0f;
  isScored = false;
  isNoResult = false;
  isResignation = false;

  //Handle encore phase


}

BoardHistory BoardHistory::copyToInitial() const {
  BoardHistory hist(initialBoard, initialPla, rules);
  hist.setInitialTurnNumber(initialTurnNumber);
  return hist;
}

void BoardHistory::setInitialTurnNumber(int n) {
  initialTurnNumber = n;
}

void BoardHistory::printBasicInfo(ostream& out, const Board& board) const {
  Board::printBoard(out, board, Board::NULL_LOC, &moveHistory);
  out << "Next player: " << PlayerIO::playerToString(presumedNextMovePla) << endl;
  out << "Rules: " << rules.toJsonString() << endl;
}

void BoardHistory::printDebugInfo(ostream& out, const Board& board) const {
  out << board << endl;
  out << "Initial pla " << PlayerIO::playerToString(initialPla) << endl;
  out << "Rules " << rules << endl;
  out << "Presumed next pla " << PlayerIO::playerToString(presumedNextMovePla) << endl;
  out << "Game result " << isGameFinished << " " << PlayerIO::playerToString(winner) << " "
      << finalWhiteMinusBlackScore << " " << isScored << " " << isNoResult << " " << isResignation << endl;
  out << "Last moves ";
  for(int i = 0; i<moveHistory.size(); i++)
    out << Location::toString(moveHistory[i].loc,board) << " ";
  out << endl;
}


const Board& BoardHistory::getRecentBoard(int numMovesAgo) const {
  assert(numMovesAgo >= 0 && numMovesAgo < NUM_RECENT_BOARDS);
  int idx = (currentRecentBoardIdx - numMovesAgo + NUM_RECENT_BOARDS) % NUM_RECENT_BOARDS;
  return recentBoards[idx];
}


void BoardHistory::setKomi(float newKomi) {
  rules.komi = newKomi;

}



float BoardHistory::whiteKomiAdjustmentForDraws(double drawEquivalentWinsForWhite) const {
  //We fold the draw utility into the komi, for input into things like the neural net.
  //Basically we model it as if the final score were jittered by a uniform draw from [-0.5,0.5].
  //E.g. if komi from self perspective is 7 and a draw counts as 0.75 wins and 0.25 losses,
  //then komi input should be as if it was 7.25, which in a jigo game when jittered by 0.5 gives white 75% wins and 25% losses.
  float drawAdjustment = rules.gameResultWillBeInteger() ? (float)(drawEquivalentWinsForWhite - 0.5) : 0.0f;
  return drawAdjustment;
}

float BoardHistory::currentSelfKomi(Player pla, double drawEquivalentWinsForWhite) const {
  float whiteKomiAdjusted =  rules.komi + whiteKomiAdjustmentForDraws(drawEquivalentWinsForWhite);

  if(pla == P_WHITE)
    return whiteKomiAdjusted;
  else if(pla == P_BLACK)
    return -whiteKomiAdjusted;
  else {
    assert(false);
    return 0.0f;
  }
}

void BoardHistory::setWinnerByResignation(Player pla) {
  isGameFinished = true;
  isScored = false;
  isNoResult = false;
  isResignation = true;
  winner = pla;
  finalWhiteMinusBlackScore = 0.0f;
}

void BoardHistory::setWinner(Player pla)
{
  isGameFinished = true;
  isScored = true;
  isNoResult = false;
  isResignation = false;
  winner = pla;
  finalWhiteMinusBlackScore = 0.0f;
  if (pla == C_EMPTY)isNoResult = true;
}


bool BoardHistory::isLegal(const Board& board, Loc moveLoc, Player movePla) const {
  //Ko-moves in the encore that are recapture blocked are interpreted as pass-for-ko, so they are legal

  if(!board.isLegal(moveLoc,movePla,false))
    return false;

  return true;
}

bool BoardHistory::isLegalTolerant(const Board& board, Loc moveLoc, Player movePla) const {
  if (moveLoc == Board::PASS_LOC)
    return true;
  if (!board.isOnBoard(moveLoc))
    return false;
  if (board.colors[moveLoc] == C_EMPTY)
    return true;
  return false;
}

bool BoardHistory::makeBoardMoveTolerant(Board& board, Loc moveLoc, Player movePla) {
  bool multiStoneSuicideLegal = true; //Tolerate suicide regardless of rules
  if(!board.isLegal(moveLoc,movePla,multiStoneSuicideLegal))
    return false;
  makeBoardMoveAssumeLegal(board,moveLoc,movePla);
  return true;
}

void BoardHistory::makeBoardMoveAssumeLegal(Board& board, Loc moveLoc, Player movePla) {
 
  //If somehow we're making a move after the game was ended, just clear those values and continue
  isGameFinished = false;
  winner = C_EMPTY;
  finalWhiteMinusBlackScore = 0.0f;
  isScored = false;
  isNoResult = false;
  isResignation = false;


  //Otherwise handle regular moves
  board.playMoveAssumeLegal(moveLoc,movePla);

  

  //Update recent boards
  currentRecentBoardIdx = (currentRecentBoardIdx + 1) % NUM_RECENT_BOARDS;
  recentBoards[currentRecentBoardIdx] = board;

  moveHistory.push_back(Move(moveLoc,movePla));
  presumedNextMovePla = board.nextPla;

  maybeFinishGame(board,movePla,moveLoc);
}

void BoardHistory::maybeFinishGame(Board& board,Player lastPla,Loc lastLoc)
{
  Loc tmploc;
  if (board.getMaxConnectLengthAndWinLoc(lastPla, tmploc) == 5)
  {
    setWinner(lastPla);
    return;
  }

  if (lastLoc == Board::PASS_LOC)
  {
    setWinner(getOpp(lastPla));
    return;
  }
  if (board.numStonesOnBoard() >= board.x_size * board.y_size-3)setWinner(C_EMPTY);
}


Hash128 BoardHistory::getSituationRulesHash(const Board& board, const BoardHistory& hist, Player nextPlayer, double drawEquivalentWinsForWhite) {
 
  //Note that board.pos_hash also incorporates the size of the board.
  Hash128 hash = board.pos_hash;
  hash ^= Board::ZOBRIST_PLAYER_HASH[nextPlayer];


  float selfKomi = hist.currentSelfKomi(nextPlayer,drawEquivalentWinsForWhite);

  //Discretize the komi for the purpose of matching hash
  int64_t komiDiscretized = (int64_t)(selfKomi*256.0f);
  uint64_t komiHash = Hash::murmurMix((uint64_t)komiDiscretized);
  hash.hash0 ^= komiHash;
  hash.hash1 ^= Hash::basicLCong(komiHash);

  //Fold in the ko, scoring, and suicide rules
  hash ^= Rules::ZOBRIST_TAX_RULE_HASH[hist.rules.taxRule];

  return hash;
}
