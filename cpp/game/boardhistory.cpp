#include "../game/boardhistory.h"
#include "../game/gamelogic.h"
#include <algorithm>

using namespace std;



BoardHistory::BoardHistory()
  :rules(),
   moveHistory(),
   posHashHistoryCount(),
   initialBoard(),
   initialPla(P_BLACK),
   initialTurnNumber(0),
   recentBoards(),
   currentRecentBoardIdx(0),
   presumedNextMovePla(P_BLACK),
   isGameFinished(false),winner(C_EMPTY),
   isNoResult(false),isResignation(false)
{
}

BoardHistory::~BoardHistory()
{}

BoardHistory::BoardHistory(const Board& board, Player pla, const Rules& r)
  :rules(r),
   moveHistory(),
   posHashHistoryCount(),
   initialBoard(),
   initialPla(),
   initialTurnNumber(0),
   recentBoards(),
   currentRecentBoardIdx(0),
   presumedNextMovePla(pla),
   isGameFinished(false),winner(C_EMPTY),
   isNoResult(false),isResignation(false)
{

  clear(board,pla,rules);
}

BoardHistory::BoardHistory(const BoardHistory& other)
  :rules(other.rules),
   moveHistory(other.moveHistory),
   posHashHistoryCount(other.posHashHistoryCount),
   initialBoard(other.initialBoard),
   initialPla(other.initialPla),
   initialTurnNumber(other.initialTurnNumber),
   recentBoards(),
   currentRecentBoardIdx(other.currentRecentBoardIdx),
   presumedNextMovePla(other.presumedNextMovePla),
   isGameFinished(other.isGameFinished),winner(other.winner),
   isNoResult(other.isNoResult),isResignation(other.isResignation)
{
  std::copy(other.recentBoards, other.recentBoards+NUM_RECENT_BOARDS, recentBoards);
}


BoardHistory& BoardHistory::operator=(const BoardHistory& other)
{
  if(this == &other)
    return *this;
  rules = other.rules;
  moveHistory = other.moveHistory;
  posHashHistoryCount = other.posHashHistoryCount;
  initialBoard = other.initialBoard;
  initialPla = other.initialPla;
  initialTurnNumber = other.initialTurnNumber;
  std::copy(other.recentBoards, other.recentBoards+NUM_RECENT_BOARDS, recentBoards);
  currentRecentBoardIdx = other.currentRecentBoardIdx;
  presumedNextMovePla = other.presumedNextMovePla;
  isGameFinished = other.isGameFinished;
  winner = other.winner;
  isNoResult = other.isNoResult;
  isResignation = other.isResignation;

  return *this;
}

BoardHistory::BoardHistory(BoardHistory&& other) noexcept
 :rules(other.rules),
  moveHistory(std::move(other.moveHistory)),
  posHashHistoryCount(std::move(other.posHashHistoryCount)),
  initialBoard(other.initialBoard),
  initialPla(other.initialPla),
  initialTurnNumber(other.initialTurnNumber),
  recentBoards(),
  currentRecentBoardIdx(other.currentRecentBoardIdx),
  presumedNextMovePla(other.presumedNextMovePla),
  isGameFinished(other.isGameFinished),winner(other.winner),
  isNoResult(other.isNoResult),isResignation(other.isResignation)
{
  std::copy(other.recentBoards, other.recentBoards+NUM_RECENT_BOARDS, recentBoards);
}

BoardHistory& BoardHistory::operator=(BoardHistory&& other) noexcept
{
  rules = other.rules;
  moveHistory = std::move(other.moveHistory);
  posHashHistoryCount = std::move(other.posHashHistoryCount);
  initialBoard = other.initialBoard;
  initialPla = other.initialPla;
  initialTurnNumber = other.initialTurnNumber;
  std::copy(other.recentBoards, other.recentBoards+NUM_RECENT_BOARDS, recentBoards);
  currentRecentBoardIdx = other.currentRecentBoardIdx;
  presumedNextMovePla = other.presumedNextMovePla;
  isGameFinished = other.isGameFinished;
  winner = other.winner;
  isNoResult = other.isNoResult;
  isResignation = other.isResignation;

  return *this;
}

void BoardHistory::clear(const Board& board, Player pla, const Rules& r) {
  rules = r;
  moveHistory.clear();
  posHashHistoryCount.clear();

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
  isNoResult = false;
  isResignation = false;

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
      << isNoResult << " " << isResignation << endl;
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





void BoardHistory::setWinnerByResignation(Player pla) {
  isGameFinished = true;
  isNoResult = false;
  isResignation = true;
  winner = pla;
}

void BoardHistory::setWinner(Player pla) {
  isGameFinished = true;
  isNoResult = false;
  isResignation = false;
  winner = pla;
}

bool BoardHistory::isLegal(const Board& board, Loc moveLoc, Player movePla) const {
  if(!board.isLegal(moveLoc,movePla))
    return false;

  return true;
}




bool BoardHistory::isLegalTolerant(const Board& board, Loc moveLoc, Player movePla) const {
  return board.isLegal(moveLoc, movePla);
}
bool BoardHistory::makeBoardMoveTolerant(Board& board, Loc moveLoc, Player movePla) {
  if(!board.isLegal(moveLoc,movePla))
    return false;
  makeBoardMoveAssumeLegal(board,moveLoc,movePla);
  return true;
}


void BoardHistory::makeBoardMoveAssumeLegal(Board& board, Loc moveLoc, Player movePla) {

  //If somehow we're making a move after the game was ended, just clear those values and continue
  isGameFinished = false;
  winner = C_EMPTY;
  isNoResult = false;
  isResignation = false;

  
  int hashCountBeforePlay = posHashHistoryCount.count(board.pos_hash);
  bool isCopyStone = board.stage == 0 && board.colors[moveLoc]==C_EMPTY;

  if(isCopyStone || hashCountBeforePlay>=2)
    posHashHistoryCount.clear();

  bool isLegalPass = moveLoc == Board::PASS_LOC && (!GameLogic::hasLegalMove(board));

  board.playMoveAssumeLegal(moveLoc,movePla);

  
  Hash128 h = board.pos_hash;
  int c = posHashHistoryCount.count(h);
  if(c == 0)
    posHashHistoryCount[h] = 1;
  else
    posHashHistoryCount[h] += 1;

  //Update recent boards
  currentRecentBoardIdx = (currentRecentBoardIdx + 1) % NUM_RECENT_BOARDS;
  recentBoards[currentRecentBoardIdx] = board;

  moveHistory.push_back(Move(moveLoc,movePla));
  presumedNextMovePla = board.nextPla;
  Color maybeWinner = GameLogic::checkWinnerAfterPlayed(board, *this, movePla, moveLoc,isLegalPass);
  if(maybeWinner!=C_WALL) { //game finished
    setWinner(maybeWinner);
  }

}



Hash128 BoardHistory::getSituationRulesHash(const Board& board, const BoardHistory& hist, Player nextPlayer) {
 //Note that board.pos_hash also incorporates the size of the board.
  Hash128 hash = board.pos_hash;
  hash ^= Board::ZOBRIST_PLAYER_HASH[nextPlayer];

  //Fold in the ko, scoring, and suicide rules
  hash ^= Rules::ZOBRIST_LOOPPASS_RULE_HASH[hist.rules.loopPassRule];
  if(hist.rules.komi != 0) {
    hash = Hash128(Hash::murmurMix(hash.hash0 + hist.rules.komi), Hash::nasam(hash.hash1 - hist.rules.komi));
  }


  return hash;
}


