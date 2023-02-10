
//-------------------------------------------------------------------------------------
//This file contains various functions for extracting stats and results from the search, choosing a move, etc
//-------------------------------------------------------------------------------------

#include "../search/search.h"

#include <cinttypes>

#include "../program/playutils.h"
#include "../search/searchnode.h"

using namespace std;
using nlohmann::json;

int64_t Search::getRootVisits() const {
  if(rootNode == NULL)
    return 0;
  int64_t n = rootNode->stats.visits.load(std::memory_order_acquire);
  return n;
}

bool Search::getPlaySelectionValues(
  vector<Loc>& locs, vector<double>& playSelectionValues, double scaleMaxToAtLeast
) const {
  if(rootNode == NULL) {
    locs.clear();
    playSelectionValues.clear();
    return false;
  }
  bool allowDirectPolicyMoves = true;
  return getPlaySelectionValues(*rootNode, locs, playSelectionValues, NULL, scaleMaxToAtLeast, allowDirectPolicyMoves);
}

bool Search::getPlaySelectionValues(
  vector<Loc>& locs, vector<double>& playSelectionValues, vector<double>* retVisitCounts, double scaleMaxToAtLeast
) const {
  if(rootNode == NULL) {
    locs.clear();
    playSelectionValues.clear();
    if(retVisitCounts != NULL)
      retVisitCounts->clear();
    return false;
  }
  bool allowDirectPolicyMoves = true;
  return getPlaySelectionValues(*rootNode, locs, playSelectionValues, retVisitCounts, scaleMaxToAtLeast, allowDirectPolicyMoves);
}

bool Search::getPlaySelectionValues(
  const SearchNode& node,
  vector<Loc>& locs, vector<double>& playSelectionValues, vector<double>* retVisitCounts, double scaleMaxToAtLeast,
  bool allowDirectPolicyMoves
) const {
  double lcbBuf[NNPos::MAX_NN_POLICY_SIZE];
  double radiusBuf[NNPos::MAX_NN_POLICY_SIZE];
  bool result = getPlaySelectionValues(
    node,locs,playSelectionValues,retVisitCounts,scaleMaxToAtLeast,allowDirectPolicyMoves,
    false,false,lcbBuf,radiusBuf
  );
  return result;
}

bool Search::getPlaySelectionValues(
  const SearchNode& node,
  vector<Loc>& locs, vector<double>& playSelectionValues, vector<double>* retVisitCounts, double scaleMaxToAtLeast,
  bool allowDirectPolicyMoves, bool alwaysComputeLcb, bool neverUseLcb,
  //Note: lcbBuf is signed from the player to move's perspective
  double lcbBuf[NNPos::MAX_NN_POLICY_SIZE], double radiusBuf[NNPos::MAX_NN_POLICY_SIZE]
) const {
  locs.clear();
  playSelectionValues.clear();
  if(retVisitCounts != NULL)
    retVisitCounts->clear();

  double totalChildWeight = 0.0;
  double maxChildWeight = 0.0;

  //Store up basic weights
  int childrenCapacity;
  const SearchChildPointer* children = node.getChildren(childrenCapacity);
  for(int i = 0; i<childrenCapacity; i++) {
    const SearchNode* child = children[i].getIfAllocated();
    if(child == NULL)
      break;
    Loc moveLoc = children[i].getMoveLocRelaxed();

    int64_t edgeVisits = children[i].getEdgeVisits();
    double childWeight = child->stats.getChildWeight(edgeVisits);

    locs.push_back(moveLoc);
    totalChildWeight += childWeight;
    if(childWeight > maxChildWeight)
      maxChildWeight = childWeight;
    
    playSelectionValues.push_back((double)childWeight);
    if(retVisitCounts != NULL)
      (*retVisitCounts).push_back((double)edgeVisits);
    
  }

  int numChildren = (int)playSelectionValues.size();

  //Find the best child by weight
  int mostWeightedIdx = 0;
  double mostWeightedChildWeight = -1e30;
  for(int i = 0; i<numChildren; i++) {
    double value = playSelectionValues[i];
    if(value > mostWeightedChildWeight) {
      mostWeightedChildWeight = value;
      mostWeightedIdx = i;
    }
  }

  //Possibly reduce weight on children that we spend too many visits on in retrospect
  if(&node == rootNode && numChildren > 0) {

    const SearchNode* bestChild = children[mostWeightedIdx].getIfAllocated();
    int64_t bestChildEdgeVisits = children[mostWeightedIdx].getEdgeVisits();
    Loc bestMoveLoc = children[mostWeightedIdx].getMoveLocRelaxed();
    assert(bestChild != NULL);
    const bool isRoot = true;
    const double policyProbMassVisited = 1.0; //doesn't matter, since fpu value computed from it isn't used here
    double parentUtility;
    double parentWeightPerVisit;
    double parentUtilityStdevFactor;
    double fpuValue = getFpuValueForChildrenAssumeVisited(
      node, rootPla, isRoot, policyProbMassVisited,
      parentUtility, parentWeightPerVisit, parentUtilityStdevFactor
    );

    bool isDuringSearch = false;

    double exploreScaling = getExploreScaling(totalChildWeight, parentUtilityStdevFactor);

    const NNOutput* nnOutput = node.getNNOutput();
    assert(nnOutput != NULL);
    const float* policyProbs = nnOutput->getPolicyProbsMaybeNoised();
    double bestChildExploreSelectionValue = getExploreSelectionValueOfChild(
      node,policyProbs,bestChild,
      bestMoveLoc,
      exploreScaling,
      totalChildWeight,bestChildEdgeVisits,fpuValue,
      parentUtility,parentWeightPerVisit,
      isDuringSearch,maxChildWeight,NULL
    );

    for(int i = 0; i<numChildren; i++) {
      const SearchNode* child = children[i].getIfAllocated();
      Loc moveLoc = children[i].getMoveLocRelaxed();
      if(i != mostWeightedIdx) {
        int64_t edgeVisits = children[i].getEdgeVisits();
        double reduced = getReducedPlaySelectionWeight(
          node, policyProbs, child,
          moveLoc,
          exploreScaling,
          edgeVisits,
          bestChildExploreSelectionValue
        );
        playSelectionValues[i] = ceil(reduced);
      }
    }
  }

  //Now compute play selection values taking into account LCB
  if(!neverUseLcb && (alwaysComputeLcb || (searchParams.useLcbForSelection && numChildren > 0))) {
    double bestLcb = -1e10;
    int bestLcbIndex = -1;
    for(int i = 0; i<numChildren; i++) {
      const SearchNode* child = children[i].getIfAllocated();
      int64_t edgeVisits = children[i].getEdgeVisits();
      Loc moveLoc = children[i].getMoveLocRelaxed();
      getSelfUtilityLCBAndRadius(node,child,edgeVisits,moveLoc,lcbBuf[i],radiusBuf[i]);
      //Check if this node is eligible to be considered for best LCB
      double weight = playSelectionValues[i];
      if(weight > 0 && weight >= searchParams.minVisitPropForLCB * mostWeightedChildWeight) {
        if(lcbBuf[i] > bestLcb) {
          bestLcb = lcbBuf[i];
          bestLcbIndex = i;
        }
      }
    }

    if(searchParams.useLcbForSelection && numChildren > 0 && (searchParams.useNonBuggyLcb ? (bestLcbIndex >= 0) : (bestLcbIndex > 0))) {
      //Best LCB move gets a bonus that ensures it is large enough relative to every other child
      double adjustedWeight = playSelectionValues[bestLcbIndex];
      for(int i = 0; i<numChildren; i++) {
        if(i != bestLcbIndex) {
          double excessValue = bestLcb - lcbBuf[i];
          //This move is actually worse lcb than some other move, it's just that the other
          //move failed its checks for having enough minimum weight. So don't actually
          //try to compute how much better this one is than that one, because it's not better.
          if(excessValue < 0)
            continue;

          double radius = radiusBuf[i];
          //How many times wider would the radius have to be before the lcb would be worse?
          //Add adjust the denom so that we cannot possibly gain more than a factor of 5, just as a guard
          double radiusFactor = (radius + excessValue) / (radius + 0.20 * excessValue);

          //That factor, squared, is the number of "weight" more that we should pretend we have, for
          //the purpose of selection, since normally stdev is proportional to 1/weight^2.
          double lbound = radiusFactor * radiusFactor * playSelectionValues[i];
          if(lbound > adjustedWeight)
            adjustedWeight = lbound;
        }
      }
      playSelectionValues[bestLcbIndex] = adjustedWeight;
    }
  }

  const NNOutput* nnOutput = node.getNNOutput();

  //If we have no children, then use the policy net directly. Only for the root, though, if calling this on any subtree
  //then just require that we have children, for implementation simplicity (since it requires that we have a board and a boardhistory too)
  //(and we also use isAllowedRootMove and avoidMoveUntilByLoc)
  if(numChildren == 0) {
    if(nnOutput == NULL || &node != rootNode || !allowDirectPolicyMoves)
      return false;

    bool obeyAllowedRootMove = true;
    while(true) {
      for(int movePos = 0; movePos<policySize; movePos++) {
        Loc moveLoc = NNPos::posToLoc(movePos,rootBoard.x_size,rootBoard.y_size,nnXLen,nnYLen);
        const float* policyProbs = nnOutput->getPolicyProbsMaybeNoised();
        double policyProb = policyProbs[movePos];
        if(!rootHistory.isLegal(rootBoard,moveLoc,rootPla) || policyProb < 0 || (obeyAllowedRootMove && !isAllowedRootMove(moveLoc)))
          continue;
        const std::vector<int>& avoidMoveUntilByLoc = rootPla == P_BLACK ? avoidMoveUntilByLocBlack : avoidMoveUntilByLocWhite;
        if(avoidMoveUntilByLoc.size() > 0) {
          assert(avoidMoveUntilByLoc.size() >= Board::MAX_ARR_SIZE);
          int untilDepth = avoidMoveUntilByLoc[moveLoc];
          if(untilDepth > 0)
            continue;
        }
        locs.push_back(moveLoc);
        playSelectionValues.push_back(policyProb);
        numChildren++;
      }
      //Still no children? Then at this point just ignore isAllowedRootMove.
      if(numChildren == 0 && obeyAllowedRootMove) {
        obeyAllowedRootMove = false;
        continue;
      }
      break;
    }
  }

  //Might happen absurdly rarely if we both have no children and don't properly have an nnOutput
  //but have a hash collision or something so we "found" an nnOutput anyways.
  //Could also happen if we have avoidMoveUntilByLoc pruning all the allowed moves.
  if(numChildren == 0)
    return false;

  double maxValue = 0.0;
  for(int i = 0; i<numChildren; i++) {
    if(playSelectionValues[i] > maxValue)
      maxValue = playSelectionValues[i];
  }

  if(maxValue <= 1e-50)
    return false;

  //Sanity check - if somehow we had more than this, something must have overflowed or gone wrong
  assert(maxValue < 1e40);

  double amountToSubtract = std::min(searchParams.chosenMoveSubtract, maxValue/64.0);
  double amountToPrune = std::min(searchParams.chosenMovePrune, maxValue/64.0);
  double newMaxValue = maxValue - amountToSubtract;
  for(int i = 0; i<numChildren; i++) {
    if(playSelectionValues[i] < amountToPrune)
      playSelectionValues[i] = 0.0;
    else {
      playSelectionValues[i] -= amountToSubtract;
      if(playSelectionValues[i] <= 0.0)
        playSelectionValues[i] = 0.0;
    }
  }

  assert(newMaxValue > 0.0);

  if(newMaxValue < scaleMaxToAtLeast) {
    for(int i = 0; i<numChildren; i++) {
      playSelectionValues[i] *= scaleMaxToAtLeast / newMaxValue;
    }
  }

  return true;
}


bool Search::getRootValues(ReportedSearchValues& values) const {
  return getNodeValues(rootNode,values);
}

ReportedSearchValues Search::getRootValuesRequireSuccess() const {
  ReportedSearchValues values;
  if(rootNode == NULL)
    throw StringError("Bug? Bot search root was null");
  bool success = getNodeValues(rootNode,values);
  if(!success)
    throw StringError("Bug? Bot search returned no root values");
  return values;
}

bool Search::getRootRawNNValues(ReportedSearchValues& values) const {
  if(rootNode == NULL)
    return false;
  return getNodeRawNNValues(*rootNode,values);
}

ReportedSearchValues Search::getRootRawNNValuesRequireSuccess() const {
  ReportedSearchValues values;
  if(rootNode == NULL)
    throw StringError("Bug? Bot search root was null");
  bool success = getNodeRawNNValues(*rootNode,values);
  if(!success)
    throw StringError("Bug? Bot search returned no root values");
  return values;
}

bool Search::getNodeRawNNValues(const SearchNode& node, ReportedSearchValues& values) const {
  const NNOutput* nnOutput = node.getNNOutput();
  if(nnOutput == NULL)
    return false;

  values.winValue = nnOutput->whiteWinProb;
  values.lossValue = nnOutput->whiteLossProb;
  values.noResultValue = nnOutput->whiteNoResultProb;


  //Sanity check
  assert(values.winValue >= 0.0);
  assert(values.lossValue >= 0.0);
  assert(values.noResultValue >= 0.0);
  assert(values.winValue + values.lossValue + values.noResultValue < 1.001);

  double winLossValue = values.winValue - values.lossValue;
  if(winLossValue > 1.0) winLossValue = 1.0;
  if(winLossValue < -1.0) winLossValue = -1.0;
  values.winLossValue = winLossValue;

  values.weight = computeWeightFromNNOutput(nnOutput);
  values.visits = 1;

  return true;
}


bool Search::getNodeValues(const SearchNode* node, ReportedSearchValues& values) const {
  if(node == NULL)
    return false;
  int64_t visits = node->stats.visits.load(std::memory_order_acquire);
  double weightSum = node->stats.weightSum.load(std::memory_order_acquire);
  double winLossValueAvg = node->stats.winLossValueAvg.load(std::memory_order_acquire);
  double noResultValueAvg = node->stats.noResultValueAvg.load(std::memory_order_acquire);
  double utilityAvg = node->stats.utilityAvg.load(std::memory_order_acquire);

  if(weightSum <= 0.0)
    return false;
  assert(visits >= 0);
  if(node == rootNode) {
    //For terminal nodes, we may have no nnoutput and yet we have legitimate visits and terminal evals.
    //But for the root, the root is never treated as a terminal node and always gets an nneval, so if
    //it has visits and weight, it has an nnoutput unless something has gone wrong.
    const NNOutput* nnOutput = node->getNNOutput();
    assert(nnOutput != NULL);
    (void)nnOutput;
  }

  values = ReportedSearchValues(
    *this,
    winLossValueAvg,
    noResultValueAvg,
    utilityAvg,
    weightSum,
    visits
  );
  return true;
}

const SearchNode* Search::getRootNode() const {
  return rootNode;
}
const SearchNode* Search::getChildForMove(const SearchNode* node, Loc moveLoc) const {
  if(node == NULL)
    return NULL;
  int childrenCapacity;
  const SearchChildPointer* children = node->getChildren(childrenCapacity);
  for(int i = 0; i<childrenCapacity; i++) {
    const SearchNode* child = children[i].getIfAllocated();
    if(child == NULL)
      break;
    Loc childMoveLoc = children[i].getMoveLocRelaxed();
    if(moveLoc == childMoveLoc)
      return child;
  }
  return NULL;
}

Loc Search::getChosenMoveLoc() {
  if(rootNode == NULL)
    return Board::NULL_LOC;

  vector<Loc> locs;
  vector<double> playSelectionValues;
  bool suc = getPlaySelectionValues(locs,playSelectionValues,0.0);
  if(!suc)
    return Board::NULL_LOC;

  assert(locs.size() == playSelectionValues.size());

  double temperature = interpolateEarly(
    searchParams.chosenMoveTemperatureHalflife, searchParams.chosenMoveTemperatureEarly, searchParams.chosenMoveTemperature
  );

  uint32_t idxChosen = chooseIndexWithTemperature(nonSearchRand, playSelectionValues.data(), (int)playSelectionValues.size(), temperature);
  return locs[idxChosen];
}


bool Search::getPolicy(float policyProbs[NNPos::MAX_NN_POLICY_SIZE]) const {
  return getPolicy(rootNode, policyProbs);
}
bool Search::getPolicy(const SearchNode* node, float policyProbs[NNPos::MAX_NN_POLICY_SIZE]) const {
  if(node == NULL)
    return false;
  const NNOutput* nnOutput = node->getNNOutput();
  if(nnOutput == NULL)
    return false;

  std::copy(nnOutput->policyProbs, nnOutput->policyProbs+NNPos::MAX_NN_POLICY_SIZE, policyProbs);
  return true;
}


//Safe to call concurrently with search
double Search::getPolicySurprise() const {
  double surprise = 0.0;
  double searchEntropy = 0.0;
  double policyEntropy = 0.0;
  if(getPolicySurpriseAndEntropy(surprise,searchEntropy,policyEntropy))
    return surprise;
  return 0.0;
}

bool Search::getPolicySurpriseAndEntropy(double& surpriseRet, double& searchEntropyRet, double& policyEntropyRet) const {
  return getPolicySurpriseAndEntropy(surpriseRet, searchEntropyRet, policyEntropyRet, rootNode);
}

//Safe to call concurrently with search
bool Search::getPolicySurpriseAndEntropy(double& surpriseRet, double& searchEntropyRet, double& policyEntropyRet, const SearchNode* node) const {
  if(node == NULL)
    return false;
  const NNOutput* nnOutput = node->getNNOutput();
  if(nnOutput == NULL)
    return false;

  vector<Loc> locs;
  vector<double> playSelectionValues;
  bool allowDirectPolicyMoves = true;
  bool alwaysComputeLcb = false;
  double lcbBuf[NNPos::MAX_NN_POLICY_SIZE];
  double radiusBuf[NNPos::MAX_NN_POLICY_SIZE];
  bool suc = getPlaySelectionValues(
    *node,locs,playSelectionValues,NULL,1.0,allowDirectPolicyMoves,alwaysComputeLcb,false,lcbBuf,radiusBuf
  );
  if(!suc)
    return false;

  float policyProbsFromNNBuf[NNPos::MAX_NN_POLICY_SIZE];
  {
    const float* policyProbsFromNN = nnOutput->getPolicyProbsMaybeNoised();
    std::copy(policyProbsFromNN, policyProbsFromNN+NNPos::MAX_NN_POLICY_SIZE, policyProbsFromNNBuf);
  }

  double sumPlaySelectionValues = 0.0;
  for(size_t i = 0; i < playSelectionValues.size(); i++)
    sumPlaySelectionValues += playSelectionValues[i];

  double surprise = 0.0;
  double searchEntropy = 0.0;
  for(size_t i = 0; i < playSelectionValues.size(); i++) {
    int pos = getPos(locs[i]);
    double policy = std::max((double)policyProbsFromNNBuf[pos],1e-100);
    double target = playSelectionValues[i] / sumPlaySelectionValues;
    if(target > 1e-100) {
      double logTarget = log(target);
      double logPolicy = log(policy);
      surprise += target * (logTarget - logPolicy);
      searchEntropy += -target * logTarget;
    }
  }

  double policyEntropy = 0.0;
  for(int pos = 0; pos<NNPos::MAX_NN_POLICY_SIZE; pos++) {
    double policy = policyProbsFromNNBuf[pos];
    if(policy > 1e-100) {
      policyEntropy += -policy * log(policy);
    }
  }

  //Just in case, guard against float imprecision
  if(surprise < 0.0)
    surprise = 0.0;
  if(searchEntropy < 0.0)
    searchEntropy = 0.0;
  if(policyEntropy < 0.0)
    policyEntropy = 0.0;

  surpriseRet = surprise;
  searchEntropyRet = searchEntropy;
  policyEntropyRet = policyEntropy;

  return true;
}

void Search::printRootPolicyMap(ostream& out) const {
  if(rootNode == NULL)
    return;
  const NNOutput* nnOutput = rootNode->getNNOutput();
  if(nnOutput == NULL)
    return;

  const float* policyProbs = nnOutput->getPolicyProbsMaybeNoised();
  for(int y = 0; y<rootBoard.y_size; y++) {
    for(int x = 0; x<rootBoard.x_size; x++) {
      int pos = NNPos::xyToPos(x,y,nnOutput->nnXLen);
      out << Global::strprintf("%6.1f ", policyProbs[pos]*100);
    }
    out << endl;
  }
  out << endl;
}

void Search::appendPV(
  vector<Loc>& buf,
  vector<int64_t>& visitsBuf,
  vector<int64_t>& edgeVisitsBuf,
  vector<Loc>& scratchLocs,
  vector<double>& scratchValues,
  const SearchNode* node,
  int maxDepth
) const {
  appendPVForMove(buf,visitsBuf,edgeVisitsBuf,scratchLocs,scratchValues,node,Board::NULL_LOC,maxDepth);
}

void Search::appendPVForMove(
  vector<Loc>& buf,
  vector<int64_t>& visitsBuf,
  vector<int64_t>& edgeVisitsBuf,
  vector<Loc>& scratchLocs,
  vector<double>& scratchValues,
  const SearchNode* node,
  Loc move,
  int maxDepth
) const {
  if(node == NULL)
    return;

  for(int depth = 0; depth < maxDepth; depth++) {
    bool success = getPlaySelectionValues(*node, scratchLocs, scratchValues, NULL, 1.0, false);
    if(!success)
      return;

    double maxSelectionValue = POLICY_ILLEGAL_SELECTION_VALUE;
    int bestChildIdx = -1;
    Loc bestChildMoveLoc = Board::NULL_LOC;

    for(int i = 0; i<scratchValues.size(); i++) {
      Loc moveLoc = scratchLocs[i];
      double selectionValue = scratchValues[i];

      if(depth == 0 && moveLoc == move) {
        maxSelectionValue = selectionValue;
        bestChildIdx = i;
        bestChildMoveLoc = moveLoc;
        break;
      }

      if(selectionValue > maxSelectionValue) {
        maxSelectionValue = selectionValue;
        bestChildIdx = i;
        bestChildMoveLoc = moveLoc;
      }
    }

    if(bestChildIdx < 0 || bestChildMoveLoc == Board::NULL_LOC)
      return;
    if(depth == 0 && move != Board::NULL_LOC && bestChildMoveLoc != move)
      return;

    int childrenCapacity;
    const SearchChildPointer* children = node->getChildren(childrenCapacity);
    assert(bestChildIdx <= childrenCapacity);
    assert(scratchValues.size() <= childrenCapacity);

    const SearchNode* child = children[bestChildIdx].getIfAllocated();
    assert(child != NULL);
    node = child;

    int64_t visits = node->stats.visits.load(std::memory_order_acquire);
    int64_t edgeVisits = children[bestChildIdx].getEdgeVisits();

    buf.push_back(bestChildMoveLoc);
    visitsBuf.push_back(visits);
    edgeVisitsBuf.push_back(edgeVisits);
  }
}


void Search::printPV(ostream& out, const SearchNode* n, int maxDepth) const {
  vector<Loc> buf;
  vector<int64_t> visitsBuf;
  vector<int64_t> edgeVisitsBuf;
  vector<Loc> scratchLocs;
  vector<double> scratchValues;
  appendPV(buf,visitsBuf,edgeVisitsBuf,scratchLocs,scratchValues,n,maxDepth);
  printPV(out,buf);
}

void Search::printPV(ostream& out, const vector<Loc>& buf) const {
  bool printedAnything = false;
  for(int i = 0; i<buf.size(); i++) {
    if(printedAnything)
      out << " ";
    if(buf[i] == Board::NULL_LOC)
      continue;
    out << Location::toString(buf[i],rootBoard);
    printedAnything = true;
  }
}

//Child should NOT be locked.
AnalysisData Search::getAnalysisDataOfSingleChild(
  const SearchNode* child, int64_t edgeVisits, vector<Loc>& scratchLocs, vector<double>& scratchValues,
  Loc move, double policyProb, double fpuValue, double parentUtility, double parentWinLossValue,
  int maxPVDepth
) const {
  int64_t childVisits = 0;
  double winLossValueAvg = 0.0;
  double noResultValueAvg = 0.0;
  double utilityAvg = 0.0;
  double utilitySqAvg = 0.0;
  double weightSum = 0.0;
  double weightSqSum = 0.0;

  if(child != NULL) {
    childVisits = child->stats.visits.load(std::memory_order_acquire);
    winLossValueAvg = child->stats.winLossValueAvg.load(std::memory_order_acquire);
    noResultValueAvg = child->stats.noResultValueAvg.load(std::memory_order_acquire);
    utilityAvg = child->stats.utilityAvg.load(std::memory_order_acquire);
    utilitySqAvg = child->stats.utilitySqAvg.load(std::memory_order_acquire);
    weightSum = child->stats.getChildWeight(edgeVisits,childVisits);
    weightSqSum = child->stats.getChildWeightSq(edgeVisits,childVisits);
  }

  AnalysisData data;
  data.move = move;
  data.numVisits = edgeVisits;
  if(childVisits <= 0 || weightSum <= 1e-30 || weightSqSum <= 1e-60) {
    data.utility = fpuValue;
    data.resultUtility = fpuValue;
    data.winLossValue = searchParams.winLossUtilityFactor == 1.0 ? parentWinLossValue + (fpuValue - parentUtility) : 0.0;
    data.noResultValue = 0.0;
    // Make sure winloss values due to FPU don't go out of bounds for purposes of reporting to UI
    if(data.winLossValue < -1.0)
      data.winLossValue = -1.0;
    if(data.winLossValue > 1.0)
      data.winLossValue = 1.0;
    data.ess = 0.0;
    data.weightSum = 0.0;
    data.weightSqSum = 0.0;
    data.utilitySqAvg = data.utility * data.utility;
  }
  else {
    data.utility = utilityAvg;
    data.resultUtility = getResultUtility(winLossValueAvg, noResultValueAvg);
    data.winLossValue = winLossValueAvg;
    data.noResultValue = noResultValueAvg;
    data.ess = weightSum * weightSum / weightSqSum;
    data.weightSum = weightSum;
    data.weightSqSum = weightSqSum;
    data.utilitySqAvg = utilitySqAvg; 
  }

  data.policyPrior = policyProb;
  data.order = 0;

  data.pv.clear();
  data.pv.push_back(move);
  data.pvVisits.clear();
  data.pvVisits.push_back(childVisits);
  data.pvEdgeVisits.clear();
  data.pvEdgeVisits.push_back(edgeVisits);
  appendPV(data.pv, data.pvVisits, data.pvEdgeVisits, scratchLocs, scratchValues, child, maxPVDepth);

  data.node = child;

  return data;
}

void Search::getAnalysisData(
  vector<AnalysisData>& buf,int minMovesToTryToGet, bool includeWeightFactors, int maxPVDepth, bool duplicateForSymmetries
) const {
  buf.clear();
  if(rootNode == NULL)
    return;
  getAnalysisData(*rootNode, buf, minMovesToTryToGet, includeWeightFactors, maxPVDepth, duplicateForSymmetries);
}

void Search::getAnalysisData(
  const SearchNode& node, vector<AnalysisData>& buf, int minMovesToTryToGet, bool includeWeightFactors, int maxPVDepth, bool duplicateForSymmetries
) const {
  buf.clear();
  vector<const SearchNode*> children;
  vector<int64_t> childrenEdgeVisits;
  vector<Loc> childrenMoveLocs;
  children.reserve(rootBoard.x_size * rootBoard.y_size + 1);
  childrenEdgeVisits.reserve(rootBoard.x_size * rootBoard.y_size + 1);
  childrenMoveLocs.reserve(rootBoard.x_size * rootBoard.y_size + 1);

  int numChildren;
  vector<Loc> scratchLocs;
  vector<double> scratchValues;
  double lcbBuf[NNPos::MAX_NN_POLICY_SIZE];
  double radiusBuf[NNPos::MAX_NN_POLICY_SIZE];
  float policyProbs[NNPos::MAX_NN_POLICY_SIZE];
  {
    int childrenCapacity;
    const SearchChildPointer* childrenArr = node.getChildren(childrenCapacity);
    for(int i = 0; i<childrenCapacity; i++) {
      const SearchNode* child = childrenArr[i].getIfAllocated();
      if(child == NULL)
        break;
      children.push_back(child);
      childrenEdgeVisits.push_back(childrenArr[i].getEdgeVisits());
      childrenMoveLocs.push_back(childrenArr[i].getMoveLocRelaxed());
    }
    numChildren = (int)children.size();

    if(numChildren <= 0)
      return;
    assert(numChildren <= NNPos::MAX_NN_POLICY_SIZE);

    bool alwaysComputeLcb = true;
    bool success = getPlaySelectionValues(node, scratchLocs, scratchValues, NULL, 1.0, false, alwaysComputeLcb, false, lcbBuf, radiusBuf);
    if(!success)
      return;

    const NNOutput* nnOutput = node.getNNOutput();
    const float* policyProbsFromNN = nnOutput->getPolicyProbsMaybeNoised();
    for(int i = 0; i<NNPos::MAX_NN_POLICY_SIZE; i++)
      policyProbs[i] = policyProbsFromNN[i];
  }

  //Copy to make sure we keep these values so we can reuse scratch later for PV
  vector<double> playSelectionValues = scratchValues;

  double policyProbMassVisited = 0.0;
  {
    for(int i = 0; i<numChildren; i++) {
      policyProbMassVisited += std::max(0.0, (double)policyProbs[getPos(childrenMoveLocs[i])]);
    }
    //Probability mass should not sum to more than 1, giving a generous allowance
    //for floating point error.
    assert(policyProbMassVisited <= 1.0001);
  }

  double parentWinLossValue;
  {
    double weightSum = node.stats.weightSum.load(std::memory_order_acquire);
    double winLossValueAvg = node.stats.winLossValueAvg.load(std::memory_order_acquire);
    assert(weightSum > 0.0);

    parentWinLossValue = winLossValueAvg;
  }

  double parentUtility;
  double parentWeightPerVisit;
  double parentUtilityStdevFactor;
  double fpuValue = getFpuValueForChildrenAssumeVisited(
    node, node.nextPla, true, policyProbMassVisited,
    parentUtility, parentWeightPerVisit, parentUtilityStdevFactor
  );

  vector<MoreNodeStats> statsBuf(numChildren);
  for(int i = 0; i<numChildren; i++) {
    const SearchNode* child = children[i];
    int64_t edgeVisits = childrenEdgeVisits[i];
    Loc moveLoc = childrenMoveLocs[i];
    double policyProb = policyProbs[getPos(moveLoc)];
    AnalysisData data = getAnalysisDataOfSingleChild(
      child, edgeVisits, scratchLocs, scratchValues, moveLoc, policyProb, fpuValue, parentUtility, parentWinLossValue,
      maxPVDepth
    );
    data.playSelectionValue = playSelectionValues[i];
    //Make sure data.lcb is from white's perspective, for consistency with everything else
    //In lcbBuf, it's from self perspective, unlike values at nodes.
    data.lcb = node.nextPla == P_BLACK ? -lcbBuf[i] : lcbBuf[i];
    data.radius = radiusBuf[i];
    buf.push_back(data);

    if(includeWeightFactors) {
      MoreNodeStats& stats = statsBuf[i];
      stats.stats = NodeStats(child->stats);
      stats.selfUtility = node.nextPla == P_WHITE ? data.utility : -data.utility;
      stats.weightAdjusted = stats.stats.getChildWeight(edgeVisits);
      stats.prevMoveLoc = moveLoc;
    }
  }

  //Find all children and compute weighting of the children based on their values
  if(includeWeightFactors) {
    double totalChildWeight = 0.0;
    for(int i = 0; i<numChildren; i++) {
      totalChildWeight += statsBuf[i].weightAdjusted;
    }
    if(searchParams.useNoisePruning) {
      double policyProbsBuf[NNPos::MAX_NN_POLICY_SIZE];
      for(int i = 0; i<numChildren; i++)
        policyProbsBuf[i] = std::max(1e-30, (double)policyProbs[getPos(statsBuf[i].prevMoveLoc)]);
      totalChildWeight = pruneNoiseWeight(statsBuf, numChildren, totalChildWeight, policyProbsBuf);
    }
    double amountToSubtract = 0.0;
    double amountToPrune = 0.0;
    downweightBadChildrenAndNormalizeWeight(
      numChildren, totalChildWeight, totalChildWeight,
      amountToSubtract, amountToPrune, statsBuf
    );
    for(int i = 0; i<numChildren; i++)
      buf[i].weightFactor = statsBuf[i].weightAdjusted;
  }

  //Fill the rest of the moves directly from policy
  if(numChildren < minMovesToTryToGet) {
    //A bit inefficient, but no big deal
    for(int i = 0; i<minMovesToTryToGet - numChildren; i++) {
      int bestPos = -1;
      double bestPolicy = -1.0;
      for(int pos = 0; pos<NNPos::MAX_NN_POLICY_SIZE; pos++) {
        if(policyProbs[pos] < bestPolicy)
          continue;

        bool alreadyUsed = false;
        for(int j = 0; j<buf.size(); j++) {
          if(getPos(buf[j].move) == pos) {
            alreadyUsed = true;
            break;
          }
        }
        if(alreadyUsed)
          continue;

        bestPos = pos;
        bestPolicy = policyProbs[pos];
      }
      if(bestPos < 0 || bestPolicy < 0.0)
        break;

      Loc bestMove = NNPos::posToLoc(bestPos,rootBoard.x_size,rootBoard.y_size,nnXLen,nnYLen);
      AnalysisData data = getAnalysisDataOfSingleChild(
        NULL, 0, scratchLocs, scratchValues, bestMove, bestPolicy, fpuValue, parentUtility, parentWinLossValue,
        maxPVDepth
      );
      buf.push_back(data);
    }
  }
  std::stable_sort(buf.begin(),buf.end());

  if(duplicateForSymmetries && searchParams.rootSymmetryPruning && rootSymmetries.size() > 1) {
    vector<AnalysisData> newBuf;
    std::set<Loc> isDone;
    for(int i = 0; i<buf.size(); i++) {
      const AnalysisData& data = buf[i];
      for(int symmetry : rootSymmetries) {
        Loc symMove = SymmetryHelpers::getSymLoc(data.move, rootBoard, symmetry);
        if(contains(isDone,symMove))
          continue;
        const std::vector<int>& avoidMoveUntilByLoc = rootPla == P_BLACK ? avoidMoveUntilByLocBlack : avoidMoveUntilByLocWhite;
        if(avoidMoveUntilByLoc.size() > 0 && avoidMoveUntilByLoc[symMove] > 0)
          continue;

        isDone.insert(symMove);
        newBuf.push_back(data);
        //Replace the fields that need to be adjusted for symmetry
        AnalysisData& newData = newBuf.back();
        newData.move = symMove;
        if(symmetry != 0)
          newData.isSymmetryOf = data.move;
        newData.symmetry = symmetry;
        for(int j = 0; j<newData.pv.size(); j++)
          newData.pv[j] = SymmetryHelpers::getSymLoc(newData.pv[j], rootBoard, symmetry);
      }
    }
    buf = std::move(newBuf);
  }

  for(int i = 0; i<buf.size(); i++)
    buf[i].order = i;
}

void Search::printPVForMove(ostream& out, const SearchNode* n, Loc move, int maxDepth) const {
  vector<Loc> buf;
  vector<int64_t> visitsBuf;
  vector<int64_t> edgeVisitsBuf;
  vector<Loc> scratchLocs;
  vector<double> scratchValues;
  appendPVForMove(buf,visitsBuf,edgeVisitsBuf,scratchLocs,scratchValues,n,move,maxDepth);
  for(int i = 0; i<buf.size(); i++) {
    if(i > 0)
      out << " ";
    out << Location::toString(buf[i],rootBoard);
  }
}

void Search::printTree(ostream& out, const SearchNode* node, PrintTreeOptions options, Player perspective) const {
  if(node == NULL)
    return;
  string prefix;
  AnalysisData data;
  {
    vector<Loc> scratchLocs;
    vector<double> scratchValues;
    //Use dummy values for parent
    double policyProb = NAN;
    double fpuValue = 0;
    double parentUtility = 0;
    double parentWinLossValue = 0;
    //Since we don't have an edge from another parent we are following, we just use the visits on the node itself as the edge visits.
    int64_t edgeVisits = node->stats.visits.load(std::memory_order_acquire);
    data = getAnalysisDataOfSingleChild(
      node, edgeVisits, scratchLocs, scratchValues,
      Board::NULL_LOC, policyProb, fpuValue, parentUtility, parentWinLossValue,
      options.maxPVDepth_
    );
    data.weightFactor = NAN;
  }
  perspective = (perspective != P_BLACK && perspective != P_WHITE) ? node->nextPla : perspective;
  printTreeHelper(out, node, options, prefix, 0, 0, data, perspective);
}

void Search::printTreeHelper(
  ostream& out, const SearchNode* n, const PrintTreeOptions& options,
  string& prefix, int64_t origVisits, int depth, const AnalysisData& data, Player perspective
) const {
  if(n == NULL)
    return;

  const SearchNode& node = *n;

  Player perspectiveToUse = (perspective != P_BLACK && perspective != P_WHITE) ? n->nextPla : perspective;
  double perspectiveFactor = perspectiveToUse == P_BLACK ? -1.0 : 1.0;

  if(depth == 0)
    origVisits = data.numVisits;

  //Output for this node
  {
    out << prefix;
    char buf[128];

    out << ": ";

    if(data.numVisits > 0) {
      sprintf(buf,"T %6.2fc ",(perspectiveFactor * data.utility * 100.0));
      out << buf;
      sprintf(buf,"W %6.2fc ",(perspectiveFactor * data.resultUtility * 100.0));
      out << buf;
    }

    // bool hasNNValue = false;
    // double nnResultValue;
    // double nnTotalValue;
    // lock.lock();
    // if(node.nnOutput != nullptr) {
    //   nnResultValue = getResultUtilityFromNN(*node.nnOutput);
    //   nnTotalValue = getUtilityFromNN(*node.nnOutput);
    //   hasNNValue = true;
    // }
    // lock.unlock();

    // if(hasNNValue) {
    //   sprintf(buf,"VW %6.2fc VS %6.2fc ", nnResultValue * 100.0, (nnTotalValue - nnResultValue) * 100.0);
    //   out << buf;
    // }
    // else {
    //   sprintf(buf,"VW ---.--c VS ---.--c ");
    //   out << buf;
    // }

    if(depth > 0 && !isnan(data.lcb)) {
      sprintf(buf,"LCB %7.2fc ", perspectiveFactor * data.lcb * 100.0);
      out << buf;
    }

    if(!isnan(data.policyPrior)) {
      sprintf(buf,"P %5.2f%% ", data.policyPrior * 100.0);
      out << buf;
    }
    if(!isnan(data.weightFactor)) {
      sprintf(buf,"WF %5.1f ", data.weightFactor);
      out << buf;
    }
    if(data.playSelectionValue >= 0 && depth > 0) {
      sprintf(buf,"PSV %7.0f ", data.playSelectionValue);
      out << buf;
    }

    if(options.printSqs_) {
      sprintf(buf,"USQ %7.5f W %6.2f WSQ %8.2f ", data.utilitySqAvg, data.weightSum, data.weightSqSum);
      out << buf;
    }

    if(options.printAvgShorttermError_) {
      double wlError = getAverageShorttermWLError(&node);
      sprintf(buf,"STWL %6.2fc ", wlError);
      out << buf;
    }

    sprintf(buf,"N %7" PRIu64 "  --  ", data.numVisits);
    out << buf;

    printPV(out, data.pv);
    out << endl;
  }

  if(depth >= options.branch_.size()) {
    if(depth >= options.maxDepth_ + options.branch_.size())
      return;
    if(data.numVisits < options.minVisitsToExpand_)
      return;
    if((double)data.numVisits < origVisits * options.minVisitsPropToExpand_)
      return;
  }
  if(depth == options.branch_.size()) {
    out << "---" << PlayerIO::playerToString(node.nextPla) << "(" << (node.nextPla == perspectiveToUse ? "^" : "v") << ")---" << endl;
  }

  vector<AnalysisData> analysisData;
  bool duplicateForSymmetries = false;
  getAnalysisData(node,analysisData,0,true,options.maxPVDepth_,duplicateForSymmetries);

  int numChildren = (int)analysisData.size();

  //Apply filtering conditions, but include children that don't match the filtering condition
  //but where there are children afterward that do, in case we ever use something more complex
  //than plain visits as a filter criterion. Do this by finding the last child that we want as the threshold.
  int lastIdxWithEnoughVisits = numChildren-1;
  while(true) {
    if(lastIdxWithEnoughVisits <= 0)
      break;

    int64_t childVisits = analysisData[lastIdxWithEnoughVisits].numVisits;
    bool hasEnoughVisits = childVisits >= options.minVisitsToShow_
      && (double)childVisits >= origVisits * options.minVisitsPropToShow_;
    if(hasEnoughVisits)
      break;
    lastIdxWithEnoughVisits--;
  }

  int numChildrenToRecurseOn = numChildren;
  if(options.maxChildrenToShow_ < numChildrenToRecurseOn)
    numChildrenToRecurseOn = options.maxChildrenToShow_;
  if(lastIdxWithEnoughVisits+1 < numChildrenToRecurseOn)
    numChildrenToRecurseOn = lastIdxWithEnoughVisits+1;


  for(int i = 0; i<numChildren; i++) {
    const SearchNode* child = analysisData[i].node;
    Loc moveLoc = analysisData[i].move;

    if((depth >= options.branch_.size() && i < numChildrenToRecurseOn) ||
       (depth < options.branch_.size() && moveLoc == options.branch_[depth]))
    {
      size_t oldLen = prefix.length();
      string locStr = Location::toString(moveLoc,rootBoard);
      if(locStr == "pass")
        prefix += "pss";
      else
        prefix += locStr;
      prefix += " ";
      while(prefix.length() < oldLen+4)
        prefix += " ";
      printTreeHelper(
        out,child,options,prefix,origVisits,depth+1,analysisData[i], perspective);
      prefix.erase(oldLen);
    }
  }
}


double Search::getAverageShorttermWLError(const SearchNode* node) const {
  if(node == NULL)
    node = rootNode;
  if(node == NULL)
    return 0.0;
  return getAverageShorttermWLErrorHelper(node);
}

double Search::getAverageShorttermWLErrorHelper(const SearchNode* node) const {
  const NNOutput* nnOutput = node->getNNOutput();
  if(nnOutput == NULL) {
    //This will also be correct for terminal nodes, which have no uncertainty.
    //The caller will scale by weightSum, so this all works as intended.
    return 0.0;
  }

  int childrenCapacity;
  const SearchChildPointer* children = node->getChildren(childrenCapacity);

  int numChildren = 0;
  for(int i = 0; i<childrenCapacity; i++) {
    const SearchNode* child = children[i].getIfAllocated();
    if(child == NULL)
      break;
    numChildren += 1;
  }

  double wlErrorSum = 0.0;
  double weightSum = 0.0;
  {
    double thisNodeWeight = computeWeightFromNNOutput(nnOutput);
    wlErrorSum += nnOutput->shorttermWinlossError * thisNodeWeight;
    weightSum += thisNodeWeight;
  }

  for(int i = numChildren-1; i>=0; i--) {
    const SearchNode* child = children[i].getIfAllocated();
    assert(child != NULL);
    int64_t edgeVisits = children[i].getEdgeVisits();
    double childWeight = child->stats.getChildWeight(edgeVisits);
    double result = getAverageShorttermWLErrorHelper(child);
    wlErrorSum += result * childWeight;
    weightSum += childWeight;
  }

  return wlErrorSum/weightSum;
}


bool Search::getAnalysisJson(
  const Player perspective,
  int analysisPVLen,
  bool includePolicy,
  bool includePVVisits,
  json& ret
) const {
  vector<AnalysisData> buf;
  static constexpr int minMoves = 0;
  static constexpr int OUTPUT_PRECISION = 8;

  const Board& board = rootBoard;
  bool duplicateForSymmetries = true;
  getAnalysisData(buf, minMoves, false, analysisPVLen, duplicateForSymmetries);

  // Stats for all the individual moves
  json moveInfos = json::array();
  for(int i = 0; i < buf.size(); i++) {
    const AnalysisData& data = buf[i];
    double winrate = 0.5 * (1.0 + data.winLossValue);
    double drawrate = data.noResultValue;
    double utility = data.utility;
    double lcb = PlayUtils::getHackedLCBForWinrate(this, data, rootPla);
    double utilityLcb = data.lcb;
    if(perspective == P_BLACK || (perspective != P_BLACK && perspective != P_WHITE && rootPla == P_BLACK)) {
      winrate = 1.0 - winrate;
      lcb = 1.0 - lcb;
      utility = -utility;
      utilityLcb = -utilityLcb;
    }

    json moveInfo;
    moveInfo["move"] = Location::toString(data.move, board);
    moveInfo["visits"] = data.numVisits;
    moveInfo["weight"] = data.weightSum;
    moveInfo["utility"] = Global::roundDynamic(utility, OUTPUT_PRECISION);
    moveInfo["winrate"] = Global::roundDynamic(winrate, OUTPUT_PRECISION);
    moveInfo["drawrate"] = Global::roundDynamic(drawrate, OUTPUT_PRECISION);
    moveInfo["prior"] = Global::roundDynamic(data.policyPrior,OUTPUT_PRECISION);
    moveInfo["lcb"] = Global::roundDynamic(lcb,OUTPUT_PRECISION);
    moveInfo["utilityLcb"] = Global::roundDynamic(utilityLcb,OUTPUT_PRECISION);
    moveInfo["order"] = data.order;
    if(data.isSymmetryOf != Board::NULL_LOC)
      moveInfo["isSymmetryOf"] = Location::toString(data.isSymmetryOf, board);

    json pv = json::array();
    int pvLen = (int)data.pv.size();
    for(int j = 0; j < pvLen; j++)
      pv.push_back(Location::toString(data.pv[j], board));
    moveInfo["pv"] = pv;

    if(includePVVisits) {
      assert(data.pvVisits.size() >= pvLen);
      json pvVisits = json::array();
      for(int j = 0; j < pvLen; j++)
        pvVisits.push_back(data.pvVisits[j]);
      moveInfo["pvVisits"] = pvVisits;

      assert(data.pvEdgeVisits.size() >= pvLen);
      json pvEdgeVisits = json::array();
      for(int j = 0; j < pvLen; j++)
        pvEdgeVisits.push_back(data.pvEdgeVisits[j]);
      moveInfo["pvEdgeVisits"] = pvEdgeVisits;
    }


    moveInfos.push_back(moveInfo);
  }
  ret["moveInfos"] = moveInfos;

  // Stats for root position
  {
    ReportedSearchValues rootVals;
    bool suc = getPrunedRootValues(rootVals);
    if(!suc)
      return false;

    double winrate = 0.5 * (1.0 + rootVals.winLossValue);
    double utility = rootVals.utility;

    if(perspective == P_BLACK || (perspective != P_BLACK && perspective != P_WHITE && rootPla == P_BLACK)) {
      winrate = 1.0 - winrate;
      utility = -utility;
    }

    json rootInfo;
    rootInfo["visits"] = rootVals.visits;
    rootInfo["weight"] = rootVals.weight;
    rootInfo["winrate"] = Global::roundDynamic(winrate,OUTPUT_PRECISION);
    rootInfo["utility"] = Global::roundDynamic(utility,OUTPUT_PRECISION);

    if(rootNode != NULL) {
      const NNOutput* nnOutput = rootNode->getNNOutput();
      if(nnOutput != NULL) {
        rootInfo["rawStWrError"] = Global::roundDynamic(nnOutput->shorttermWinlossError * 0.5,OUTPUT_PRECISION);
        rootInfo["rawVarTimeLeft"] = Global::roundDynamic(nnOutput->varTimeLeft,OUTPUT_PRECISION);
      }
    }

    Hash128 thisHash;
    Hash128 symHash;
    for(int symmetry = 0; symmetry < SymmetryHelpers::NUM_SYMMETRIES; symmetry++) {
      Board symBoard = SymmetryHelpers::getSymBoard(board,symmetry);
      Hash128 hash = symBoard.getSitHash(rootPla);
      if(symmetry == 0) {
        thisHash = hash;
        symHash = hash;
      }
      else {
        if(hash < symHash)
          symHash = hash;
      }
    }
    rootInfo["thisHash"] = Global::uint64ToHexString(thisHash.hash1) + Global::uint64ToHexString(thisHash.hash0);
    rootInfo["symHash"] = Global::uint64ToHexString(symHash.hash1) + Global::uint64ToHexString(symHash.hash0);
    rootInfo["currentPlayer"] = PlayerIO::playerToStringShort(rootPla);

    ret["rootInfo"] = rootInfo;
  }

  // Raw policy prior
  if(includePolicy) {
    float policyProbs[NNPos::MAX_NN_POLICY_SIZE];
    bool suc = getPolicy(policyProbs);
    if(!suc)
      return false;
    json policy = json::array();
    for(int y = 0; y < board.y_size; y++) {
      for(int x = 0; x < board.x_size; x++) {
        int pos = NNPos::xyToPos(x, y, nnXLen);
        policy.push_back(Global::roundDynamic(policyProbs[pos],OUTPUT_PRECISION));
      }
    }

    int passPos = NNPos::locToPos(Board::PASS_LOC, board.x_size, nnXLen, nnYLen);
    policy.push_back(Global::roundDynamic(policyProbs[passPos],OUTPUT_PRECISION));
    ret["policy"] = policy;
  }


  return true;
}

//Compute all the stats of the node based on its children, pruning weights such that they are as expected
//based on policy and utility. This is used to give accurate rootInfo even with a lot of wide root noise
bool Search::getPrunedRootValues(ReportedSearchValues& values) const {
  return getPrunedNodeValues(rootNode,values);
}

bool Search::getPrunedNodeValues(const SearchNode* nodePtr, ReportedSearchValues& values) const {
  if(nodePtr == NULL)
    return false;
  const SearchNode& node = *nodePtr;
  int childrenCapacity;
  const SearchChildPointer* children = node.getChildren(childrenCapacity);

  vector<double> playSelectionValues;
  vector<Loc> locs; // not used
  bool allowDirectPolicyMoves = false;
  bool alwaysComputeLcb = false;
  bool neverUseLcb = true;
  bool suc = getPlaySelectionValues(node,locs,playSelectionValues,NULL,1.0,allowDirectPolicyMoves,alwaysComputeLcb,neverUseLcb,NULL,NULL);
  //If there are no children, or otherwise values could not be computed,
  //then fall back to the normal case and just listen to the values on the node rather than trying
  //to recompute things.
  if(!suc) {
    return getNodeValues(nodePtr,values);
  }

  double winLossValueSum = 0.0;
  double noResultValueSum = 0.0;
  double utilitySum = 0.0;
  double utilitySqSum = 0.0;
  double weightSum = 0.0;
  double weightSqSum = 0.0;
  for(int i = 0; i<childrenCapacity; i++) {
    const SearchNode* child = children[i].getIfAllocated();
    if(child == NULL)
      break;
    int64_t edgeVisits = children[i].getEdgeVisits();
    NodeStats stats = NodeStats(child->stats);

    if(stats.visits <= 0 || stats.weightSum <= 0.0 || edgeVisits <= 0)
      continue;
    double weight = playSelectionValues[i];
    double weightScaling = weight / stats.weightSum;
    winLossValueSum += weight * stats.winLossValueAvg;
    noResultValueSum += weight * stats.noResultValueAvg;
    utilitySum += weight * stats.utilityAvg;
    utilitySqSum += weight * stats.utilitySqAvg;
    weightSqSum += weightScaling * weightScaling * stats.weightSqSum;
    weightSum += weight;
  }

  //Also add in the direct evaluation of this node.
  {
    const NNOutput* nnOutput = node.getNNOutput();
    //If somehow the nnOutput is still null here, skip
    if(nnOutput == NULL)
      return false;
    double winProb = (double)nnOutput->whiteWinProb;
    double lossProb = (double)nnOutput->whiteLossProb;
    double noResultProb = (double)nnOutput->whiteNoResultProb;
    double utility = getResultUtility(winProb - lossProb, noResultProb);

    double weight = computeWeightFromNNOutput(nnOutput);
    winLossValueSum += (winProb - lossProb) * weight;
    noResultValueSum += noResultProb * weight;
    utilitySum += utility * weight;
    utilitySqSum += utility * utility * weight;
    weightSqSum += weight * weight;
    weightSum += weight;
  }
  values = ReportedSearchValues(
    *this,
    winLossValueSum / weightSum,
    noResultValueSum / weightSum,
    utilitySum / weightSum,
    node.stats.weightSum.load(std::memory_order_acquire),
    node.stats.visits.load(std::memory_order_acquire)
  );
  return true;
}
