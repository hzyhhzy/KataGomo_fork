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
   consecutiveEndingPasses(0),
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
   consecutiveEndingPasses(0),
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
   consecutiveEndingPasses(other.consecutiveEndingPasses),
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
  consecutiveEndingPasses = other.consecutiveEndingPasses;
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
  consecutiveEndingPasses(other.consecutiveEndingPasses),
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
  consecutiveEndingPasses = other.consecutiveEndingPasses;
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

  consecutiveEndingPasses = 0;
  isGameFinished = false;
  winner = C_EMPTY;
  finalWhiteMinusBlackScore = 0.0f;
  isScored = false;
  isNoResult = false;
  isResignation = false;

}

BoardHistory BoardHistory::copyToInitial() const {
  BoardHistory hist(initialBoard, initialPla, rules);
  hist.setInitialTurnNumber(initialTurnNumber);
  return hist;
}

void BoardHistory::setInitialTurnNumber(int64_t n) {
  initialTurnNumber = n;
}


void BoardHistory::printBasicInfo(ostream& out, const Board& board) const {
  Board::printBoard(out, board, Board::NULL_LOC, &moveHistory);
  out << "Next player: " << PlayerIO::playerToString(presumedNextMovePla) << endl;
  out << "Rules: " << rules.toJsonString() << endl;
  out << "B stones captured: " << board.numBlackCaptures << endl;
  out << "W stones captured: " << board.numWhiteCaptures << endl;
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
  float whiteKomiAdjusted = rules.komi + whiteKomiAdjustmentForDraws(drawEquivalentWinsForWhite);

  if(pla == P_WHITE)
    return whiteKomiAdjusted;
  else if(pla == P_BLACK)
    return -whiteKomiAdjusted;
  else {
    assert(false);
    return 0.0f;
  }
}

void BoardHistory::endAndSetWinner(Color winner0, float whiteScore = 0.0) {
  winner = winner0;
  finalWhiteMinusBlackScore = whiteScore;
  isScored = true;
  isNoResult = false;
  isResignation = false;
  isGameFinished = true;
}

int BoardHistory::countAreaScoreWhiteMinusBlack(const Board& board) const {
  ASSERT_UNREACHABLE;  // This function is only for "scoring" games.
  return 0;
}


void BoardHistory::setWinnerByResignation(Player pla) {
  isGameFinished = true;
  isScored = false;
  isNoResult = false;
  isResignation = true;
  winner = pla;
  finalWhiteMinusBlackScore = 0.0f;
}

bool BoardHistory::isLegal(const Board& board, Loc moveLoc, Player movePla) const {
  //Ko-moves in the encore that are recapture blocked are interpreted as pass-for-ko, so they are legal
  {
    //Only check ko bans during normal play.
  }
  if(!board.isLegal(moveLoc,movePla,rules.multiStoneSuicideLegal))
    return false;

  return true;
}

int64_t BoardHistory::getCurrentTurnNumber() const {
  return std::max((int64_t)0,initialTurnNumber + (int64_t)moveHistory.size());
}

//Return the number of consecutive game-ending passes there would be if a pass was made
int BoardHistory::newConsecutiveEndingPassesAfterPass() const {
  int newConsecutiveEndingPasses = consecutiveEndingPasses;
  newConsecutiveEndingPasses++;
  return newConsecutiveEndingPasses;
}


bool BoardHistory::isLegalTolerant(const Board& board, Loc moveLoc, Player movePla) const {
  bool multiStoneSuicideLegal = true; //Tolerate suicide regardless of rules
  if(!board.isLegal(moveLoc,movePla,multiStoneSuicideLegal))
    return false;
  return true;
}
bool BoardHistory::makeBoardMoveTolerant(Board& board, Loc moveLoc, Player movePla) {
  bool multiStoneSuicideLegal = true;  // Tolerate suicide regardless of rules
  if(!board.isLegal(moveLoc, movePla, multiStoneSuicideLegal))
    return false;
  
  makeBoardMoveAssumeLegal(board,moveLoc,movePla);
  return true;
}

void BoardHistory::makeBoardMoveAssumeLegal(Board& board, Loc moveLoc, Player movePla) {
  assert(!isGameFinished);

  //And if somehow we're making a move after the game was ended, just clear those values and continue.
  isGameFinished = false;
  winner = C_EMPTY;
  finalWhiteMinusBlackScore = 0.0f;
  isScored = false;
  isNoResult = false;
  isResignation = false;

  //Update consecutiveEndingPasses and button
  if(moveLoc != Board::PASS_LOC)
    consecutiveEndingPasses = 0;
  else {

    consecutiveEndingPasses = newConsecutiveEndingPassesAfterPass();
  }

  //Otherwise handle regular moves
  board.playMoveAssumeLegal(moveLoc,movePla);


  //Update recent boards
  currentRecentBoardIdx = (currentRecentBoardIdx + 1) % NUM_RECENT_BOARDS;
  recentBoards[currentRecentBoardIdx] = board;

  moveHistory.push_back(Move(moveLoc,movePla));
  presumedNextMovePla = getOpp(movePla);


  Player opp = getOpp(movePla);

  
  if(moveLoc == Board::PASS_LOC)
    endAndSetWinner(opp);

  //Phase transitions and game end
  if(consecutiveEndingPasses >= 2) {
    endAndSetWinner(C_EMPTY);
    ASSERT_UNREACHABLE;
  }

  int myCapture = movePla == C_WHITE ? board.numBlackCaptures : board.numWhiteCaptures;
  int oppCapture = movePla == C_WHITE ? board.numBlackCaptures : board.numWhiteCaptures;
  if(myCapture > 0)
    endAndSetWinner(movePla);
  else if(oppCapture > 0)
    endAndSetWinner(getOpp(movePla));

  //Break long cycles with no-result
  if(moveLoc != Board::PASS_LOC && rules.koRule == Rules::KO_SIMPLE) {
    //static_assert(false, "TODO: find a simple method to detect long cycles");

    if(moveHistory.size() > board.x_size * board.y_size * 5) {
      isNoResult = true;
      isGameFinished = true;
      ASSERT_UNREACHABLE;
    }
  }

}


bool BoardHistory::hasBlackPassOrWhiteFirst() const {
  //First move was made by white this game, on an empty board.
  if(initialBoard.isEmpty() && moveHistory.size() > 0 && moveHistory[0].pla == P_WHITE)
    return true;
  //Black passed exactly once or white doublemoved
  int numBlackPasses = 0;
  int numWhitePasses = 0;
  int numBlackDoubleMoves = 0;
  int numWhiteDoubleMoves = 0;
  for(int i = 0; i<moveHistory.size(); i++) {
    if(moveHistory[i].loc == Board::PASS_LOC && moveHistory[i].pla == P_BLACK)
      numBlackPasses++;
    if(moveHistory[i].loc == Board::PASS_LOC && moveHistory[i].pla == P_WHITE)
      numWhitePasses++;
    if(i > 0 && moveHistory[i].pla == P_BLACK && moveHistory[i-1].pla == P_BLACK)
      numBlackDoubleMoves++;
    if(i > 0 && moveHistory[i].pla == P_WHITE && moveHistory[i-1].pla == P_WHITE)
      numWhiteDoubleMoves++;
  }
  if(numBlackPasses == 1 && numWhitePasses == 0 && numBlackDoubleMoves == 0 && numWhiteDoubleMoves == 0)
    return true;
  if(numBlackPasses == 0 && numWhitePasses == 0 && numBlackDoubleMoves == 0 && numWhiteDoubleMoves == 1)
    return true;

  return false;
}

Hash128 BoardHistory::getSituationAndSimpleKoHash(const Board& board, Player nextPlayer) {
  //Note that board.pos_hash also incorporates the size of the board.
  Hash128 hash = board.pos_hash;
  hash ^= Board::ZOBRIST_PLAYER_HASH[nextPlayer];
  if(board.ko_loc != Board::NULL_LOC)
    hash ^= Board::ZOBRIST_KO_LOC_HASH[board.ko_loc];
  return hash;
}

Hash128 BoardHistory::getSituationAndSimpleKoAndPrevPosHash(const Board& board, const BoardHistory& hist, Player nextPlayer) {
  //Note that board.pos_hash also incorporates the size of the board.
  Hash128 hash = board.pos_hash;
  hash ^= Board::ZOBRIST_PLAYER_HASH[nextPlayer];
  if(board.ko_loc != Board::NULL_LOC)
    hash ^= Board::ZOBRIST_KO_LOC_HASH[board.ko_loc];

  Hash128 mixed;
  mixed.hash1 = Hash::rrmxmx(hash.hash0);
  mixed.hash0 = Hash::splitMix64(hash.hash1);
  if(hist.moveHistory.size() > 0)
    mixed ^= hist.getRecentBoard(1).pos_hash;
  return mixed;
}

Hash128 BoardHistory::getSituationRulesAndKoHash(const Board& board, const BoardHistory& hist, Player nextPlayer, double drawEquivalentWinsForWhite) {
  
  //Note that board.pos_hash also incorporates the size of the board.
  Hash128 hash = board.pos_hash;
  hash ^= Board::ZOBRIST_PLAYER_HASH[nextPlayer];

  if(board.ko_loc != Board::NULL_LOC)
    hash ^= Board::ZOBRIST_KO_LOC_HASH[board.ko_loc];
  

  float selfKomi = hist.currentSelfKomi(nextPlayer,drawEquivalentWinsForWhite);

  //Discretize the komi for the purpose of matching hash
  int64_t komiDiscretized = (int64_t)(selfKomi*256.0f);
  uint64_t komiHash = Hash::murmurMix((uint64_t)komiDiscretized);
  hash.hash0 ^= komiHash;
  hash.hash1 ^= Hash::basicLCong(komiHash);

  //Fold in the ko, scoring, and suicide rules
  hash ^= Rules::ZOBRIST_KO_RULE_HASH[hist.rules.koRule];
  if(hist.rules.multiStoneSuicideLegal)
    hash ^= Rules::ZOBRIST_MULTI_STONE_SUICIDE_HASH;

  // Fold in movenum.
  static constexpr uint64_t m0 = 7607666294965183507ULL;
  static constexpr uint64_t m1 = 3103394289034396213ULL;
  int movenum = hist.moveHistory.size();
  hash.hash0 ^= Hash::nasam(movenum * m0);
  hash.hash1 ^= Hash::murmurMix(movenum * m1);
  return hash;
}



