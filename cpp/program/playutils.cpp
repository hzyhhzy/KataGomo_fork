#include "../program/playutils.h"
#include "../core/timer.h"

#include <sstream>

using namespace std;

ExtraBlackAndKomi PlayUtils::chooseKomi(
  float base, float stdev, double allowIntegerProb, 
  double bigStdevProb, float bigStdev, double sqrtBoardArea, Rand& rand
) {
  float komi = base;

  float stdevToUse = 0.0f;
  if(stdev > 0.0f)
    stdevToUse = stdev;
  if(bigStdev > 0.0f && rand.nextBool(bigStdevProb))
    stdevToUse = bigStdev;
  //Adjust for board size, so that we don't give the same massive komis on smaller boards
  stdevToUse = stdevToUse * (float)(sqrtBoardArea / 19.0);

  bool allowInteger = rand.nextBool(allowIntegerProb);

  ExtraBlackAndKomi ret;
  ret.komiMean = komi;
  ret.komiStdev = stdevToUse;
  //These two are set later
  ret.makeGameFair = false;
  ret.makeGameFairForEmptyBoard = false;
  //This is recorded for application later, since other things may adjust the komi in between.
  ret.allowInteger = allowInteger;
  return ret;
}


static float roundKomiWithLinearProb(float komi, Rand& rand) {
  //Discretize komi
  float lower = floor(komi*2.0f) / 2.0f;
  float upper = ceil(komi*2.0f) / 2.0f;

  if(lower == upper)
    komi = lower;
  else {
    assert(upper > lower);
    if(rand.nextDouble() < (komi - lower) / (upper - lower))
      komi = upper;
    else
      komi = lower;
  }
  return komi;
}

//Also ignores allowInteger
void PlayUtils::setKomiWithoutNoise(const ExtraBlackAndKomi& extraBlackAndKomi, BoardHistory& hist) {
  float komi = extraBlackAndKomi.komiMean;
  komi = roundAndClipKomi(komi, hist.getRecentBoard(0), false);
  assert(Rules::komiIsIntOrHalfInt(komi));
  hist.setKomi(komi);
}

void PlayUtils::setKomiWithNoise(const ExtraBlackAndKomi& extraBlackAndKomi, BoardHistory& hist, Rand& rand) {
  float komi = extraBlackAndKomi.komiMean;
  if(extraBlackAndKomi.komiStdev > 0)
    komi += extraBlackAndKomi.komiStdev * (float)rand.nextGaussianTruncated(3.0);
  komi = roundKomiWithLinearProb(komi,rand);
  komi = roundAndClipKomi(komi, hist.getRecentBoard(0), false);
  assert(Rules::komiIsIntOrHalfInt(komi));
  if(!extraBlackAndKomi.allowInteger && komi == (int)komi)
    komi += rand.nextBool(0.5) ? (-0.5f) : (0.5f);
  hist.setKomi(komi);
}


Loc PlayUtils::chooseRandomLegalMove(const Board& board, const BoardHistory& hist, Player pla, Rand& gameRand, Loc banMove) {
  int numLegalMoves = 0;
  Loc locs[Board::MAX_ARR_SIZE];
  for(Loc loc = 0; loc < Board::MAX_ARR_SIZE; loc++) {
    if(hist.isLegal(board,loc,pla) && loc != banMove) {
      locs[numLegalMoves] = loc;
      numLegalMoves += 1;
    }
  }
  if(numLegalMoves > 0) {
    int n = gameRand.nextUInt(numLegalMoves);
    return locs[n];
  }
  return Board::NULL_LOC;
}

int PlayUtils::chooseRandomLegalMoves(const Board& board, const BoardHistory& hist, Player pla, Rand& gameRand, Loc* buf, int len) {
  int numLegalMoves = 0;
  Loc locs[Board::MAX_ARR_SIZE];
  for(Loc loc = 0; loc < Board::MAX_ARR_SIZE; loc++) {
    if(hist.isLegal(board,loc,pla)) {
      locs[numLegalMoves] = loc;
      numLegalMoves += 1;
    }
  }
  if(numLegalMoves > 0) {
    for(int i = 0; i<len; i++) {
      int n = gameRand.nextUInt(numLegalMoves);
      buf[i] = locs[n];
    }
    return len;
  }
  return 0;
}


Loc PlayUtils::chooseRandomPolicyMove(
  const NNOutput* nnOutput, const Board& board, const BoardHistory& hist, Player pla, Rand& gameRand, double temperature, bool allowPass, Loc banMove
) {
  const float* policyProbs = nnOutput->policyProbs;
  int nnXLen = nnOutput->nnXLen;
  int nnYLen = nnOutput->nnYLen;
  int numLegalMoves = 0;
  double relProbs[NNPos::MAX_NN_POLICY_SIZE];
  int locs[NNPos::MAX_NN_POLICY_SIZE];
  for(int pos = 0; pos<NNPos::MAX_NN_POLICY_SIZE; pos++) {
    Loc loc = NNPos::posToLoc(pos,board.x_size,board.y_size,nnXLen,nnYLen);
    if((loc == Board::PASS_LOC && !allowPass) || loc == banMove)
      continue;
    if(policyProbs[pos] > 0.0 && hist.isLegal(board,loc,pla)) {
      double relProb = policyProbs[pos];
      relProbs[numLegalMoves] = relProb;
      locs[numLegalMoves] = loc;
      numLegalMoves += 1;
    }
  }

  //Just in case the policy map is somehow not consistent with the board position
  if(numLegalMoves > 0) {
    uint32_t n = Search::chooseIndexWithTemperature(gameRand, relProbs, numLegalMoves, temperature);
    return locs[n];
  }
  return Board::NULL_LOC;
}


Loc PlayUtils::getGameInitializationMove(
  Search* botB, Search* botW, Board& board, const BoardHistory& hist, Player pla, NNResultBuf& buf,
  Rand& gameRand, double temperature
) {
  NNEvaluator* nnEval = (pla == P_BLACK ? botB : botW)->nnEvaluator;
  MiscNNInputParams nnInputParams;
  nnInputParams.drawEquivalentWinsForWhite = (pla == P_BLACK ? botB : botW)->searchParams.drawEquivalentWinsForWhite;
  nnEval->evaluate(board,hist,pla,nnInputParams,buf,false,false);
  std::shared_ptr<NNOutput> nnOutput = std::move(buf.result);

  vector<Loc> locs;
  vector<double> playSelectionValues;
  int nnXLen = nnOutput->nnXLen;
  int nnYLen = nnOutput->nnYLen;
  assert(nnXLen >= board.x_size);
  assert(nnYLen >= board.y_size);
  assert(nnXLen > 0 && nnXLen < 100); //Just a sanity check to make sure no other crazy values have snuck in
  assert(nnYLen > 0 && nnYLen < 100); //Just a sanity check to make sure no other crazy values have snuck in
  int policySize = NNPos::getPolicySize(nnXLen,nnYLen);
  for(int movePos = 0; movePos<policySize; movePos++) {
    Loc moveLoc = NNPos::posToLoc(movePos,board.x_size,board.y_size,nnXLen,nnYLen);
    double policyProb = nnOutput->policyProbs[movePos];
    if(!hist.isLegal(board,moveLoc,pla) || policyProb <= 0)
      continue;
    locs.push_back(moveLoc);
    playSelectionValues.push_back(pow(policyProb,1.0/temperature));
  }

  //In practice, this should never happen, but in theory, a very badly-behaved net that rounds
  //all legal moves to zero could result in this. We still go ahead and fail, since this more likely some sort of bug.
  if(playSelectionValues.size() <= 0)
    throw StringError("getGameInitializationMove: playSelectionValues.size() <= 0");

  //With a tiny probability, choose a uniformly random move instead of a policy move, to also
  //add a bit more outlierish variety
  uint32_t idxChosen;
  if(gameRand.nextBool(0.0002))
    idxChosen = gameRand.nextUInt(playSelectionValues.size());
  else
    idxChosen = gameRand.nextUInt(playSelectionValues.data(),playSelectionValues.size());
  Loc loc = locs[idxChosen];
  return loc;
}


//Try playing a bunch of pure policy moves instead of playing from the start to initialize the board
//and add entropy
void PlayUtils::initializeGameUsingPolicy(
  Search* botB, Search* botW, Board& board, BoardHistory& hist, Player& pla,
  Rand& gameRand,
  double proportionOfBoardArea, double temperature
) {
  NNResultBuf buf;

  //This gives us about 15 moves on average for 19x19.
  int numInitialMovesToPlay = (int)floor(gameRand.nextExponential() * (board.x_size * board.y_size * proportionOfBoardArea));
  assert(numInitialMovesToPlay >= 0);
  for(int i = 0; i<numInitialMovesToPlay; i++) {
    Loc loc = getGameInitializationMove(botB, botW, board, hist, pla, buf, gameRand, temperature);

    //Make the move!
    assert(hist.isLegal(board,loc,pla));
    hist.makeBoardMoveAssumeLegal(board,loc,pla);
    pla = board.nextPla;

    if(hist.isGameFinished)
      break;
  }
}



double PlayUtils::getHackedLCBForWinrate(const Search* search, const AnalysisData& data, Player pla) {
  double winrate = 0.5 * (1.0 + data.winLossValue);
  //Super hacky - in KataGo, lcb is on utility (i.e. also weighting score), not winrate, but if you're using
  //lz-analyze you probably don't know about utility and expect LCB to be about winrate. So we apply the LCB
  //radius to the winrate in order to get something reasonable to display, and also scale it proportionally
  //by how much winrate is supposed to matter relative to score.
  double radiusScaleHackFactor = search->searchParams.winLossUtilityFactor / (
    search->searchParams.winLossUtilityFactor +
    search->searchParams.staticScoreUtilityFactor +
    search->searchParams.dynamicScoreUtilityFactor +
    1.0e-20 //avoid divide by 0
  );
  //Also another factor of 0.5 because winrate goes from only 0 to 1 instead of -1 to 1 when it's part of utility
  radiusScaleHackFactor *= 0.5;
  double lcb = pla == P_WHITE ? winrate - data.radius * radiusScaleHackFactor : winrate + data.radius * radiusScaleHackFactor;
  return lcb;
}

float PlayUtils::roundAndClipKomi(double unrounded, const Board& board, bool looseClipping) {
  //Just in case, make sure komi is reasonable
  float range = looseClipping ? 40.0f + board.x_size * board.y_size : 40.0f + 0.5f * board.x_size * board.y_size;
  if(unrounded < -range)
    unrounded = -range;
  if(unrounded > range)
    unrounded = range;
  return (float)(0.5 * round(2.0 * unrounded));
}

static SearchParams getNoiselessParams(SearchParams oldParams, int64_t numVisits) {
  SearchParams newParams = oldParams;
  newParams.maxVisits = numVisits;
  newParams.maxPlayouts = numVisits;
  newParams.rootNoiseEnabled = false;
  newParams.rootPolicyTemperature = 1.0;
  newParams.rootPolicyTemperatureEarly = 1.0;
  newParams.rootFpuReductionMax = newParams.fpuReductionMax;
  newParams.rootFpuLossProp = newParams.fpuLossProp;
  newParams.rootDesiredPerChildVisitsCoeff = 0.0;
  newParams.rootNumSymmetriesToSample = 1;
  if(newParams.numThreads > (numVisits+7)/8)
    newParams.numThreads = (numVisits+7)/8;
  return newParams;
}

ReportedSearchValues PlayUtils::getWhiteScoreValues(
  Search* bot,
  const Board& board,
  const BoardHistory& hist,
  Player pla,
  int64_t numVisits,
  const OtherGameProperties& otherGameProps
) {
  assert(numVisits > 0);
  SearchParams oldParams = bot->searchParams;
  SearchParams newParams = getNoiselessParams(oldParams,numVisits);

  if(otherGameProps.playoutDoublingAdvantage != 0.0 && otherGameProps.playoutDoublingAdvantagePla != C_EMPTY) {
    //Don't actually adjust playouts, but DO tell the bot what it's up against, so that it gives estimates
    //appropriate to the asymmetric game about to be played
    newParams.playoutDoublingAdvantagePla = otherGameProps.playoutDoublingAdvantagePla;
    newParams.playoutDoublingAdvantage = otherGameProps.playoutDoublingAdvantage;
  }

  bot->setParams(newParams);
  bot->setPosition(pla,board,hist);
  bot->runWholeSearch(pla);

  ReportedSearchValues values = bot->getRootValuesRequireSuccess();
  bot->setParams(oldParams);
  return values;
}

double PlayUtils::getSearchFactor(
  double searchFactorWhenWinningThreshold,
  double searchFactorWhenWinning,
  const SearchParams& params,
  const vector<double>& recentWinLossValues,
  Player pla
) {
  double searchFactor = 1.0;
  if(recentWinLossValues.size() >= 3 && params.winLossUtilityFactor - searchFactorWhenWinningThreshold > 1e-10) {
    double recentLeastWinning = pla == P_BLACK ? -params.winLossUtilityFactor : params.winLossUtilityFactor;
    for(int i = recentWinLossValues.size()-3; i < recentWinLossValues.size(); i++) {
      if(pla == P_BLACK && recentWinLossValues[i] > recentLeastWinning)
        recentLeastWinning = recentWinLossValues[i];
      if(pla == P_WHITE && recentWinLossValues[i] < recentLeastWinning)
        recentLeastWinning = recentWinLossValues[i];
    }
    double excessWinning = pla == P_BLACK ? -searchFactorWhenWinningThreshold - recentLeastWinning : recentLeastWinning - searchFactorWhenWinningThreshold;
    if(excessWinning > 0) {
      double lambda = excessWinning / (params.winLossUtilityFactor - searchFactorWhenWinningThreshold);
      searchFactor = 1.0 + lambda * (searchFactorWhenWinning - 1.0);
    }
  }
  return searchFactor;
}

void PlayUtils::adjustKomiToEven(Search* botB, Search* botW, const Board& board, BoardHistory& hist, Player pla, int64_t numVisits, const OtherGameProperties& otherGameProps, Rand& rand)
{

}

vector<double> PlayUtils::computeOwnership(
  Search* bot,
  const Board& board,
  const BoardHistory& hist,
  Player pla,
  int64_t numVisits
) {
  assert(numVisits > 0);
  bool oldAlwaysIncludeOwnerMap = bot->alwaysIncludeOwnerMap;
  bot->setAlwaysIncludeOwnerMap(true);

  SearchParams oldParams = bot->searchParams;
  SearchParams newParams = getNoiselessParams(oldParams,numVisits);
  newParams.playoutDoublingAdvantagePla = C_EMPTY;
  newParams.playoutDoublingAdvantage = 0.0;

  bot->setParams(newParams);
  bot->setPosition(pla,board,hist);
  bot->runWholeSearch(pla);

  int64_t minVisitsForOwnership = 2;
  vector<double> ownerships = bot->getAverageTreeOwnership(minVisitsForOwnership);

  bot->setParams(oldParams);
  bot->setAlwaysIncludeOwnerMap(oldAlwaysIncludeOwnerMap);
  bot->clearSearch();

  return ownerships;
}


string PlayUtils::BenchmarkResults::toStringNotDone() const {
  ostringstream out;
  out << "numSearchThreads = " << Global::strprintf("%2d",numThreads) << ":"
      << " " << totalPositionsSearched << " / " << totalPositions << " positions,"
      << " visits/s = " << Global::strprintf("%.2f",totalVisits / totalSeconds)
      << " (" << Global::strprintf("%.1f", totalSeconds) << " secs)";
  return out.str();
}
string PlayUtils::BenchmarkResults::toString() const {
  ostringstream out;
  out << "numSearchThreads = " << Global::strprintf("%2d",numThreads) << ":"
      << " " << totalPositionsSearched << " / " << totalPositions << " positions,"
      << " visits/s = " << Global::strprintf("%.2f",totalVisits / totalSeconds)
      << " nnEvals/s = " << Global::strprintf("%.2f",numNNEvals / totalSeconds)
      << " nnBatches/s = " << Global::strprintf("%.2f",numNNBatches / totalSeconds)
      << " avgBatchSize = " << Global::strprintf("%.2f",avgBatchSize)
      << " (" << Global::strprintf("%.1f", totalSeconds) << " secs)";
  return out.str();
}
string PlayUtils::BenchmarkResults::toStringWithElo(const BenchmarkResults* baseline, double secondsPerGameMove) const {
  ostringstream out;
  out << "numSearchThreads = " << Global::strprintf("%2d",numThreads) << ":"
      << " " << totalPositionsSearched << " / " << totalPositions << " positions,"
      << " visits/s = " << Global::strprintf("%.2f",totalVisits / totalSeconds)
      << " nnEvals/s = " << Global::strprintf("%.2f",numNNEvals / totalSeconds)
      << " nnBatches/s = " << Global::strprintf("%.2f",numNNBatches / totalSeconds)
      << " avgBatchSize = " << Global::strprintf("%.2f",avgBatchSize)
      << " (" << Global::strprintf("%.1f", totalSeconds) << " secs)";

  if(baseline == NULL)
    out << " (EloDiff baseline)";
  else {
    double diff = computeEloEffect(secondsPerGameMove) - baseline->computeEloEffect(secondsPerGameMove);
    out << " (EloDiff " << Global::strprintf("%+.0f",diff) << ")";
  }
  return out.str();
}

//From some test matches by lightvector using g170
static constexpr double eloGainPerDoubling = 250;

double PlayUtils::BenchmarkResults::computeEloEffect(double secondsPerGameMove) const {
  auto computeEloCost = [&](double baseVisits) {
    //Completely ad-hoc formula that approximately fits noisy tests. Probably not very good
    //but then again the recommendation of this benchmark program is very rough anyways, it
    //doesn't need to be all that great.
    return numThreads * 7.0 * pow(1600.0 / (800.0 + baseVisits),0.85);
  };

  double visitsPerSecond = totalVisits / totalSeconds;
  double gain = eloGainPerDoubling * log(visitsPerSecond) / log(2);
  double visitsPerMove = visitsPerSecond * secondsPerGameMove;
  double cost = computeEloCost(visitsPerMove);
  return gain - cost;
}

void PlayUtils::BenchmarkResults::printEloComparison(const vector<BenchmarkResults>& results, double secondsPerGameMove) {
  int bestIdx = 0;
  for(int i = 1; i<results.size(); i++) {
    if(results[i].computeEloEffect(secondsPerGameMove) > results[bestIdx].computeEloEffect(secondsPerGameMove))
      bestIdx = i;
  }

  cout << endl;
  cout << "Based on some test data, each speed doubling gains perhaps ~" << eloGainPerDoubling << " Elo by searching deeper." << endl;
  cout << "Based on some test data, each thread costs perhaps 7 Elo if using 800 visits, and 2 Elo if using 5000 visits (by making MCTS worse)." << endl;
  cout << "So APPROXIMATELY based on this benchmark, if you intend to do a " << secondsPerGameMove << " second search: " << endl;
  for(int i = 0; i<results.size(); i++) {
    int numThreads = results[i].numThreads;
    double eloEffect = results[i].computeEloEffect(secondsPerGameMove) - results[0].computeEloEffect(secondsPerGameMove);
    cout << "numSearchThreads = " << Global::strprintf("%2d",numThreads) << ": ";
    if(i == 0)
      cout << "(baseline)" << (i == bestIdx ? " (recommended)" : "") << endl;
    else
      cout << Global::strprintf("%+5.0f",eloEffect) << " Elo" << (i == bestIdx ? " (recommended)" : "") << endl;
  }
  cout << endl;
}


PlayUtils::BenchmarkResults PlayUtils::benchmarkSearchOnPositionsAndPrint(
  const SearchParams& params,
  const CompactSgf* sgf,
  int numPositionsToUse,
  NNEvaluator* nnEval,
  const BenchmarkResults* baseline,
  double secondsPerGameMove,
  bool printElo
) {
  //Pick random positions from the SGF file, but deterministically
  vector<Move> moves = sgf->moves;
  string posSeed = "benchmarkPosSeed|";
  for(int i = 0; i<moves.size(); i++) {
    posSeed += Global::intToString((int)moves[i].loc);
    posSeed += "|";
  }

  vector<int> possiblePositionIdxs;
  {
    Rand posRand(posSeed);
    for(int i = 0; i<moves.size(); i++) {
      possiblePositionIdxs.push_back(i);
    }
    if(possiblePositionIdxs.size() > 0) {
      for(int i = possiblePositionIdxs.size()-1; i > 1; i--) {
        int r = posRand.nextUInt(i);
        int tmp = possiblePositionIdxs[i];
        possiblePositionIdxs[i] = possiblePositionIdxs[r];
        possiblePositionIdxs[r] = tmp;
      }
    }
    if(possiblePositionIdxs.size() > numPositionsToUse)
      possiblePositionIdxs.resize(numPositionsToUse);
  }

  std::sort(possiblePositionIdxs.begin(),possiblePositionIdxs.end());

  BenchmarkResults results;
  results.numThreads = params.numThreads;
  results.totalPositions = possiblePositionIdxs.size();

  nnEval->clearCache();
  nnEval->clearStats();

  Rand seedRand;
  Search* bot = new Search(params,nnEval,nnEval->getLogger(),Global::uint64ToString(seedRand.nextUInt64()));

  //Ignore the SGF rules, except for komi. Just use Tromp-taylor.
  Rules initialRules = Rules::getTrompTaylorish();
  //Take the komi from the sgf, otherwise ignore the rules in the sgf
  initialRules.komi = sgf->komi;

  Board board;
  Player nextPla;
  BoardHistory hist;
  sgf->setupInitialBoardAndHist(initialRules, board, nextPla, hist);

  int moveNum = 0;

  for(int i = 0; i<possiblePositionIdxs.size(); i++) {
    cout << "\r" << results.toStringNotDone() << "      " << std::flush;

    int nextIdx = possiblePositionIdxs[i];
    while(moveNum < moves.size() && moveNum < nextIdx) {
      bool suc = hist.makeBoardMoveTolerant(board,moves[moveNum].loc,moves[moveNum].pla);
      if(!suc) {
        cerr << endl;
        cerr << board << endl;
        cerr << "SGF Illegal move " << (moveNum+1) << " for " << PlayerIO::colorToChar(moves[moveNum].pla) << ": " << Location::toString(moves[moveNum].loc,board) << endl;
        throw StringError("Illegal move in SGF");
      }
      nextPla = board.nextPla;
      moveNum += 1;
    }

    bot->clearSearch();
    bot->setPosition(nextPla,board,hist);
    nnEval->clearCache();

    ClockTimer timer;
    bot->runWholeSearch(nextPla);
    double seconds = timer.getSeconds();

    results.totalPositionsSearched += 1;
    results.totalSeconds += seconds;
    results.totalVisits += bot->getRootVisits();
  }

  results.numNNEvals = nnEval->numRowsProcessed();
  results.numNNBatches = nnEval->numBatchesProcessed();
  results.avgBatchSize = nnEval->averageProcessedBatchSize();

  if(printElo)
    cout << "\r" << results.toStringWithElo(baseline,secondsPerGameMove) << std::endl;
  else
    cout << "\r" << results.toString() << std::endl;

  delete bot;

  return results;
}


void PlayUtils::printGenmoveLog(ostream& out, const AsyncBot* bot, const NNEvaluator* nnEval, Loc moveLoc, double timeTaken, Player perspective) {
  const Search* search = bot->getSearch();
  Board::printBoard(out, bot->getRootBoard(), moveLoc, &(bot->getRootHist().moveHistory));
  out << bot->getRootHist().rules << "\n";
  if(!std::isnan(timeTaken))
    out << "Time taken: " << timeTaken << "\n";
  out << "Root visits: " << search->getRootVisits() << "\n";
  out << "New playouts: " << search->lastSearchNumPlayouts << "\n";
  out << "NN rows: " << nnEval->numRowsProcessed() << endl;
  out << "NN batches: " << nnEval->numBatchesProcessed() << endl;
  out << "NN avg batch size: " << nnEval->averageProcessedBatchSize() << endl;
  if(search->searchParams.playoutDoublingAdvantage != 0)
    out << "PlayoutDoublingAdvantage: " << (
      search->getRootPla() == getOpp(search->getPlayoutDoublingAdvantagePla()) ?
      -search->searchParams.playoutDoublingAdvantage : search->searchParams.playoutDoublingAdvantage) << endl;
  out << "PV: ";
  search->printPV(out, search->rootNode, 25);
  out << "\n";
  out << "Tree:\n";
  search->printTree(out, search->rootNode, PrintTreeOptions().maxDepth(1).maxChildrenToShow(10),perspective);
}

Rules PlayUtils::genRandomRules(Rand& rand) {
  vector<int> allowedTaxRules = { Rules::TAX_NONE, Rules::TAX_SEKI, Rules::TAX_ALL };

  Rules rules;
  rules.taxRule = allowedTaxRules[rand.nextUInt(allowedTaxRules.size())];
  return rules;
}



