#ifndef NEURALNET_NNINPUTS_H_
#define NEURALNET_NNINPUTS_H_

#include <memory>

#include "../core/global.h"
#include "../core/hash.h"
#include "../core/rand.h"
#include "../game/board.h"
#include "../game/boardhistory.h"
#include "../game/rules.h"
#include "../game/gamelogic.h"

namespace NNPos {
  constexpr int MAX_BOARD_LEN = Board::MAX_LEN;
  constexpr int MAX_BOARD_AREA = MAX_BOARD_LEN * MAX_BOARD_LEN;
  //Policy output adds +1 for the pass move
  constexpr int MAX_NN_POLICY_SIZE = MAX_BOARD_AREA + 1;
  // Extra score distribution radius, used for writing score in data rows and for the neural net score belief output
  constexpr int EXTRA_SCORE_DISTR_RADIUS = 60;

  int xyToPos(int x, int y, int nnXLen);
  int locToPos(Loc loc, int boardXSize, int nnXLen, int nnYLen);
  Loc posToLoc(int pos, int boardXSize, int boardYSize, int nnXLen, int nnYLen);
  bool isPassPos(int pos, int nnXLen, int nnYLen);
  int getPolicySize(int nnXLen, int nnYLen);
}

namespace NNInputs {
  constexpr int SYMMETRY_NOTSPECIFIED = -1;
  constexpr int SYMMETRY_ALL = -2;
}

struct MiscNNInputParams {
  double noResultUtilityForWhite = 0.0;
  //todo£ºnoResultUtilityReduce
  double playoutDoublingAdvantage = 0.0;
  float nnPolicyTemperature = 1.0f;

  GameLogic::ResultsBeforeNN resultsBeforeNN = GameLogic::ResultsBeforeNN();

  // If no symmetry is specified, it will use default or random based on config, unless node is already cached.
  int symmetry = NNInputs::SYMMETRY_NOTSPECIFIED;

  static const Hash128 ZOBRIST_PLAYOUT_DOUBLINGS;
  static const Hash128 ZOBRIST_NN_POLICY_TEMP;
  static const Hash128 ZOBRIST_NO_RESULT_UTILITY;
  static const Hash128 ZOBRIST_USE_VCF;
  static const Hash128 ZOBRIST_USE_FORBIDDEN_FEATURE;
};

namespace NNInputs {

  //2021
  const int NUM_FEATURES_SPATIAL_V7 = 22;
  const int NUM_FEATURES_GLOBAL_V7 = 19;

  //2022 multi-rules, vcn,fpw,mm
  const int NUM_FEATURES_SPATIAL_V101 = 22;
  const int NUM_FEATURES_GLOBAL_V101 = 39;

  //2024 nnue
  const int NUM_FEATURES_SPATIAL_V102 = 32;
  const int NUM_FEATURES_GLOBAL_V102 = 64;

  Hash128 getHash(
    const Board& board, const BoardHistory& boardHistory, Player nextPlayer,
    const MiscNNInputParams& nnInputParams
  );

  void fillRowV7(
    const Board& board, const BoardHistory& boardHistory, Player nextPlayer,
    const MiscNNInputParams& nnInputParams, int nnXLen, int nnYLen, bool useNHWC, float* rowBin, float* rowGlobal
  );
  
  void fillRowV101(
    const Board& board, const BoardHistory& boardHistory, Player nextPlayer,
    const MiscNNInputParams& nnInputParams, int nnXLen, int nnYLen, bool useNHWC, float* rowBin, float* rowGlobal
  );
  
  void fillRowV102(
    const Board& board, const BoardHistory& boardHistory, Player nextPlayer,
    const MiscNNInputParams& nnInputParams, int nnXLen, int nnYLen, bool useNHWC, float* rowBin, float* rowGlobal
  );
}

struct NNOutput {
  static inline uint8_t policyQuant(float p) {
    if(p < 0)
      return 0;
    int32_t t = int32_t(p * 32768.0);
    if(t >= (1 << 15))
      return 255;

    if(t >= (1 << 14))  // 1
      return 7 * 32 + (t - (1 << 14)) / (1 << 9);
    else if(t >= (1 << 13))  // 1/2
      return 6 * 32 + (t - (1 << 13)) / (1 << 8);
    else if(t >= (1 << 12))  // 1/4
      return 5 * 32 + (t - (1 << 12)) / (1 << 7);
    else if(t >= (1 << 11))  // 1/8
      return 4 * 32 + (t - (1 << 11)) / (1 << 6);
    else if(t >= (1 << 10))  // 1/16
      return 7 * 16 + (t - (1 << 10)) / (1 << 6);
    else if(t >= (1 << 9))  // 1/32
      return 6 * 16 + (t - (1 << 9)) / (1 << 5);
    else if(t >= (1 << 8))  // 1/64
      return 5 * 16 + (t - (1 << 8)) / (1 << 4);
    else if(t >= (1 << 7))  // 1/128
      return 4 * 16 + (t - (1 << 7)) / (1 << 3);
    else if(t >= (1 << 6))  // 1/512~1/256
      return 3 * 16 + (t - (1 << 6)) / (1 << 2);
    else if(t >= (15 << 1))  // 15/16384~1/512
      return 31 + (t - (15 << 1)) / (1 << 1);
    else  // <15/16384
      return 1 + t;
    // min=1/32768
  }

  static inline float policyDequant(uint8_t t) {
    if(t == 0)
      return -1.0;
    int p = 0;
    const float f = 1 / 65536.0;
    if(t < 31)
      p = (-1 + 2 * t) * 1;
    else if(t < 3 * 16)
      p = (2 * t + 1 - 1 * 32) * 2;
    else if(t < 4 * 16)
      p = (2 * t + 1 - 2 * 32) * 4;
    else if(t < 5 * 16)
      p = (2 * t + 1 - 3 * 32) * 8;
    else if(t < 6 * 16)
      p = (2 * t + 1 - 4 * 32) * 16;
    else if(t < 7 * 16)
      p = (2 * t + 1 - 5 * 32) * 32;
    else if(t < 4 * 32)
      p = (2 * t + 1 - 6 * 32) * 64;
    else if(t < 5 * 32)
      p = (2 * t + 1 - 3 * 64) * 64;
    else if(t < 6 * 32)
      p = (2 * t + 1 - 4 * 64) * 128;
    else if(t < 7 * 32)
      p = (2 * t + 1 - 5 * 64) * 256;
    else
      p = (2 * t + 1 - 6 * 64) * 512;
    return float(p) * f;
  }

  Hash128 nnHash; //NNInputs - getHash

  //Initially from the perspective of the player to move at the time of the eval, fixed up later in nnEval.cpp
  //to be the value from white's perspective.
  //These three are categorial probabilities for each outcome.
  float whiteWinProb;
  float whiteLossProb;
  float whiteNoResultProb;

  //Expected arrival time of remaining game variance, in turns, weighted by variance, only when modelVersion >= 9
  float varTimeLeft;
  //A metric indicating the "typical" error in the winloss value or the score that the net expects, relative to the
  //short-term future MCTS value.
  float shorttermWinlossError;

  //Indexed by pos rather than loc
  //Values in here will be set to negative for illegal moves, including superko
  int8_t policyProbsQuantized[NNPos::MAX_NN_POLICY_SIZE];

  int nnXLen;
  int nnYLen;

  //If not NULL, then contains policy with dirichlet noise or any other noise adjustments for this node
  float* noisedPolicyProbs;

  NNOutput(); //Does NOT initialize values
  NNOutput(const NNOutput& other);
  ~NNOutput();

  //Averages the others. Others must be nonempty and share the same nnHash (i.e. be for the same board, except if somehow the hash collides)
  //Does NOT carry over noisedPolicyProbs.
  NNOutput(const std::vector<std::shared_ptr<NNOutput>>& others);

  NNOutput& operator=(const NNOutput&);

  inline float getPolicyProb(int pos) const { return policyDequant(policyProbsQuantized[pos]); }
  inline float getPolicyProbMaybeNoised(int pos) const { return noisedPolicyProbs != NULL ? noisedPolicyProbs[pos] : policyDequant(policyProbsQuantized[pos]); }
  void debugPrint(std::ostream& out, const Board& board);
  inline int getPos(Loc loc, const Board& board) const { return NNPos::locToPos(loc, board.x_size, nnXLen, nnYLen ); }

};

namespace SymmetryHelpers {
  //A symmetry is 3 bits flipY(bit 0), flipX(bit 1), transpose(bit 2). They are applied in that order.
  //The first four symmetries only reflect, and do not transpose X and Y.
  constexpr int NUM_SYMMETRIES = 8;
  constexpr int NUM_SYMMETRIES_WITHOUT_TRANSPOSE = 4;

  //These two IGNORE transpose if hSize and wSize do not match. So non-square transposes are disallowed.
  //copyOutputsWithSymmetry performs the inverse of symmetry.
  void copyInputsWithSymmetry(const float* src, float* dst, int nSize, int hSize, int wSize, int cSize, bool useNHWC, int symmetry);
  void copyOutputsWithSymmetry(const float* src, float* dst, int nSize, int hSize, int wSize, int symmetry);

  //Applies a symmetry to a location
  Loc getSymLoc(int x, int y, const Board& board, int symmetry);
  Loc getSymLoc(Loc loc, const Board& board, int symmetry);
  Loc getSymLoc(int x, int y, int xSize, int ySize, int symmetry);
  Loc getSymLoc(Loc loc, int xSize, int ySize, int symmetry);

  //Applies a symmetry to a board
  Board getSymBoard(const Board& board, int symmetry);

  //Get the inverse of the specified symmetry
  int invert(int symmetry);
  //Get the symmetry equivalent to first applying firstSymmetry and then applying nextSymmetry.
  int compose(int firstSymmetry, int nextSymmetry);
  int compose(int firstSymmetry, int nextSymmetry, int nextNextSymmetry);

  inline bool isTranspose(int symmetry) { return (symmetry & 0x4) != 0; }
  inline bool isFlipX(int symmetry) { return (symmetry & 0x2) != 0; }
  inline bool isFlipY(int symmetry) { return (symmetry & 0x1) != 0; }

  //Fill isSymDupLoc with true on all but one copy of each symmetrically equivalent move, and false everywhere else.
  //isSymDupLocs should be an array of size Board::MAX_ARR_SIZE
  //If onlySymmetries is not NULL, will only consider the symmetries specified there.
  //validSymmetries will be filled with all symmetries of the current board, including using history for checking ko/superko and some encore-related state.
  //This implementation is dependent on specific order of the symmetries (i.e. transpose is coded as 0x4)
  //Will pretend moves that have a nonzero value in avoidMoves do not exist.
  void markDuplicateMoveLocs(
    const Board& board,
    const BoardHistory& hist,
    const std::vector<int>* onlySymmetries,
    const std::vector<int>& avoidMoves,
    bool* isSymDupLoc,
    std::vector<int>& validSymmetries
  );
}

//Utility functions for computing the "scoreValue", the unscaled utility of various numbers of points, prior to multiplication by
//staticScoreUtilityFactor or dynamicScoreUtilityFactor (see searchparams.h)
namespace ScoreValue {

  //The number of wins a game result should count as
  double whiteWinsOfWinner(Player winner, double noResultUtilityForWhite);

}

#endif  // NEURALNET_NNINPUTS_H_
