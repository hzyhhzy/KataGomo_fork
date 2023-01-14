#include "../neuralnet/nninputs.h"

using namespace std;

int NNPos::xyToPos(int x, int y, int nnXLen) {
  return y * nnXLen + x;
}
int NNPos::locToPos(Loc loc, int boardXSize, int nnXLen, int nnYLen) {
  if(loc == Board::PASS_LOC)
    return nnXLen * nnYLen;
  else if(loc == Board::NULL_LOC)
    return nnXLen * (nnYLen + 1);
  return Location::getY(loc,boardXSize) * nnXLen + Location::getX(loc,boardXSize);
}
Loc NNPos::posToLoc(int pos, int boardXSize, int boardYSize, int nnXLen, int nnYLen) {
  if(pos == nnXLen * nnYLen)
    return Board::PASS_LOC;
  int x = pos % nnXLen;
  int y = pos / nnXLen;
  if(x < 0 || x >= boardXSize || y < 0 || y >= boardYSize)
    return Board::NULL_LOC;
  return Location::getLoc(x,y,boardXSize);
}

bool NNPos::isPassPos(int pos, int nnXLen, int nnYLen) {
  return pos == nnXLen * nnYLen;
}

int NNPos::getPolicySize(int nnXLen, int nnYLen) {
  return nnXLen * nnYLen + 1;
}

//-----------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------

const Hash128 MiscNNInputParams::ZOBRIST_PLAYOUT_DOUBLINGS =
  Hash128(0xa5e6114d380bfc1dULL, 0x4160557f1222f4adULL);
const Hash128 MiscNNInputParams::ZOBRIST_NN_POLICY_TEMP =
  Hash128(0xebcbdfeec6f4334bULL, 0xb85e43ee243b5ad2ULL);

//-----------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------

double ScoreValue::whiteWinsOfWinner(Player winner, double drawEquivalentWinsForWhite) {
  if(winner == P_WHITE)
    return 1.0;
  else if(winner == P_BLACK)
    return 0.0;

  assert(winner == C_EMPTY);
  return drawEquivalentWinsForWhite;
}

static const double twoOverPi = 0.63661977236758134308;
static const double piOverTwo = 1.57079632679489661923;

double ScoreValue::whiteScoreDrawAdjust(double finalWhiteMinusBlackScore, double drawEquivalentWinsForWhite, const BoardHistory& hist) {
  return finalWhiteMinusBlackScore + hist.whiteKomiAdjustmentForDraws(drawEquivalentWinsForWhite);
}

double ScoreValue::whiteScoreValueOfScoreSmooth(double finalWhiteMinusBlackScore, double center, double scale, double drawEquivalentWinsForWhite, const Board& b, const BoardHistory& hist) {
  double adjustedScore = finalWhiteMinusBlackScore + hist.whiteKomiAdjustmentForDraws(drawEquivalentWinsForWhite) - center;
  if(b.x_size == b.y_size)
    return atan(adjustedScore / (scale*b.x_size)) * twoOverPi;
  else
    return atan(adjustedScore / (scale*sqrt(b.x_size*b.y_size))) * twoOverPi;
}

double ScoreValue::whiteScoreValueOfScoreSmoothNoDrawAdjust(double finalWhiteMinusBlackScore, double center, double scale, const Board& b) {
  double adjustedScore = finalWhiteMinusBlackScore - center;
  if(b.x_size == b.y_size)
    return atan(adjustedScore / (scale*b.x_size)) * twoOverPi;
  else
    return atan(adjustedScore / (scale*sqrt(b.x_size*b.y_size))) * twoOverPi;
}

double ScoreValue::whiteDScoreValueDScoreSmoothNoDrawAdjust(double finalWhiteMinusBlackScore, double center, double scale, const Board& b) {
  double adjustedScore = finalWhiteMinusBlackScore - center;
  double scaleFactor;
  if(b.x_size == b.y_size)
    scaleFactor = scale*b.x_size;
  else
    scaleFactor = scale*sqrt(b.x_size*b.y_size);

  return scaleFactor / (scaleFactor * scaleFactor + adjustedScore * adjustedScore) * twoOverPi;
}

static double inverse_atan(double x) {
  if(x >= piOverTwo - 1e-6) return 1e6;
  if(x <= -piOverTwo + 1e-6) return -1e6;
  return tan(x);
}

double ScoreValue::approxWhiteScoreOfScoreValueSmooth(double scoreValue, double center, double scale, const Board& b) {
  assert(scoreValue >= -1 && scoreValue <= 1);
  double scoreUnscaled = inverse_atan(scoreValue*piOverTwo);
  if(b.x_size == b.y_size)
    return scoreUnscaled * (scale*b.x_size) + center;
  else
    return scoreUnscaled * (scale*sqrt(b.x_size*b.y_size)) + center;
}

double ScoreValue::whiteScoreMeanSqOfScoreGridded(double finalWhiteMinusBlackScore, double drawEquivalentWinsForWhite) {
  assert((int)(finalWhiteMinusBlackScore * 2) == finalWhiteMinusBlackScore * 2);
  bool finalScoreIsInteger = ((int)finalWhiteMinusBlackScore == finalWhiteMinusBlackScore);
  if(!finalScoreIsInteger)
    return finalWhiteMinusBlackScore * finalWhiteMinusBlackScore;

  double lower = finalWhiteMinusBlackScore - 0.5;
  double upper = finalWhiteMinusBlackScore + 0.5;
  double lowerSq = lower * lower;
  double upperSq = upper * upper;

  return lowerSq + (upperSq - lowerSq) * drawEquivalentWinsForWhite;
}


static bool scoreValueTablesInitialized = false;
static double* expectedSVTable = NULL;
static const int svTableAssumedBSize = NNPos::MAX_BOARD_LEN;
static const int svTableMeanRadius = svTableAssumedBSize*svTableAssumedBSize + NNPos::EXTRA_SCORE_DISTR_RADIUS;
static const int svTableMeanLen = svTableMeanRadius*2;
static const int svTableStdevLen = svTableAssumedBSize*svTableAssumedBSize + NNPos::EXTRA_SCORE_DISTR_RADIUS;

void ScoreValue::freeTables() {
  if(scoreValueTablesInitialized) {
    delete[] expectedSVTable;
    expectedSVTable = NULL;
    scoreValueTablesInitialized = false;
  }
}

void ScoreValue::initTables() {
  assert(!scoreValueTablesInitialized);
  expectedSVTable = new double[svTableMeanLen*svTableStdevLen];

  //Precompute normal PDF
  const int stepsPerUnit = 10; //Must be divisible by 2. This is both the number of segments that we divide points into, and that we divide stdevs into
  const int boundStdevs = 5;
  int minStdevSteps = -boundStdevs*stepsPerUnit;
  int maxStdevSteps = boundStdevs*stepsPerUnit;
  double* normalPDF = new double[(maxStdevSteps-minStdevSteps)+1];
  for(int i = minStdevSteps; i <= maxStdevSteps; i++) {
    double xInStdevs = (double)i / stepsPerUnit;
    double w = exp(-0.5 * xInStdevs * xInStdevs);
    normalPDF[i-minStdevSteps] = w;
  }
  //Precompute scorevalue at increments of 1/stepsPerUnit points
  Board board(svTableAssumedBSize,svTableAssumedBSize);
  int minSVSteps = - (svTableMeanRadius*stepsPerUnit + stepsPerUnit/2 + boundStdevs * svTableStdevLen * stepsPerUnit);
  int maxSVSteps = -minSVSteps;
  double* svPrecomp = new double[(maxSVSteps-minSVSteps)+1];
  for(int i = minSVSteps; i <= maxSVSteps; i++) {
    double mean = (double)i / stepsPerUnit;
    double sv = whiteScoreValueOfScoreSmoothNoDrawAdjust(mean, 0.0, 1.0, board);
    svPrecomp[i-minSVSteps] = sv;
  }

  //Perform numeric integration
  for(int meanIdx = 0; meanIdx < svTableMeanLen; meanIdx++) {
    int meanSteps = (meanIdx - svTableMeanRadius) * stepsPerUnit - stepsPerUnit/2;
    for(int stdevIdx = 0; stdevIdx < svTableStdevLen; stdevIdx++) {
      double wSum = 0.0;
      double wsvSum = 0.0;
      for(int i = minStdevSteps; i <= maxStdevSteps; i++) {
        int xSteps = meanSteps + stdevIdx * i;
        double w = normalPDF[i-minStdevSteps];
        assert(xSteps >= minSVSteps && xSteps <= maxSVSteps);
        double sv = svPrecomp[xSteps-minSVSteps];
        wSum += w;
        wsvSum += w*sv;
      }
      expectedSVTable[meanIdx*svTableStdevLen + stdevIdx] = wsvSum / wSum;
    }
  }

  delete[] normalPDF;
  delete[] svPrecomp;
  scoreValueTablesInitialized = true;
}

double ScoreValue::expectedWhiteScoreValue(double whiteScoreMean, double whiteScoreStdev, double center, double scale, const Board& b) {
  assert(scoreValueTablesInitialized);

  double scaleFactor;
  if(b.x_size == b.y_size)
    scaleFactor = (double)svTableAssumedBSize / (scale * b.x_size);
  else
    scaleFactor = (double)svTableAssumedBSize / (scale * sqrt(b.x_size*b.y_size));

  double meanScaled = (whiteScoreMean - center) * scaleFactor;
  double stdevScaled = whiteScoreStdev * scaleFactor;

  double meanRounded = round(meanScaled);
  double stdevFloored = floor(stdevScaled);
  int meanIdx0 = (int)meanRounded + svTableMeanRadius;
  int stdevIdx0 = (int)stdevFloored;
  int meanIdx1 = meanIdx0+1;
  int stdevIdx1 = stdevIdx0+1;

  if(meanIdx0 < 0) { meanIdx0 = 0; meanIdx1 = 0; }
  if(meanIdx1 >= svTableMeanLen) { meanIdx0 = svTableMeanLen-1; meanIdx1 = svTableMeanLen-1; }
  assert(stdevIdx0 >= 0);
  if(stdevIdx1 >= svTableStdevLen) { stdevIdx0 = svTableStdevLen-1; stdevIdx1 = svTableStdevLen-1; }

  double lambdaMean = meanScaled - meanRounded + 0.5;
  double lambdaStdev = stdevScaled - stdevFloored;

  double a00 = expectedSVTable[meanIdx0*svTableStdevLen + stdevIdx0];
  double a01 = expectedSVTable[meanIdx0*svTableStdevLen + stdevIdx1];
  double a10 = expectedSVTable[meanIdx1*svTableStdevLen + stdevIdx0];
  double a11 = expectedSVTable[meanIdx1*svTableStdevLen + stdevIdx1];

  double b0 = a00 + lambdaStdev*(a01-a00);
  double b1 = a10 + lambdaStdev*(a11-a10);
  return b0 + lambdaMean*(b1-b0);
}

double ScoreValue::getScoreStdev(double scoreMean, double scoreMeanSq) {
  double variance = scoreMeanSq - scoreMean * scoreMean;
  if(variance <= 0.0)
    return 0.0;
  return sqrt(variance);
}

NNOutput::NNOutput()
  :noisedPolicyProbs(NULL)
{}
NNOutput::NNOutput(const NNOutput& other) {
  nnHash = other.nnHash;
  whiteWinProb = other.whiteWinProb;
  whiteLossProb = other.whiteLossProb;
  whiteNoResultProb = other.whiteNoResultProb;
  whiteScoreMean = other.whiteScoreMean;
  whiteScoreMeanSq = other.whiteScoreMeanSq;
  whiteLead = other.whiteLead;
  varTimeLeft = other.varTimeLeft;
  shorttermWinlossError = other.shorttermWinlossError;
  shorttermScoreError = other.shorttermScoreError;

  nnXLen = other.nnXLen;
  nnYLen = other.nnYLen;

  if(other.noisedPolicyProbs != NULL) {
    noisedPolicyProbs = new float[NNPos::MAX_NN_POLICY_SIZE];
    std::copy(other.noisedPolicyProbs, other.noisedPolicyProbs + NNPos::MAX_NN_POLICY_SIZE, noisedPolicyProbs);
  }
  else
    noisedPolicyProbs = NULL;

  std::copy(other.policyProbs, other.policyProbs+NNPos::MAX_NN_POLICY_SIZE, policyProbs);
}

NNOutput::NNOutput(const vector<shared_ptr<NNOutput>>& others) {
  assert(others.size() < 1000000);
  int len = (int)others.size();
  float floatLen = (float)len;
  assert(len > 0);
  for(int i = 1; i<len; i++) {
    assert(others[i]->nnHash == others[0]->nnHash);
  }
  nnHash = others[0]->nnHash;

  whiteWinProb = 0.0f;
  whiteLossProb = 0.0f;
  whiteNoResultProb = 0.0f;
  whiteScoreMean = 0.0f;
  whiteScoreMeanSq = 0.0f;
  whiteLead = 0.0f;
  varTimeLeft = 0.0f;
  shorttermWinlossError = 0.0f;
  shorttermScoreError = 0.0f;
  for(int i = 0; i<len; i++) {
    const NNOutput& other = *(others[i]);
    whiteWinProb += other.whiteWinProb;
    whiteLossProb += other.whiteLossProb;
    whiteNoResultProb += other.whiteNoResultProb;
    whiteScoreMean += other.whiteScoreMean;
    whiteScoreMeanSq += other.whiteScoreMeanSq;
    whiteLead += other.whiteLead;
    varTimeLeft += other.varTimeLeft;
    shorttermWinlossError += other.shorttermWinlossError;
    shorttermScoreError += other.shorttermScoreError;
  }
  whiteWinProb /= floatLen;
  whiteLossProb /= floatLen;
  whiteNoResultProb /= floatLen;
  whiteScoreMean /= floatLen;
  whiteScoreMeanSq /= floatLen;
  whiteLead /= floatLen;
  varTimeLeft /= floatLen;
  shorttermWinlossError /= floatLen;
  shorttermScoreError /= floatLen;

  nnXLen = others[0]->nnXLen;
  nnYLen = others[0]->nnYLen;

  noisedPolicyProbs = NULL;

  //For technical correctness in case of impossibly rare hash collisions:
  //Just give up if they don't all match in move legality
  {
    bool mismatch = false;
    std::fill(policyProbs, policyProbs + NNPos::MAX_NN_POLICY_SIZE, 0.0f);
    for(int i = 0; i<len; i++) {
      const NNOutput& other = *(others[i]);
      for(int pos = 0; pos<NNPos::MAX_NN_POLICY_SIZE; pos++) {
        if(i > 0 && (policyProbs[pos] < 0) != (other.policyProbs[pos] < 0))
          mismatch = true;
        policyProbs[pos] += other.policyProbs[pos];
      }
    }
    //In case of mismatch, just take the first one
    //This should basically never happen, only on true hash collisions
    if(mismatch) {
      const NNOutput& other = *(others[0]);
      std::copy(other.policyProbs, other.policyProbs + NNPos::MAX_NN_POLICY_SIZE, policyProbs);
    }
    else {
      for(int pos = 0; pos<NNPos::MAX_NN_POLICY_SIZE; pos++)
        policyProbs[pos] /= floatLen;
    }
  }

}

NNOutput& NNOutput::operator=(const NNOutput& other) {
  if(&other == this)
    return *this;
  nnHash = other.nnHash;
  whiteWinProb = other.whiteWinProb;
  whiteLossProb = other.whiteLossProb;
  whiteNoResultProb = other.whiteNoResultProb;
  whiteScoreMean = other.whiteScoreMean;
  whiteScoreMeanSq = other.whiteScoreMeanSq;
  whiteLead = other.whiteLead;
  varTimeLeft = other.varTimeLeft;
  shorttermWinlossError = other.shorttermWinlossError;
  shorttermScoreError = other.shorttermScoreError;

  nnXLen = other.nnXLen;
  nnYLen = other.nnYLen;

  if(noisedPolicyProbs != NULL)
    delete[] noisedPolicyProbs;
  if(other.noisedPolicyProbs != NULL) {
    noisedPolicyProbs = new float[NNPos::MAX_NN_POLICY_SIZE];
    std::copy(other.noisedPolicyProbs, other.noisedPolicyProbs + NNPos::MAX_NN_POLICY_SIZE, noisedPolicyProbs);
  }
  else
    noisedPolicyProbs = NULL;

  std::copy(other.policyProbs, other.policyProbs+NNPos::MAX_NN_POLICY_SIZE, policyProbs);

  return *this;
}


NNOutput::~NNOutput() {
  if(noisedPolicyProbs != NULL) {
    delete[] noisedPolicyProbs;
    noisedPolicyProbs = NULL;
  }
}


void NNOutput::debugPrint(ostream& out, const Board& board) {
  out << "Win " << Global::strprintf("%.2fc",whiteWinProb*100) << endl;
  out << "Loss " << Global::strprintf("%.2fc",whiteLossProb*100) << endl;
  out << "NoResult " << Global::strprintf("%.2fc",whiteNoResultProb*100) << endl;
  out << "ScoreMean " << Global::strprintf("%.2f",whiteScoreMean) << endl;
  out << "ScoreMeanSq " << Global::strprintf("%.1f",whiteScoreMeanSq) << endl;
  out << "Lead " << Global::strprintf("%.2f",whiteLead) << endl;
  out << "VarTimeLeft " << Global::strprintf("%.1f",varTimeLeft) << endl;
  out << "STWinlossError " << Global::strprintf("%.3f",shorttermWinlossError) << endl;
  out << "STScoreError " << Global::strprintf("%.1f",shorttermScoreError) << endl;

  out << "Policy" << endl;
  for(int y = 0; y<board.y_size; y++) {
    for(int x = 0; x<board.x_size; x++) {
      int pos = NNPos::xyToPos(x,y,nnXLen);
      float prob = policyProbs[pos];
      if(prob < 0)
        out << "   - ";
      else
        out << Global::strprintf("%4d ", (int)round(prob * 1000));
    }
    out << endl;
  }

}

//-------------------------------------------------------------------------------------------------------------

static void copyWithSymmetry(const float* src, float* dst, int nSize, int hSize, int wSize, int cSize, bool useNHWC, int symmetry, bool reverse) {
  bool transpose = (symmetry & 0x4) != 0 && hSize == wSize;
  bool flipX = (symmetry & 0x2) != 0;
  bool flipY = (symmetry & 0x1) != 0;
  if(transpose && !reverse)
    std::swap(flipX,flipY);
  if(useNHWC) {
    int nStride = hSize * wSize * cSize;
    int hStride = wSize * cSize;
    int wStride = cSize;
    int hBaseNew = 0; int hStrideNew = hStride;
    int wBaseNew = 0; int wStrideNew = wStride;

    if(flipY) { hBaseNew = (hSize-1) * hStrideNew; hStrideNew = -hStrideNew; }
    if(flipX) { wBaseNew = (wSize-1) * wStrideNew; wStrideNew = -wStrideNew; }

    if(transpose)
      std::swap(hStrideNew,wStrideNew);

    for(int n = 0; n<nSize; n++) {
      for(int h = 0; h<hSize; h++) {
        int nhOld = n * nStride + h*hStride;
        int nhNew = n * nStride + hBaseNew + h*hStrideNew;
        for(int w = 0; w<wSize; w++) {
          int nhwOld = nhOld + w*wStride;
          int nhwNew = nhNew + wBaseNew + w*wStrideNew;
          for(int c = 0; c<cSize; c++) {
            dst[nhwNew + c] = src[nhwOld + c];
          }
        }
      }
    }
  }
  else {
    int ncSize = nSize * cSize;
    int ncStride = hSize * wSize;
    int hStride = wSize;
    int wStride = 1;
    int hBaseNew = 0; int hStrideNew = hStride;
    int wBaseNew = 0; int wStrideNew = wStride;

    if(flipY) { hBaseNew = (hSize-1) * hStrideNew; hStrideNew = -hStrideNew; }
    if(flipX) { wBaseNew = (wSize-1) * wStrideNew; wStrideNew = -wStrideNew; }

    if(transpose)
      std::swap(hStrideNew,wStrideNew);

    for(int nc = 0; nc<ncSize; nc++) {
      for(int h = 0; h<hSize; h++) {
        int nchOld = nc * ncStride + h*hStride;
        int nchNew = nc * ncStride + hBaseNew + h*hStrideNew;
        for(int w = 0; w<wSize; w++) {
          int nchwOld = nchOld + w*wStride;
          int nchwNew = nchNew + wBaseNew + w*wStrideNew;
          dst[nchwNew] = src[nchwOld];
        }
      }
    }
  }
}


void SymmetryHelpers::copyInputsWithSymmetry(const float* src, float* dst, int nSize, int hSize, int wSize, int cSize, bool useNHWC, int symmetry) {
  copyWithSymmetry(src, dst, nSize, hSize, wSize, cSize, useNHWC, symmetry, false);
}

void SymmetryHelpers::copyOutputsWithSymmetry(const float* src, float* dst, int nSize, int hSize, int wSize, int symmetry) {
  copyWithSymmetry(src, dst, nSize, hSize, wSize, 1, false, symmetry, true);
}

int SymmetryHelpers::invert(int symmetry) {
  if(symmetry == 5)
    return 6;
  if(symmetry == 6)
    return 5;
  return symmetry;
}

int SymmetryHelpers::compose(int firstSymmetry, int nextSymmetry) {
  if(isTranspose(firstSymmetry))
    nextSymmetry = (nextSymmetry & 0x4) | ((nextSymmetry & 0x2) >> 1) | ((nextSymmetry & 0x1) << 1);
  return firstSymmetry ^ nextSymmetry;
}

int SymmetryHelpers::compose(int firstSymmetry, int nextSymmetry, int nextNextSymmetry) {
  return compose(compose(firstSymmetry,nextSymmetry),nextNextSymmetry);
}

Loc SymmetryHelpers::getSymLoc(int x, int y, int xSize, int ySize, int symmetry) {
  bool transpose = (symmetry & 0x4) != 0;
  bool flipX = (symmetry & 0x2) != 0;
  bool flipY = (symmetry & 0x1) != 0;
  if(flipX) { x = xSize - x - 1; }
  if(flipY) { y = ySize - y - 1; }

  if(transpose)
    std::swap(x,y);
  return Location::getLoc(x,y,transpose ? ySize : xSize);
}

Loc SymmetryHelpers::getSymLoc(int x, int y, const Board& board, int symmetry) {
  return getSymLoc(x,y,board.x_size,board.y_size,symmetry);
}

Loc SymmetryHelpers::getSymLoc(Loc loc, const Board& board, int symmetry) {
  if(loc == Board::NULL_LOC || loc == Board::PASS_LOC)
    return loc;
  return getSymLoc(Location::getX(loc,board.x_size), Location::getY(loc,board.x_size), board, symmetry);
}

Loc SymmetryHelpers::getSymLoc(Loc loc, int xSize, int ySize, int symmetry) {
  if(loc == Board::NULL_LOC || loc == Board::PASS_LOC)
    return loc;
  return getSymLoc(Location::getX(loc,xSize), Location::getY(loc,xSize), xSize, ySize, symmetry);
}


Board SymmetryHelpers::getSymBoard(const Board& board, int symmetry) {
  bool transpose = (symmetry & 0x4) != 0;
  bool flipX = (symmetry & 0x2) != 0;
  bool flipY = (symmetry & 0x1) != 0;
  Board symBoard(
    transpose ? board.y_size : board.x_size,
    transpose ? board.x_size : board.y_size
  );
  Loc symKoLoc = Board::NULL_LOC;
  for(int y = 0; y<board.y_size; y++) {
    for(int x = 0; x<board.x_size; x++) {
      Loc loc = Location::getLoc(x,y,board.x_size);
      int symX = flipX ? board.x_size - x - 1 : x;
      int symY = flipY ? board.y_size - y - 1 : y;
      if(transpose)
        std::swap(symX,symY);
      Loc symLoc = Location::getLoc(symX,symY,symBoard.x_size);
      bool suc = symBoard.setStone(symLoc,board.colors[loc]);
      assert(suc);
      (void)suc;
    }
  }
  return symBoard;
}

void SymmetryHelpers::markDuplicateMoveLocs(
  const Board& board,
  const BoardHistory& hist,
  const std::vector<int>* onlySymmetries,
  const std::vector<int>& avoidMoves,
  bool* isSymDupLoc,
  std::vector<int>& validSymmetries
) {
  std::fill(isSymDupLoc, isSymDupLoc + Board::MAX_ARR_SIZE, false);
  validSymmetries.clear();
  validSymmetries.reserve(SymmetryHelpers::NUM_SYMMETRIES);
  validSymmetries.push_back(0);


  //If board has different sizes of x and y, we will not search symmetries involved with transpose.
  int symmetrySearchUpperBound = board.x_size == board.y_size ? SymmetryHelpers::NUM_SYMMETRIES : SymmetryHelpers::NUM_SYMMETRIES_WITHOUT_TRANSPOSE;

  for(int symmetry = 1; symmetry < symmetrySearchUpperBound; symmetry++) {
    if(onlySymmetries != NULL && !contains(*onlySymmetries,symmetry))
      continue;

    bool isBoardSym = true;
    for(int y = 0; y < board.y_size; y++) {
      for(int x = 0; x < board.x_size; x++) {
        Loc loc = Location::getLoc(x, y, board.x_size);
        Loc symLoc = getSymLoc(x, y, board,symmetry);
        bool isStoneSym = (board.colors[loc] == board.colors[symLoc]);
        if(!isStoneSym ) {
          isBoardSym = false;
          break;
        }
      }
      if(!isBoardSym)
        break;
    }
    if(isBoardSym)
      validSymmetries.push_back(symmetry);
  }

  //The way we iterate is to achieve https://senseis.xmp.net/?PlayingTheFirstMoveInTheUpperRightCorner%2FDiscussion
  //Reverse the iteration order for white, so that natural openings result in white on the left and black on the right
  //as is common now in SGFs
  if(hist.presumedNextMovePla == P_BLACK) {
    for(int x = board.x_size-1; x >= 0; x--) {
      for(int y = 0; y < board.y_size; y++) {
        Loc loc = Location::getLoc(x, y, board.x_size);
        if(avoidMoves.size() > 0 && avoidMoves[loc] > 0)
          continue;
        for(int symmetry: validSymmetries) {
          if(symmetry == 0)
            continue;
          Loc symLoc = getSymLoc(x, y, board, symmetry);
          if(!isSymDupLoc[loc] && loc != symLoc)
            isSymDupLoc[symLoc] = true;
        }
      }
    }
  }
  else {
    for(int x = 0; x < board.x_size; x++) {
      for(int y = board.y_size-1; y >= 0; y--) {
        Loc loc = Location::getLoc(x, y, board.x_size);
        if(avoidMoves.size() > 0 && avoidMoves[loc] > 0)
          continue;
        for(int symmetry: validSymmetries) {
          if(symmetry == 0)
            continue;
          Loc symLoc = getSymLoc(x, y, board, symmetry);
          if(!isSymDupLoc[loc] && loc != symLoc)
            isSymDupLoc[symLoc] = true;
        }
      }
    }
  }
}

//-------------------------------------------------------------------------------------------------------------

static void setRowBin(float* rowBin, int pos, int feature, float value, int posStride, int featureStride) {
  rowBin[pos * posStride + feature * featureStride] = value;
}

//Currently does NOT depend on history (except for marking ko-illegal spots)
Hash128 NNInputs::getHash(
  const Board& board, const BoardHistory& hist, Player nextPlayer,
  const MiscNNInputParams& nnInputParams
) {
  Hash128 hash = BoardHistory::getSituationRulesAndKoHash(board, hist, nextPlayer, nnInputParams.drawEquivalentWinsForWhite);

  //Fold in whether the game is over or not, since this affects how we compute input features
  //but is not a function necessarily of previous hashed values.
  //If the history is in a weird prolonged state, also treat it similarly.
  if(hist.isGameFinished )
    hash ^= Board::ZOBRIST_GAME_IS_OVER;

  //Fold in asymmetric playout indicator
  if(nnInputParams.playoutDoublingAdvantage != 0) {
    int64_t playoutDoublingsDiscretized = (int64_t)(nnInputParams.playoutDoublingAdvantage*256.0f);
    hash.hash0 += Hash::splitMix64((uint64_t)playoutDoublingsDiscretized);
    hash.hash1 += Hash::basicLCong((uint64_t)playoutDoublingsDiscretized);
    hash ^= MiscNNInputParams::ZOBRIST_PLAYOUT_DOUBLINGS;
  }

  //Fold in policy temperature
  if(nnInputParams.nnPolicyTemperature != 1.0f) {
    int64_t nnPolicyTemperatureDiscretized = (int64_t)(nnInputParams.nnPolicyTemperature*2048.0f);
    hash.hash0 ^= Hash::basicLCong2((uint64_t)nnPolicyTemperatureDiscretized);
    hash.hash1 = Hash::splitMix64(hash.hash1 + (uint64_t)nnPolicyTemperatureDiscretized);
    hash.hash0 += hash.hash1;
    hash ^= MiscNNInputParams::ZOBRIST_NN_POLICY_TEMP;
  }

  return hash;
}

//===========================================================================================
//INPUTSVERSION 7
//===========================================================================================


void NNInputs::fillRowV7(
  const Board& board, const BoardHistory& hist, Player nextPlayer,
  const MiscNNInputParams& nnInputParams,
  int nnXLen, int nnYLen, bool useNHWC, float* rowBin, float* rowGlobal
) {
  assert(nnXLen <= NNPos::MAX_BOARD_LEN);
  assert(nnYLen <= NNPos::MAX_BOARD_LEN);
  assert(board.x_size <= nnXLen);
  assert(board.y_size <= nnYLen);
  std::fill(rowBin,rowBin+NUM_FEATURES_SPATIAL_V7*nnXLen*nnYLen,false);
  std::fill(rowGlobal,rowGlobal+NUM_FEATURES_GLOBAL_V7,0.0f);

  Player pla = nextPlayer;
  Player opp = getOpp(pla);
  int xSize = board.x_size;
  int ySize = board.y_size;

  int featureStride;
  int posStride;
  if(useNHWC) {
    featureStride = 1;
    posStride = NNInputs::NUM_FEATURES_SPATIAL_V7;
  }
  else {
    featureStride = nnXLen * nnYLen;
    posStride = 1;
  }

  for(int y = 0; y<ySize; y++) {
    for(int x = 0; x<xSize; x++) {
      int pos = NNPos::xyToPos(x,y,nnXLen);
      Loc loc = Location::getLoc(x,y,xSize);

      //Feature 0 - on board
      setRowBin(rowBin,pos,0, 1.0f, posStride, featureStride);

      Color stone = board.colors[loc];

      //Features 1,2 - pla,opp stone
      //Features 3,4,5 - 1,2,3 libs
      if(stone == pla)
        setRowBin(rowBin,pos,1, 1.0f, posStride, featureStride);
      else if(stone == opp)
        setRowBin(rowBin,pos,2, 1.0f, posStride, featureStride);

    }
  }



  //Global features.
  //The first 5 of them were set already above to flag which of the past 5 moves were passes.

  //Komi and any score adjustments
  float selfKomi = hist.currentSelfKomi(nextPlayer,nnInputParams.drawEquivalentWinsForWhite);
  float bArea = (float)(xSize * ySize);
  //Bound komi just in case
  if(selfKomi > bArea+20.0f)
    selfKomi = bArea+20.0f;
  if(selfKomi < -bArea-20.0f)
    selfKomi = -bArea-20.0f;
  rowGlobal[5] = selfKomi/20.0f;

  //Scoring
  if(hist.rules.scoringRule == Rules::SCORING_AREA) {}
  else
    ASSERT_UNREACHABLE;


  //Used for handicap play
  //Parameter 15 is used because there's actually a discontinuity in how training behavior works when this is
  //nonzero, no matter how slightly.
  if(nnInputParams.playoutDoublingAdvantage != 0) {
    rowGlobal[15] = 1.0;
    rowGlobal[16] = (float)(0.5 * nnInputParams.playoutDoublingAdvantage);
  }


  //Provide parity information about the board size and komi
  //This comes from the following observation:
  //From white's perspective:
  //Komi = 0.0 - Draw possible
  //Komi = 0.5 - Win the games we would have drawn with komi 0.0
  //Komi = 1.0 - Usually no difference from komi 0.5
  //Komi = 1.5 - Usually no difference from komi 0.5
  //Komi = 2.0 - Draw possible
  //If we were to assign an "effective goodness" to these komis in order it would look like
  //0 1 1 1 2 3 3 3 4 5 5 5 6 ...
  //since when away from the right parity, increasing the komi doesn't help us except in cases of seki with odd numbers of dame.
  //If we were to add 0.5 times a vector like:
  //0 -1 0 1 0 -1 0 1 0 -1 0 ...
  //Then this would become a linear function and hopefully easier for a neural net to learn.
  //We expect that this is hard for a neural net to learn since it depends on the parity of the board size
  //and is very "xor"like.
  //So we provide it as an input.
  //Since we are using a model where games are jittered by 0.5 (see BoardHistory::whiteKomiAdjustmentForDraws)
  //in theory right thing to first order to provide should be a triangular wave with a period of 2 komi points:
  //  ../\........
  //  ./..\.......
  //  /....\..../.
  //  ......\../..
  //  .......\/...
  //The upsloping part of the wave is centered around the komi value where you could draw
  //since komi is extra valuable when it turns losses into draws into wins, peaking at the komi value where you could draw + 0.5.
  //It's downsloping around the komi value where you can't draw, since the marginal komi there is nearly useless, not causing you to win
  //more games except in case of odd-dame seki.

  if(hist.rules.scoringRule == Rules::SCORING_AREA ) {
    bool boardAreaIsEven = (xSize*ySize) % 2 == 0;

    //What is the parity of the komi values that can produce jigos?
    bool drawableKomisAreEven = boardAreaIsEven;

    //Find the difference between the komi viewed from our perspective and the nearest drawable komi below it.
    float komiFloor;
    if(drawableKomisAreEven)
      komiFloor = floor(selfKomi / 2.0f) * 2.0f;
    else
      komiFloor = floor((selfKomi-1.0f) / 2.0f) * 2.0f + 1.0f;

    //Cap just in case we have floating point weirdness
    float delta = selfKomi - komiFloor;
    assert(delta >= -0.0001f);
    assert(delta <= 2.0001f);
    if(delta < 0.0f)
      delta = 0.0f;
    if(delta > 2.0f)
      delta = 2.0f;

    //Create the triangle wave based on the difference
    float wave;
    if(delta < 0.5f)
      wave = delta;
    else if(delta < 1.5f)
      wave = 1.0f-delta;
    else
      wave = delta-2.0f;

    //NOTE: If ever changing which feature this is, must also update index in model.py where we multiply it into the scorebelief parity vector
    rowGlobal[18] = wave;
  }

}
