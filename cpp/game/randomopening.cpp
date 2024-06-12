#include "../game/randomopening.h"
#include "../game/gamelogic.h"
#include "../core/rand.h"
#include "../search/asyncbot.h"
using namespace RandomOpening;
using namespace std;

static Loc getRandomNearbyMove(Board& board, Rand& gameRand, double avgDist) { 
  int xsize = board.x_size, ysize = board.y_size;
  if(board.isEmpty()) {
    int x = gameRand.nextUInt(xsize - 2) + 1, y = gameRand.nextUInt(ysize - 2) + 1;
    Loc loc = Location::getLoc(x, y, xsize);
    return loc;
  }
  std::vector<double> prob(xsize * ysize, 0);
  for(int x1 = 0; x1 < xsize; x1++)
    for(int y1 = 0; y1 < ysize; y1++) {
      Loc loc = Location::getLoc(x1, y1, xsize);
      if(board.colors[loc] == C_EMPTY)
        continue;
      for(int x2 = 0; x2 < xsize; x2++)
        for(int y2 = 0; y2 < ysize; y2++) {
          Loc loc2 = Location::getLoc(x2, y2, xsize);
          if(board.colors[loc2] != C_EMPTY)
            continue;
          double middleBonusFactor = 2.0;
          double halfBoardLen = std::max(0.5 * (xsize - 1), 0.5 * (ysize - 1));
          double distFromCenter = std::max(std::abs(x2 - 0.5 * (xsize - 1)), std::abs(y2 - 0.5 * (ysize - 1)));
          double middleBonus = middleBonusFactor * (halfBoardLen - distFromCenter) / halfBoardLen;
          double prob_increase =
            (1 + middleBonus) * pow((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1) + avgDist * avgDist, -2);
          prob[y2 * xsize + x2] += prob_increase;
        }
    }

  double totalProb = 0;
  for(int x = 0; x < xsize; x++)
    for(int y = 0; y < ysize; y++) {
      totalProb += prob[y * xsize + x];
    }

  for(int x = 0; x < xsize; x++)
    for(int y = 0; y < ysize; y++) {
      prob[y * xsize + x] /= totalProb;
    }

  double randomDouble = gameRand.nextDouble() - 1e-8;

  double probSum = 0;
  for(int x = 0; x < xsize; x++)
    for(int y = 0; y < ysize; y++) {
      probSum += prob[y * xsize + x];
      if(probSum >= randomDouble) {
        return Location::getLoc(x, y, xsize);
      }
    }

  ASSERT_UNREACHABLE;
}
static double getBoardValue(Search* bot, Board& board, const BoardHistory& hist, Player nextPlayer) {
  NNEvaluator* nnEval = bot->nnEvaluator;
  MiscNNInputParams nnInputParams;
  NNResultBuf buf;
  nnEval->evaluate(board, hist, nextPlayer, nnInputParams, buf, false);
  std::shared_ptr<NNOutput> nnOutput = std::move(buf.result);
  double value = nnOutput->whiteWinProb - nnOutput->whiteLossProb;
  if(nextPlayer == C_BLACK)
    return -value;
  else
    return value;
}
static Loc getBalanceMove(
  Search* botB,
  Search* botW,
  const Board& board,
  const BoardHistory& hist,
  Player nextPlayer,
  Rand& gameRand,
  bool forSelfplay,
  double rejectProb) {
  int xsize = board.x_size, ysize = board.y_size;
  std::vector<double> prob(xsize * ysize, 0);
  double maxProb = 0;
  for(int x = 0; x < xsize; x++)
    for(int y = 0; y < ysize; y++) {
      Loc loc = Location::getLoc(x, y, xsize);

      if(!board.isLegal(loc, nextPlayer))
        continue;

      Board boardCopy(board);
      BoardHistory histCopy(hist);

      histCopy.makeBoardMoveAssumeLegal(boardCopy, loc, nextPlayer);
      if(histCopy.isGameFinished)
        continue;

      Search* bot = gameRand.nextBool(0.5) ? botB : botW;
      double value = getBoardValue(bot, boardCopy, histCopy, getOpp(nextPlayer));

      double p = forSelfplay ? pow(1 - value * value, 4) : pow(1 - value * value, 10);
      maxProb = std::max(maxProb, p);
      prob[y * xsize + x] = p;
    }
  if(gameRand.nextBool(1 - maxProb) && gameRand.nextBool(rejectProb)) {
    return Board::NULL_LOC;
  }

  double totalProb = 0;
  for(int x = 0; x < xsize; x++)
    for(int y = 0; y < ysize; y++) {
      totalProb += prob[y * xsize + x];
    }
  for(int x = 0; x < xsize; x++)
    for(int y = 0; y < ysize; y++) {
      prob[y * xsize + x] /= totalProb;
    }

  double randomDouble = gameRand.nextDouble() - 1e-8;

  double probSum = 0;
  for(int x = 0; x < xsize; x++)
    for(int y = 0; y < ysize; y++) {
      probSum += prob[y * xsize + x];
      if(probSum >= randomDouble) {
        return Location::getLoc(x, y, xsize);
      }
    }

  // some rare conditions, return NULL_LOC.
  std::cout << "totalProb=" << totalProb << ", probSum=" << probSum << " in getBalanceMove(), Rule=" << hist.rules.toString()
       << std::endl;
  return Board::NULL_LOC;
  /*while (1)
  {
    int x = gameRand.nextUInt(xsize);
    int y = gameRand.nextUInt(ysize);
    Loc loc=Location::getLoc(x, y, xsize);
    if (board.isLegal(loc, nextPlayer, true))
      return loc;
  }
  ASSERT_UNREACHABLE;*/
}
static bool tryInitializeBalancedRandomOpening(
  Search* botB,
  Search* botW,
  Board& board,
  BoardHistory& hist,
  Player& nextPlayer,
  Rand& gameRand,
  bool forSelfplay,
  double rejectProb) {
  Board boardCopy(board);
  BoardHistory histCopy(hist);
  Player nextPlayerCopy = nextPlayer;

  std::vector<float> randomMoveNumProb;

  if(hist.rules.VCNRule == Rules::VCNRULE_NOVC)
    randomMoveNumProb = vector<float>{10, 30, 50, 80, 60, 40, 20, 10, 5, 1, 0, 0};
  else if(hist.rules.VCNRule == Rules::VCNRULE_VC1_B)
    randomMoveNumProb = vector<float>{0.03, 0.03, 25, 20, 15, 10, 5, 1, 0, 0, 0, 0};
  else if(hist.rules.VCNRule == Rules::VCNRULE_VC1_W)
    randomMoveNumProb = vector<float>{0.03, 0.03, 0.03, 20, 15, 10, 5, 1, 0, 0, 0, 0};
  else if(hist.rules.VCNRule == Rules::VCNRULE_VC2_B)
    randomMoveNumProb = vector<float>{0.01, 0.01, 0.03, 0.03, 35, 30, 25, 20, 15, 10, 5, 1};
  else if(hist.rules.VCNRule == Rules::VCNRULE_VC2_W)
    randomMoveNumProb = vector<float>{0.01, 0.01, 0.03, 0.03, 0.03, 30, 25, 20, 15, 10, 5, 1};
  else
    cout << Rules::writeVCNRule(hist.rules.VCNRule) << " does not support balanced openings init" << endl;
  int maxRandomMoveNum = randomMoveNumProb.size();

  static const double avgRandomDistFactor = 0.8;

  double randomMoveNumProbTotal = 0;
  for(int i = 0; i < maxRandomMoveNum; i++)
    randomMoveNumProbTotal += randomMoveNumProb[i];
  double randomMoveNumProbSum = 0;
  double randomMoveNumProbRandomDouble = gameRand.nextDouble() * randomMoveNumProbTotal - 1e-7;
  int randomMoveNum = -1;
  for(int i = 0; i < maxRandomMoveNum; i++) {
    randomMoveNumProbSum += randomMoveNumProb[i];
    if(randomMoveNumProbSum >= randomMoveNumProbRandomDouble) {
      randomMoveNum = i;
      break;
    }
  }
  if(randomMoveNum == -1)
    ASSERT_UNREACHABLE;

  double avgDist = gameRand.nextExponential() * avgRandomDistFactor;
  for(int i = 0; i < randomMoveNum; i++) {
    Loc randomLoc = getRandomNearbyMove(boardCopy, gameRand, avgDist);
    histCopy.makeBoardMoveAssumeLegal(boardCopy, randomLoc, nextPlayerCopy);
    if(histCopy.isGameFinished)
      return false;
    nextPlayerCopy = getOpp(nextPlayerCopy);
  }
  Loc balancedMove = getBalanceMove(botB, botW, boardCopy, histCopy, nextPlayerCopy, gameRand, forSelfplay, rejectProb);
  if(balancedMove == Board::NULL_LOC)
    return false;
  histCopy.makeBoardMoveAssumeLegal(boardCopy, balancedMove, nextPlayerCopy);
  if(histCopy.isGameFinished)
    return false;
  nextPlayerCopy = getOpp(nextPlayerCopy);

  board = boardCopy;
  hist = histCopy;
  nextPlayer = nextPlayerCopy;
  return true;
}
void RandomOpening::initializeBalancedRandomOpening(
  Search* botB,
  Search* botW,
  Board& board,
  BoardHistory& hist,
  Player& nextPlayer,
  Rand& gameRand,
  bool forSelfplay) {
  static const int maxTryTimes = 20;
  int tryTimes = 0;
  double rejectProb = 0.995;
  while(!tryInitializeBalancedRandomOpening(botB, botW, board, hist, nextPlayer, gameRand, forSelfplay, rejectProb)) {
    tryTimes++;
    if(tryTimes > maxTryTimes) {
      tryTimes = 0;
      std::cout << "Reached max trying times for finding balanced openings, Rule=" << hist.rules.toString() << std::endl;
      rejectProb = 0.8;
    }
  }
}