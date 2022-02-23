#include "../neuralnet/nninputs.h"
#include "../vcfsolver/VCFsolver.h"
#include "../forbiddenPoint/ForbiddenPointFinder.h"
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



double ScoreValue::getScoreStdev(double scoreMean, double scoreMeanSq) {
  double variance = scoreMeanSq - scoreMean * scoreMean;
  if(variance <= 0.0)
    return 0.0;
  return sqrt(variance);
}



//-----------------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------------


NNOutput::NNOutput()
  :whiteOwnerMap(NULL),noisedPolicyProbs(NULL)
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
  if(other.whiteOwnerMap != NULL) {
    whiteOwnerMap = new float[nnXLen * nnYLen];
    std::copy(other.whiteOwnerMap, other.whiteOwnerMap + nnXLen * nnYLen, whiteOwnerMap);
  }
  else
    whiteOwnerMap = NULL;

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

  {
    float whiteOwnerMapCount = 0.0f;
    whiteOwnerMap = NULL;
    for(int i = 0; i<len; i++) {
      const NNOutput& other = *(others[i]);
      if(other.whiteOwnerMap != NULL) {
        if(whiteOwnerMap == NULL) {
          whiteOwnerMap = new float[nnXLen * nnYLen];
          std::fill(whiteOwnerMap, whiteOwnerMap + nnXLen * nnYLen, 0.0f);
        }
        whiteOwnerMapCount += 1.0f;
        for(int pos = 0; pos<nnXLen*nnYLen; pos++)
          whiteOwnerMap[pos] += other.whiteOwnerMap[pos];
      }
    }
    if(whiteOwnerMap != NULL) {
      assert(whiteOwnerMapCount > 0);
      for(int pos = 0; pos<nnXLen*nnYLen; pos++)
        whiteOwnerMap[pos] /= whiteOwnerMapCount;
    }
  }

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
  if(whiteOwnerMap != NULL)
    delete[] whiteOwnerMap;
  if(other.whiteOwnerMap != NULL) {
    whiteOwnerMap = new float[nnXLen * nnYLen];
    std::copy(other.whiteOwnerMap, other.whiteOwnerMap + nnXLen * nnYLen, whiteOwnerMap);
  }
  else
    whiteOwnerMap = NULL;
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
  if(whiteOwnerMap != NULL) {
    delete[] whiteOwnerMap;
    whiteOwnerMap = NULL;
  }
  if(noisedPolicyProbs != NULL) {
    delete[] noisedPolicyProbs;
    noisedPolicyProbs = NULL;
  }
}


void NNOutput::debugPrint(ostream& out, const Board& board) {
  out << "Win " << Global::strprintf("%.2fc",whiteWinProb*100) << endl;
  out << "Loss " << Global::strprintf("%.2fc",whiteLossProb*100) << endl;
  out << "NoResult " << Global::strprintf("%.2fc",whiteNoResultProb*100) << endl;
  out << "ScoreMean " << Global::strprintf("%.1f",whiteScoreMean) << endl;
  out << "ScoreMeanSq " << Global::strprintf("%.1f",whiteScoreMeanSq) << endl;
  out << "Lead " << Global::strprintf("%.1f",whiteLead) << endl;
  out << "VarTimeLeft " << Global::strprintf("%.1f",varTimeLeft) << endl;
  out << "STWinlossError " << Global::strprintf("%.1f",shorttermWinlossError) << endl;
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

  if(whiteOwnerMap != NULL) {
    for(int y = 0; y<board.y_size; y++) {
      for(int x = 0; x<board.x_size; x++) {
        int pos = NNPos::xyToPos(x,y,nnXLen);
        float whiteOwn = whiteOwnerMap[pos];
        out << Global::strprintf("%5d ", (int)round(whiteOwn * 1000));
      }
      out << endl;
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
  for(int y = 0; y<board.y_size; y++) {
    for(int x = 0; x<board.x_size; x++) {
      Loc loc = Location::getLoc(x,y,board.x_size);
      int symX = flipX ? board.x_size - x - 1 : x;
      int symY = flipY ? board.y_size - y - 1 : y;
      if(transpose)
        std::swap(symX,symY);
      Loc symLoc = Location::getLoc(symX,symY,symBoard.x_size);
      symBoard.setStone(symLoc,board.colors[loc]);
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
        if(!isStoneSym) {
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
  Hash128 hash = BoardHistory::getSituationRulesHash(board, hist, nextPlayer, nnInputParams.drawEquivalentWinsForWhite);

  //Fold in whether the game is over or not, since this affects how we compute input features
  //but is not a function necessarily of previous hashed values.
  //If the history is in a weird prolonged state, also treat it similarly.
  if(hist.isGameFinished)
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
  int nnXLen, int nnYLen, bool useNHWC, float* rowBin, float* rowGlobal, ResultBeforeNN& resultbeforenn
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
  resultbeforenn.init(board,hist,nextPlayer);


  CForbiddenPointFinder fpf(board.x_size);
#ifdef FORBIDDEN_FEATURE 
  if (hist.rules.basicRule == Rules::BASICRULE_RENJU)
  {

    for (int x = 0; x < board.x_size; x++)
      for (int y = 0; y < board.y_size; y++)
      {
        fpf.SetStone(x, y, board.colors[Location::getLoc(x, y, board.x_size)]);
      }
  }
#endif

  for (int y = 0; y < ySize; y++) {
    for (int x = 0; x < xSize; x++) {
      int pos = NNPos::xyToPos(x, y, nnXLen);
      Loc loc = Location::getLoc(x, y, xSize);

      //Feature 0 - on board
      setRowBin(rowBin, pos, 0, 1.0f, posStride, featureStride);

      Color stone = board.colors[loc];

      //Features 1,2 - pla,opp stone
      if (stone == pla)
        setRowBin(rowBin, pos, 1, 1.0f, posStride, featureStride);
      else if (stone == opp)
        setRowBin(rowBin, pos, 2, 1.0f, posStride, featureStride);

      //Features 5 - vcf
#ifdef USE_VCF_FEATURE_IF_USE_VCF
      if (resultbeforenn.winner == nextPlayer && loc == resultbeforenn.myOnlyLoc)setRowBin(rowBin, pos, 5, 1.0f, posStride, featureStride);
#endif

#ifdef FORBIDDEN_FEATURE 
      if (hist.rules.basicRule == Rules::BASICRULE_RENJU)
      {
        if (pla == C_BLACK)
        {
          if (fpf.isForbidden(x, y)) setRowBin(rowBin, pos, 3, 1.0f, posStride, featureStride);
        }
        else if (pla == C_WHITE)
        {

          if (fpf.isForbidden(x, y)) setRowBin(rowBin, pos, 4, 1.0f, posStride, featureStride);

        }
      }
#endif 
    }
  }
  //Tax
  //if(hist.rules.taxRule == Rules::TAX_NONE) {}
  //else if(hist.rules.taxRule == Rules::TAX_SEKI)
  //  rowGlobal[15] = 1.0f;
  //else if(hist.rules.taxRule == Rules::TAX_ALL) {\
  //  rowGlobal[15] = 2.0f;
  //}
  //else
  //  ASSERT_UNREACHABLE;

  rowGlobal[5] = nextPlayer == P_BLACK ? -1 : 1;
  //rowGlobal[6] =1;// hist.rules.koRule == Rules::KO_SITUATIONAL;//situational = sixNotWin
#ifdef USE_VCF_FEATURE_IF_USE_VCF
  if (resultbeforenn.myVCFresult == 1)rowGlobal[7] = 1.0;//can vcf
  else if (resultbeforenn.myVCFresult == 2)rowGlobal[8] = 1.0;//cannot vcf
  if (resultbeforenn.oppVCFresult == 1)rowGlobal[9] = 1.0;//opp can vcf
  else if (resultbeforenn.oppVCFresult == 2)rowGlobal[10] = 1.0;//opp cannot vcf
#endif

  //rowGlobal[11] = nextPlayer == P_BLACK ? -nnInputParams.noResultUtilityForWhite : nnInputParams.noResultUtilityForWhite;



  //Used for handicap play
  //Parameter 15 is used because there's actually a discontinuity in how training behavior works when this is
  //nonzero, no matter how slightly.
  if(nnInputParams.playoutDoublingAdvantage != 0) {
    rowGlobal[15] = 1.0;
    rowGlobal[16] = (float)(0.5 * nnInputParams.playoutDoublingAdvantage);
  }


  

}
