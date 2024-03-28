#include "../neuralnet/nneval.h"
#include "../neuralnet/modelversion.h"
#include "../game/gamelogic.h"

using namespace std;

//-------------------------------------------------------------------------------------

NNResultBuf::NNResultBuf()
  : clientWaitingForResult(),
    resultMutex(),
    hasResult(false),
    boardXSizeForServer(0),
    boardYSizeForServer(0),
    rowSpatialSize(0),
    rowGlobalSize(0),
    rowSpatial(NULL),
    rowGlobal(NULL),
    result(nullptr),
    errorLogLockout(false),
    // If no symmetry is specified, it will use default or random based on config.
    symmetry(NNInputs::SYMMETRY_NOTSPECIFIED)
{}

NNResultBuf::~NNResultBuf() {
  if(rowSpatial != NULL)
    delete[] rowSpatial;
  if(rowGlobal != NULL)
    delete[] rowGlobal;
}

//-------------------------------------------------------------------------------------

NNServerBuf::NNServerBuf(const NNEvaluator& nnEval, const LoadedModel* model)
  :inputBuffers(NULL),
   resultBufs(NULL)
{
  int maxNumRows = nnEval.getMaxBatchSize();
  if(model != NULL)
    inputBuffers = NeuralNet::createInputBuffers(model,maxNumRows,nnEval.getNNXLen(),nnEval.getNNYLen());
  resultBufs = new NNResultBuf*[maxNumRows];
  for(int i = 0; i < maxNumRows; i++)
    resultBufs[i] = NULL;
}

NNServerBuf::~NNServerBuf() {
  if(inputBuffers != NULL)
    NeuralNet::freeInputBuffers(inputBuffers);
  inputBuffers = NULL;
  //Pointers inside here don't need to be deleted, they simply point to the clients waiting for results
  delete[] resultBufs;
  resultBufs = NULL;
}

//-------------------------------------------------------------------------------------

NNEvaluator::NNEvaluator(
  const string& mName,
  const string& mFileName,
  const string& expectedSha256,
  Logger* lg,
  int maxBatchSize,
  int maxConcurrentEvals,
  int xLen,
  int yLen,
  bool rExactNNLen,
  bool iUseNHWC,
  int nnCacheSizePowerOfTwo,
  int nnMutexPoolSizePowerofTwo,
  bool skipNeuralNet,
  const string& openCLTunerFile,
  const string& homeDataDirOverride,
  bool openCLReTunePerBoardSize,
  enabled_t useFP16Mode,
  enabled_t useNHWCMode,
  int numThr,
  const vector<int>& gpuIdxByServerThr,
  const string& rSeed,
  bool doRandomize,
  int defaultSymmetry
)
  :modelName(mName),
   modelFileName(mFileName),
   nnXLen(xLen),
   nnYLen(yLen),
   requireExactNNLen(rExactNNLen),
   policySize(NNPos::getPolicySize(xLen,yLen)),
   inputsUseNHWC(iUseNHWC),
   usingFP16Mode(useFP16Mode),
   usingNHWCMode(useNHWCMode),
   numThreads(numThr),
   gpuIdxByServerThread(gpuIdxByServerThr),
   randSeed(rSeed),
   debugSkipNeuralNet(skipNeuralNet),
   computeContext(NULL),
   loadedModel(NULL),
   nnCacheTable(NULL),
   logger(lg),
   numServerThreadsEverSpawned(0),
   serverThreads(),
   maxNumRows(maxBatchSize),
   numResultBufss(),
   numResultBufssMask(),
   m_numRowsProcessed(0),
   m_numBatchesProcessed(0),
   serverWaitingForBatchStart(),
   bufferMutex(),
   isKilled(false),
   numServerThreadsStartingUp(0),
   mainThreadWaitingForSpawn(),
   numOngoingEvals(0),
   numWaitingEvals(0),
   numEvalsToAwaken(0),
   waitingForFinish(),
   currentDoRandomize(doRandomize),
   currentDefaultSymmetry(defaultSymmetry),
   m_resultBufss(NULL),
   m_currentResultBufsLen(0),
   m_currentResultBufsIdx(0),
   m_oldestResultBufsIdx(0)
{
  if(nnXLen > NNPos::MAX_BOARD_LEN)
    throw StringError("Maximum supported nnEval board size is " + Global::intToString(NNPos::MAX_BOARD_LEN));
  if(nnYLen > NNPos::MAX_BOARD_LEN)
    throw StringError("Maximum supported nnEval board size is " + Global::intToString(NNPos::MAX_BOARD_LEN));
  if(maxConcurrentEvals <= 0)
    throw StringError("maxConcurrentEvals is negative: " + Global::intToString(maxConcurrentEvals));
  if(maxBatchSize <= 0)
    throw StringError("maxBatchSize is negative: " + Global::intToString(maxBatchSize));
  if(gpuIdxByServerThread.size() != numThreads)
    throw StringError("gpuIdxByServerThread.size() != numThreads");

  if(logger != NULL) {
    logger->write(
      "Initializing neural net buffer to be size " +
      Global::intToString(nnXLen) + " * " + Global::intToString(nnYLen) +
      (requireExactNNLen ? " exactly" : " allowing smaller boards")
    );
  }

  //Add three, just to give a bit of extra headroom, and make it a power of two
  numResultBufss = maxConcurrentEvals / maxBatchSize + 3;
  {
    int x = 1;
    while (x < numResultBufss)
      x *= 2;
    numResultBufss = x;
  }
  numResultBufssMask = numResultBufss - 1;

  if(nnCacheSizePowerOfTwo >= 0)
    nnCacheTable = new NNCacheTable(nnCacheSizePowerOfTwo, nnMutexPoolSizePowerofTwo);

  if(!debugSkipNeuralNet) {
    vector<int> gpuIdxs = gpuIdxByServerThread;
    std::sort(gpuIdxs.begin(), gpuIdxs.end());
    std::unique(gpuIdxs.begin(), gpuIdxs.end());
    loadedModel = NeuralNet::loadModelFile(modelFileName,expectedSha256);
    modelVersion = NeuralNet::getModelVersion(loadedModel);
    inputsVersion = NNModelVersion::getInputsVersion(modelVersion);
    computeContext = NeuralNet::createComputeContext(
      gpuIdxs,logger,nnXLen,nnYLen,
      openCLTunerFile,homeDataDirOverride,openCLReTunePerBoardSize,
      usingFP16Mode,usingNHWCMode,loadedModel
    );
  }
  else {
    modelVersion = NNModelVersion::defaultModelVersion;
    inputsVersion = NNModelVersion::getInputsVersion(modelVersion);
  }

  m_resultBufss = new NNResultBuf**[numResultBufss];
  for(int i = 0; i < numResultBufss; i++) {
    m_resultBufss[i] = new NNResultBuf*[maxBatchSize];
    for(int j = 0; j < maxBatchSize; j++)
      m_resultBufss[i][j] = NULL;
  }
}

NNEvaluator::~NNEvaluator() {
  killServerThreads();

  for(int i = 0; i < numResultBufss; i++) {
    NNResultBuf** resultBufs = m_resultBufss[i];
    //Pointers inside here don't need to be deleted, they simply point to the clients waiting for results
    delete[] resultBufs;
    m_resultBufss[i] = NULL;
  }
  delete[] m_resultBufss;
  m_resultBufss = NULL;

  if(computeContext != NULL)
    NeuralNet::freeComputeContext(computeContext);
  computeContext = NULL;

  if(loadedModel != NULL)
    NeuralNet::freeLoadedModel(loadedModel);
  loadedModel = NULL;

  delete nnCacheTable;
}

string NNEvaluator::getModelName() const {
  return modelName;
}
string NNEvaluator::getModelFileName() const {
  return modelFileName;
}
string NNEvaluator::getInternalModelName() const {
  if(loadedModel == NULL)
    return "random";
  else
    return NeuralNet::getModelName(loadedModel);
}
Logger* NNEvaluator::getLogger() {
  return logger;
}
bool NNEvaluator::isNeuralNetLess() const {
  return debugSkipNeuralNet;
}
int NNEvaluator::getMaxBatchSize() const {
  return maxNumRows;
}
int NNEvaluator::getNumGpus() const {
#ifdef USE_EIGEN_BACKEND
  return 1;
#else
  std::set<int> gpuIdxs;
  for(int i = 0; i<gpuIdxByServerThread.size(); i++) {
    gpuIdxs.insert(gpuIdxByServerThread[i]);
  }
  return (int)gpuIdxs.size();
#endif
}
int NNEvaluator::getNumServerThreads() const {
  return (int)gpuIdxByServerThread.size();
}
std::set<int> NNEvaluator::getGpuIdxs() const {
  std::set<int> gpuIdxs;
#ifdef USE_EIGEN_BACKEND
  gpuIdxs.insert(0);
#else
  for(int i = 0; i<gpuIdxByServerThread.size(); i++) {
    gpuIdxs.insert(gpuIdxByServerThread[i]);
  }
#endif
  return gpuIdxs;
}

int NNEvaluator::getNNXLen() const {
  return nnXLen;
}
int NNEvaluator::getNNYLen() const {
  return nnYLen;
}
enabled_t NNEvaluator::getUsingFP16Mode() const {
  return usingFP16Mode;
}
enabled_t NNEvaluator::getUsingNHWCMode() const {
  return usingNHWCMode;
}

bool NNEvaluator::supportsShorttermError() const {
  return modelVersion >= 9;
}

bool NNEvaluator::getDoRandomize() const {
  lock_guard<std::mutex> lock(bufferMutex);
  return currentDoRandomize;
}
int NNEvaluator::getDefaultSymmetry() const {
  lock_guard<std::mutex> lock(bufferMutex);
  return currentDefaultSymmetry;
}
void NNEvaluator::setDoRandomize(bool b) {
  lock_guard<std::mutex> lock(bufferMutex);
  currentDoRandomize = b;
}
void NNEvaluator::setDefaultSymmetry(int s) {
  lock_guard<std::mutex> lock(bufferMutex);
  currentDefaultSymmetry = s;
}

Rules NNEvaluator::getSupportedRules(const Rules& desiredRules, bool& supported) {
  if(loadedModel == NULL) {
    supported = true;
    return desiredRules;
  }
  return NeuralNet::getSupportedRules(loadedModel, desiredRules, supported);
}

uint64_t NNEvaluator::numRowsProcessed() const {
  return m_numRowsProcessed.load(std::memory_order_relaxed);
}
uint64_t NNEvaluator::numBatchesProcessed() const {
  return m_numBatchesProcessed.load(std::memory_order_relaxed);
}
double NNEvaluator::averageProcessedBatchSize() const {
  return (double)numRowsProcessed() / (double)numBatchesProcessed();
}

void NNEvaluator::clearStats() {
  m_numRowsProcessed.store(0);
  m_numBatchesProcessed.store(0);
}

void NNEvaluator::clearCache() {
  if(nnCacheTable != NULL)
    nnCacheTable->clear();
}


bool NNEvaluator::isAnyThreadUsingFP16() const {
  lock_guard<std::mutex> lock(bufferMutex);
  for(const int& isUsingFP16: serverThreadsIsUsingFP16) {
    if(isUsingFP16)
      return true;
  }
  return false;
}

static void serveEvals(
  string randSeedThisThread,
  NNEvaluator* nnEval, const LoadedModel* loadedModel,
  int gpuIdxForThisThread,
  int serverThreadIdx
) {
  NNServerBuf* buf = new NNServerBuf(*nnEval,loadedModel);
  Rand rand(randSeedThisThread);

  //Used to have a try catch around this but actually we're in big trouble if this raises an exception
  //and causes possibly the only nnEval thread to die, so actually go ahead and let the exception escape to
  //toplevel for easier debugging
  nnEval->serve(*buf,rand,gpuIdxForThisThread,serverThreadIdx);
  delete buf;
}

void NNEvaluator::setNumThreads(const vector<int>& gpuIdxByServerThr) {
  if(serverThreads.size() != 0)
    throw StringError("NNEvaluator::setNumThreads called when threads were already running!");
  numThreads = (int)gpuIdxByServerThr.size();
  gpuIdxByServerThread = gpuIdxByServerThr;
}

void NNEvaluator::spawnServerThreads() {
  if(serverThreads.size() != 0)
    throw StringError("NNEvaluator::spawnServerThreads called when threads were already running!");

  {
    lock_guard<std::mutex> lock(bufferMutex);
    serverThreadsIsUsingFP16.resize(numThreads,0);
  }

  numServerThreadsStartingUp = numThreads;
  for(int i = 0; i<numThreads; i++) {
    int gpuIdxForThisThread = gpuIdxByServerThread[i];
    string randSeedThisThread = randSeed + ":NNEvalServerThread:" + Global::intToString(numServerThreadsEverSpawned);
    numServerThreadsEverSpawned++;
    std::thread* thread = new std::thread(
      &serveEvals,randSeedThisThread,this,loadedModel,gpuIdxForThisThread,i
    );
    serverThreads.push_back(thread);
  }

  unique_lock<std::mutex> lock(bufferMutex);
  while(numServerThreadsStartingUp > 0)
    mainThreadWaitingForSpawn.wait(lock);
}

void NNEvaluator::killServerThreads() {
  unique_lock<std::mutex> lock(bufferMutex);
  isKilled = true;
  lock.unlock();
  serverWaitingForBatchStart.notify_all();
  waitingForFinish.notify_all();

  for(size_t i = 0; i<serverThreads.size(); i++)
    serverThreads[i]->join();
  for(size_t i = 0; i<serverThreads.size(); i++)
    delete serverThreads[i];
  serverThreads.clear();
  serverThreadsIsUsingFP16.clear();

  //Can unset now that threads are dead
  isKilled = false;

  assert(numOngoingEvals == 0);
  assert(numWaitingEvals == 0);
  assert(numEvalsToAwaken == 0);
}

void NNEvaluator::serve(
  NNServerBuf& buf, Rand& rand,
  int gpuIdxForThisThread,
  int serverThreadIdx
) {
  int64_t numBatchesHandledThisThread = 0;
  int64_t numRowsHandledThisThread = 0;

  ComputeHandle* gpuHandle = NULL;
  if(loadedModel != NULL)
    gpuHandle = NeuralNet::createComputeHandle(
      computeContext,
      loadedModel,
      logger,
      maxNumRows,
      requireExactNNLen,
      inputsUseNHWC,
      gpuIdxForThisThread,
      serverThreadIdx
    );

  {
    lock_guard<std::mutex> lock(bufferMutex);
    assert(serverThreadIdx < serverThreadsIsUsingFP16.size());
    serverThreadsIsUsingFP16[serverThreadIdx] = gpuHandle == NULL ? 0 : NeuralNet::isUsingFP16(gpuHandle) ? 1 : 0;
    numServerThreadsStartingUp--;
    if(numServerThreadsStartingUp <= 0)
      mainThreadWaitingForSpawn.notify_all();
  }

  vector<NNOutput*> outputBuf;

  unique_lock<std::mutex> lock(bufferMutex);
  while(true) {
    while(m_currentResultBufsLen <= 0 && m_currentResultBufsIdx == m_oldestResultBufsIdx && !isKilled)
      serverWaitingForBatchStart.wait(lock);

    if(isKilled)
      break;

    std::swap(m_resultBufss[m_oldestResultBufsIdx],buf.resultBufs);

    int numRows;
    //We grabbed everything in the latest buffer, so clients should move on to an entirely new buffer
    if(m_currentResultBufsIdx == m_oldestResultBufsIdx) {
      m_oldestResultBufsIdx = (m_oldestResultBufsIdx + 1) & numResultBufssMask;
      m_currentResultBufsIdx = m_oldestResultBufsIdx;
      numRows = m_currentResultBufsLen;
      m_currentResultBufsLen = 0;
    }
    //We grabbed a buffer that clients have already entirely moved onward from.
    else {
      m_oldestResultBufsIdx = (m_oldestResultBufsIdx + 1) & numResultBufssMask;
      numRows = maxNumRows;
    }

    numOngoingEvals += 1;
    bool doRandomize = currentDoRandomize;
    int defaultSymmetry = currentDefaultSymmetry;
    lock.unlock();

    if(debugSkipNeuralNet) {
      for(int row = 0; row < numRows; row++) {
        assert(buf.resultBufs[row] != NULL);
        NNResultBuf* resultBuf = buf.resultBufs[row];
        buf.resultBufs[row] = NULL;

        int boardXSize = resultBuf->boardXSizeForServer;
        int boardYSize = resultBuf->boardYSizeForServer;

        unique_lock<std::mutex> resultLock(resultBuf->resultMutex);
        assert(resultBuf->hasResult == false);
        resultBuf->result = std::make_shared<NNOutput>();

        float* policyProbs = resultBuf->result->policyProbs;
        for(int i = 0; i<NNPos::MAX_NN_POLICY_SIZE; i++)
          policyProbs[i] = 0;

        //At this point, these aren't probabilities, since this is before the postprocessing
        //that happens for each result. These just need to be unnormalized log probabilities.
        //Illegal move filtering happens later.
        for(int y = 0; y<boardYSize; y++) {
          for(int x = 0; x<boardXSize; x++) {
            int pos = NNPos::xyToPos(x,y,nnXLen);
            policyProbs[pos] = (float)rand.nextGaussian();
          }
        }
        policyProbs[NNPos::locToPos(Board::PASS_LOC,boardXSize,nnXLen,nnYLen)] = (float)rand.nextGaussian();

        resultBuf->result->nnXLen = nnXLen;
        resultBuf->result->nnYLen = nnYLen;
     

        //These aren't really probabilities. Win/Loss/NoResult will get softmaxed later
        double whiteWinProb = 0.0 + rand.nextGaussian() * 0.20;
        double whiteLossProb = 0.0 + rand.nextGaussian() * 0.20;
        double whiteNoResultProb = 0.0 + rand.nextGaussian() * 0.20;
        double varTimeLeft = 0.5 * boardXSize * boardYSize;
        resultBuf->result->whiteWinProb = (float)whiteWinProb;
        resultBuf->result->whiteLossProb = (float)whiteLossProb;
        resultBuf->result->whiteNoResultProb = (float)whiteNoResultProb;
        resultBuf->result->varTimeLeft = (float)varTimeLeft;
        resultBuf->result->shorttermWinlossError = 0.0f;
        resultBuf->hasResult = true;
        resultBuf->clientWaitingForResult.notify_all();
        resultLock.unlock();
      }
    }
    else {
      outputBuf.clear();
      for(int row = 0; row<numRows; row++) {
        NNOutput* emptyOutput = new NNOutput();
        assert(buf.resultBufs[row] != NULL);
        emptyOutput->nnXLen = nnXLen;
        emptyOutput->nnYLen = nnYLen;
        outputBuf.push_back(emptyOutput);
      }

      for(int row = 0; row<numRows; row++) {
        if(buf.resultBufs[row]->symmetry == NNInputs::SYMMETRY_NOTSPECIFIED) {
          if(doRandomize)
            buf.resultBufs[row]->symmetry = rand.nextUInt(SymmetryHelpers::NUM_SYMMETRIES);
          else {
            assert(defaultSymmetry >= 0 && defaultSymmetry <= SymmetryHelpers::NUM_SYMMETRIES-1);
            buf.resultBufs[row]->symmetry = defaultSymmetry;
          }
        }
      }

      NeuralNet::getOutput(gpuHandle, buf.inputBuffers, numRows, buf.resultBufs, outputBuf);
      assert(outputBuf.size() == numRows);

      m_numRowsProcessed.fetch_add(numRows, std::memory_order_relaxed);
      m_numBatchesProcessed.fetch_add(1, std::memory_order_relaxed);
      numRowsHandledThisThread += numRows;
      numBatchesHandledThisThread += 1;

      for(int row = 0; row < numRows; row++) {
        assert(buf.resultBufs[row] != NULL);
        NNResultBuf* resultBuf = buf.resultBufs[row];
        buf.resultBufs[row] = NULL;

        unique_lock<std::mutex> resultLock(resultBuf->resultMutex);
        assert(resultBuf->hasResult == false);
        resultBuf->result = std::shared_ptr<NNOutput>(outputBuf[row]);
        resultBuf->hasResult = true;
        resultBuf->clientWaitingForResult.notify_all();
        resultLock.unlock();
      }
    }

    //Lock and update stats before looping again
    lock.lock();
    numOngoingEvals -= 1;

    if(numWaitingEvals > 0) {
      numEvalsToAwaken += numWaitingEvals;
      numWaitingEvals = 0;
      waitingForFinish.notify_all();
    }
    continue;
  }

  NeuralNet::freeComputeHandle(gpuHandle);
  if(logger != NULL) {
    logger->write(
      "GPU " + Global::intToString(gpuIdxForThisThread) + " finishing, processed " +
      Global::int64ToString(numRowsHandledThisThread) + " rows " +
      Global::int64ToString(numBatchesHandledThisThread) + " batches"
    );
  }
}

void NNEvaluator::waitForNextNNEvalIfAny() {
  unique_lock<std::mutex> lock(bufferMutex);
  if(numOngoingEvals <= 0)
    return;

  numWaitingEvals++;
  while(numEvalsToAwaken <= 0 && !isKilled)
    waitingForFinish.wait(lock);
  numEvalsToAwaken--;
}


static double softPlus(double x) {
  //Avoid blowup
  if(x > 40.0)
    return x;
  else
    return log(1.0 + exp(x));
}


void NNEvaluator::evaluate(
  Board& board,
  const BoardHistory& history,
  Player nextPlayer,
  const MiscNNInputParams& nnInputParams,
  NNResultBuf& buf,
  bool skipCache
) {
  assert(!isKilled);
  buf.hasResult = false;
 

  if(board.x_size > nnXLen || board.y_size > nnYLen)
    throw StringError("NNEvaluator was configured with nnXLen = " + Global::intToString(nnXLen) +
                      " nnYLen = " + Global::intToString(nnYLen) +
                      " but was asked to evaluate board with larger x or y size");
  if(requireExactNNLen) {
    if(board.x_size != nnXLen || board.y_size != nnYLen)
      throw StringError("NNEvaluator was configured with nnXLen = " + Global::intToString(nnXLen) +
                        " nnYLen = " + Global::intToString(nnYLen) +
                        " and requireExactNNLen, but was asked to evaluate board with different x or y size");
  }

  Hash128 nnHash = NNInputs::getHash(board, history, nextPlayer, nnInputParams);

  if(nnCacheTable != NULL && !skipCache && nnCacheTable->get(nnHash,buf.result)) {
    
    buf.hasResult = true;
    return;
  }

  buf.boardXSizeForServer = board.x_size;
  buf.boardYSizeForServer = board.y_size;

  MiscNNInputParams nnInputParamsWithResultsBeforeNN = nnInputParams;
  nnInputParamsWithResultsBeforeNN.resultsBeforeNN.init(board, history, nextPlayer);

  if(!debugSkipNeuralNet) {
    int rowSpatialLen = NNModelVersion::getNumSpatialFeatures(modelVersion) * nnXLen * nnYLen;
    if(buf.rowSpatial == NULL) {
      buf.rowSpatial = new float[rowSpatialLen];
      buf.rowSpatialSize = rowSpatialLen;
    }
    else {
      if(buf.rowSpatialSize != rowSpatialLen)
        throw StringError("Cannot reuse an nnResultBuf with different dimensions or model version");
    }
    int rowGlobalLen = NNModelVersion::getNumGlobalFeatures(modelVersion);
    if(buf.rowGlobal == NULL) {
      buf.rowGlobal = new float[rowGlobalLen];
      buf.rowGlobalSize = rowGlobalLen;
    }
    else {
      if(buf.rowGlobalSize != rowGlobalLen)
        throw StringError("Cannot reuse an nnResultBuf with different dimensions or model version");
    }


    static_assert(NNModelVersion::latestInputsVersionImplemented == 7, "");
    if(inputsVersion == 7)
      NNInputs::fillRowV7(board, history, nextPlayer, nnInputParamsWithResultsBeforeNN, nnXLen, nnYLen, inputsUseNHWC, buf.rowSpatial, buf.rowGlobal);
    else
      ASSERT_UNREACHABLE;
  }

  buf.symmetry = nnInputParams.symmetry;

  unique_lock<std::mutex> lock(bufferMutex);

  m_resultBufss[m_currentResultBufsIdx][m_currentResultBufsLen] = &buf;
  m_currentResultBufsLen += 1;
  if(m_currentResultBufsLen == 1 && m_currentResultBufsIdx == m_oldestResultBufsIdx)
    serverWaitingForBatchStart.notify_one();

  bool overlooped = false;
  if(m_currentResultBufsLen >= maxNumRows) {
    m_currentResultBufsLen = 0;
    m_currentResultBufsIdx = (m_currentResultBufsIdx + 1) & numResultBufssMask;
    overlooped = m_currentResultBufsIdx == m_oldestResultBufsIdx;
  }
  lock.unlock();

  //This should only fire if we have more than maxConcurrentEvals evaluating, such that they wrap the
  //circular buffer.
  assert(!overlooped);
  (void)overlooped; //Avoid unused variable when asserts disabled

  unique_lock<std::mutex> resultLock(buf.resultMutex);
  while(!buf.hasResult)
    buf.clientWaitingForResult.wait(resultLock);
  resultLock.unlock();

  //Perform postprocessing on the result - turn the nn output into probabilities
  //As a hack though, if the only thing we were missing was the ownermap, just grab the old policy and values
  //and use those. This avoids recomputing in a randomly different orientation when we just need the ownermap
  //and causing policy weights to be different, which would reduce performance of successive searches in a game
  //by making the successive searches distribute their playouts less coherently and using the cache more poorly.
  
    float* policy = buf.result->policyProbs;

    float nnPolicyInvTemperature = 1.0f / nnInputParams.nnPolicyTemperature;

    int xSize = board.x_size;
    int ySize = board.y_size;

    float maxPolicy = -1e25f;
    bool isLegal[NNPos::MAX_NN_POLICY_SIZE];
    int legalCount = 0;

    GameLogic::ResultsBeforeNN resultsBeforeNN = nnInputParamsWithResultsBeforeNN.resultsBeforeNN;
    if(resultsBeforeNN.myOnlyLoc == Board::NULL_LOC) {
      for(int i = 0; i < policySize; i++) {
        Loc loc = NNPos::posToLoc(i, xSize, ySize, nnXLen, nnYLen);
        isLegal[i] = history.isLegal(board, loc, nextPlayer);
      }
    } 
    else  // assume all other moves are illegal
    {
      for(int i = 0; i < policySize; i++) {
        isLegal[i] = false;
      }
      isLegal[NNPos::locToPos(resultsBeforeNN.myOnlyLoc, xSize, nnXLen, nnYLen)] = true;
      isLegal[NNPos::locToPos(Board::PASS_LOC, xSize, nnXLen, nnYLen)] = true;
    }

    for(int i = 0; i<policySize; i++) {
      float policyValue;
      if(isLegal[i]) {
        legalCount += 1;
        policyValue = policy[i] * nnPolicyInvTemperature;
      }
      else
        policyValue = -1e30f;

      policy[i] = policyValue;
      if(policyValue > maxPolicy)
        maxPolicy = policyValue;
    }

    assert(legalCount > 0);

    float policySum = 0.0f;
    for(int i = 0; i<policySize; i++) {
      policy[i] = exp(policy[i] - maxPolicy);
      policySum += policy[i];
    }

    if(!isfinite(policySum)) {
      cout << "Got nonfinite for policy sum" << endl;
      history.printDebugInfo(cout,board);
      throw StringError("Got nonfinite for policy sum");
    }

    //Somehow all legal moves rounded to 0 probability
    if(policySum <= 0.0) {
      if(!buf.errorLogLockout && logger != NULL) {
        buf.errorLogLockout = true;
        logger->write("Warning: all legal moves rounded to 0 probability for " + string(modelFileName));
      }
      float uniform = 1.0f / legalCount;
      for(int i = 0; i<policySize; i++) {
        policy[i] = isLegal[i] ? uniform : -1.0f;
      }
    }
    //Normal case
    else {
      for(int i = 0; i<policySize; i++)
        policy[i] = isLegal[i] ? (policy[i] / policySum) : -1.0f;
    }

    //Fill everything out-of-bounds too, for robustness.
    for(int i = policySize; i<NNPos::MAX_NN_POLICY_SIZE; i++)
      policy[i] = -1.0f;

    //Fix up the value as well. Note that the neural net gives us back the value from the perspective
    //of the player so we need to negate that to make it the white value.
    static_assert(NNModelVersion::latestModelVersionImplemented == 11, "");
    if(modelVersion >= 4 && modelVersion <= 11) {
      double winProb;
      double lossProb;
      double noResultProb;
      double varTimeLeft;
      double shorttermWinlossError;
      {
        double winLogits = buf.result->whiteWinProb;
        double lossLogits = buf.result->whiteLossProb;
        double noResultLogits = buf.result->whiteNoResultProb;
        double varTimeLeftPreSoftplus = buf.result->varTimeLeft;
        double shorttermWinlossErrorPreSoftplus = buf.result->shorttermWinlossError;

        
        if(resultsBeforeNN.winner == C_EMPTY) {  // draw
          winProb = 0.5;
          lossProb = 0.5;
          noResultProb = 0.0;
        } 
        else if(resultsBeforeNN.winner == nextPlayer) {  // next player win
          winProb = 1.0;
          lossProb = 0.0;
          noResultProb = 0.0;
        } 
        else if(resultsBeforeNN.winner == getOpp(nextPlayer)) {  // opp win
          winProb = 0.0;
          lossProb = 1.0;
          noResultProb = 0.0;
        } 
        else { //no sure results
          // Softmax
          double maxLogits = std::max(std::max(winLogits, lossLogits), noResultLogits);
          winProb = exp(winLogits - maxLogits);
          lossProb = exp(lossLogits - maxLogits);
          noResultProb = exp(noResultLogits - maxLogits);

        } 
       
        double probSum = winProb + lossProb + noResultProb;
        winProb /= probSum;
        lossProb /= probSum;
        noResultProb /= probSum;



        varTimeLeft = softPlus(varTimeLeftPreSoftplus) * 40.0;


        if(modelVersion >= 10) {
          shorttermWinlossError = sqrt(softPlus(shorttermWinlossErrorPreSoftplus) * 0.25);
        }
        else {
          shorttermWinlossError = softPlus(shorttermWinlossErrorPreSoftplus);
        }

        if(
          !isfinite(probSum) ||
          !isfinite(varTimeLeft) ||
          !isfinite(shorttermWinlossError) 
        ) {
          cout << "Got nonfinite for nneval value" << endl;
          cout << winLogits << " " << lossLogits << " " << noResultLogits
               << " " << varTimeLeft
               << " " << shorttermWinlossError 
               << endl;
          throw StringError("Got nonfinite for nneval value");
        }
      }

      if(nextPlayer == P_WHITE) {
        buf.result->whiteWinProb = (float)winProb;
        buf.result->whiteLossProb = (float)lossProb;
        buf.result->whiteNoResultProb = (float)noResultProb;
      }
      else {
        buf.result->whiteWinProb = (float)lossProb;
        buf.result->whiteLossProb = (float)winProb;
        buf.result->whiteNoResultProb = (float)noResultProb;
      }

      if(modelVersion >= 9) {
        buf.result->varTimeLeft = (float)varTimeLeft;
        buf.result->shorttermWinlossError = (float)shorttermWinlossError;
      }
      else {
        buf.result->varTimeLeft = -1;
        buf.result->shorttermWinlossError = -1;
      }
    }
    else {
      throw StringError("NNEval value postprocessing not implemented for model version");
    }
  



  //And record the nnHash in the result and put it into the table
  buf.result->nnHash = nnHash;
  if(nnCacheTable != NULL)
    nnCacheTable->set(buf.result);

}

//Uncomment this to lower the effective hash size down to one where we get true collisions
//#define SIMULATE_TRUE_HASH_COLLISIONS

NNCacheTable::Entry::Entry()
  :ptr(nullptr)
{}
NNCacheTable::Entry::~Entry()
{}

NNCacheTable::NNCacheTable(int sizePowerOfTwo, int mutexPoolSizePowerOfTwo) {
  if(sizePowerOfTwo < 0 || sizePowerOfTwo > 63)
    throw StringError("NNCacheTable: Invalid sizePowerOfTwo: " + Global::intToString(sizePowerOfTwo));
  if(mutexPoolSizePowerOfTwo < 0 || mutexPoolSizePowerOfTwo > 31)
    throw StringError("NNCacheTable: Invalid mutexPoolSizePowerOfTwo: " + Global::intToString(mutexPoolSizePowerOfTwo));
#if defined(SIMULATE_TRUE_HASH_COLLISIONS)
  sizePowerOfTwo = sizePowerOfTwo > 12 ? 12 : sizePowerOfTwo;
#endif
  if(mutexPoolSizePowerOfTwo > sizePowerOfTwo)
    mutexPoolSizePowerOfTwo = sizePowerOfTwo;

  tableSize = ((uint64_t)1) << sizePowerOfTwo;
  tableMask = tableSize-1;
  entries = new Entry[tableSize];
  uint32_t mutexPoolSize = ((uint32_t)1) << mutexPoolSizePowerOfTwo;
  mutexPoolMask = mutexPoolSize-1;
  mutexPool = new MutexPool(mutexPoolSize);
}
NNCacheTable::~NNCacheTable() {
  delete[] entries;
  delete mutexPool;
}

bool NNCacheTable::get(Hash128 nnHash, shared_ptr<NNOutput>& ret) {
  //Free ret BEFORE locking, to avoid any expensive operations while locked.
  if(ret != nullptr)
    ret.reset();

  uint64_t idx = nnHash.hash0 & tableMask;
  uint32_t mutexIdx = (uint32_t)idx & mutexPoolMask;
  Entry& entry = entries[idx];
  std::mutex& mutex = mutexPool->getMutex(mutexIdx);

  std::lock_guard<std::mutex> lock(mutex);

  bool found = false;
#if defined(SIMULATE_TRUE_HASH_COLLISIONS)
  if(entry.ptr != nullptr && ((entry.ptr->nnHash.hash0 ^ nnHash.hash0) & 0xFFF) == 0) {
    ret = entry.ptr;
    found = true;
  }
#else
  if(entry.ptr != nullptr && entry.ptr->nnHash == nnHash) {
    ret = entry.ptr;
    found = true;
  }
#endif
  return found;
}

void NNCacheTable::set(const shared_ptr<NNOutput>& p) {
  //Immediately copy p right now, before locking, to avoid any expensive operations while locked.
  shared_ptr<NNOutput> buf(p);

  uint64_t idx = p->nnHash.hash0 & tableMask;
  uint32_t mutexIdx = (uint32_t)idx & mutexPoolMask;
  Entry& entry = entries[idx];
  std::mutex& mutex = mutexPool->getMutex(mutexIdx);

  {
    std::lock_guard<std::mutex> lock(mutex);
    //Perform a swap, to avoid any expensive free under the mutex.
    entry.ptr.swap(buf);
  }

  //No longer locked, allow buf to fall out of scope now, will free whatever used to be present in the table.
}

void NNCacheTable::clear() {
  shared_ptr<NNOutput> buf;
  for(size_t idx = 0; idx<tableSize; idx++) {
    Entry& entry = entries[idx];
    uint32_t mutexIdx = (uint32_t)idx & mutexPoolMask;
    std::mutex& mutex = mutexPool->getMutex(mutexIdx);
    {
      std::lock_guard<std::mutex> lock(mutex);
      entry.ptr.swap(buf);
    }
    buf.reset();
  }
}
