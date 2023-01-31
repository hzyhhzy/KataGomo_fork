#include "../core/global.h"
#include "../core/config_parser.h"
#include "../core/timer.h"
#include "../dataio/sgf.h"
#include "../search/asyncbot.h"
#include "../search/searchnode.h"
#include "../program/setup.h"
#include "../program/playutils.h"
#include "../program/play.h"
#include "../command/commandline.h"
#include "../main.h"

using namespace std;

int MainCmds::evalsgf(const vector<string>& args) {
  Board::initHash();
  Rand seedRand;

  ConfigParser cfg;
  string modelFile;
  string sgfFile;
  int moveNum;
  string printBranch;
  string extraMoves;
  string avoidMoves;
  string hintLoc;
  int64_t maxVisits;
  int numThreads;
  bool printRootNNValues;
  bool printPolicy;
  bool printLogPolicy;
  bool printDirichletShape;
  bool printAvgShorttermError;
  bool printGraph;
  int printMaxDepth;
  bool rawNN;
  try {
    KataGoCommandLine cmd("Run a search on a position from an sgf file, for debugging.");
    cmd.addConfigFileArg("","gtp_example.cfg");
    cmd.addModelFileArg();

    TCLAP::UnlabeledValueArg<string> sgfFileArg("","Sgf file to analyze",true,string(),"FILE");
    TCLAP::ValueArg<int> moveNumArg("m","move-num","Sgf move num to analyze, 1-indexed",true,0,"MOVENUM");

    TCLAP::ValueArg<string> printBranchArg("","print-branch","Move branch in search tree to print",false,string(),"MOVE MOVE ...");
    TCLAP::ValueArg<string> printArg("p","print","Alias for -print-branch",false,string(),"MOVE MOVE ...");
    TCLAP::ValueArg<string> extraMovesArg("","extra-moves","Extra moves to force-play before doing search",false,string(),"MOVE MOVE ...");
    TCLAP::ValueArg<string> extraArg("e","extra","Alias for -extra-moves",false,string(),"MOVE MOVE ...");
    TCLAP::ValueArg<string> avoidMovesArg("","avoid-moves","Avoid moves in search",false,string(),"MOVE MOVE ...");
    TCLAP::ValueArg<string> hintLocArg("","hint-loc","Hint loc",false,string(),"MOVE");
    TCLAP::ValueArg<long> visitsArg("v","visits","Set the number of visits",false,-1,"VISITS");
    TCLAP::ValueArg<int> threadsArg("t","threads","Set the number of threads",false,-1,"THREADS");
    TCLAP::SwitchArg printRootNNValuesArg("","print-root-nn-values","Print root nn values");
    TCLAP::SwitchArg printPolicyArg("","print-policy","Print policy");
    TCLAP::SwitchArg printLogPolicyArg("","print-log-policy","Print log policy");
    TCLAP::SwitchArg printDirichletShapeArg("","print-dirichlet-shape","Print dirichlet shape");
    TCLAP::SwitchArg printRootEndingBonusArg("","print-root-ending-bonus","Print root ending bonus now");
    TCLAP::SwitchArg printAvgShorttermErrorArg("","print-avg-shortterm-error","Compute and print avgShorttermError");
    TCLAP::SwitchArg printGraphArg("","print-graph","Print graph structure of the search");
    TCLAP::ValueArg<int> printMaxDepthArg("","print-max-depth","How deep to print",false,1,"DEPTH");
    TCLAP::SwitchArg rawNNArg("","raw-nn","Perform single raw neural net eval");
    cmd.add(sgfFileArg);
    cmd.add(moveNumArg);

    cmd.setShortUsageArgLimit();

    cmd.addOverrideConfigArg();

    cmd.add(printBranchArg);
    cmd.add(printArg);
    cmd.add(extraMovesArg);
    cmd.add(extraArg);
    cmd.add(avoidMovesArg);
    cmd.add(hintLocArg);
    cmd.add(visitsArg);
    cmd.add(threadsArg);
    cmd.add(printRootNNValuesArg);
    cmd.add(printPolicyArg);
    cmd.add(printLogPolicyArg);
    cmd.add(printDirichletShapeArg);
    cmd.add(printRootEndingBonusArg);
    cmd.add(printAvgShorttermErrorArg);
    cmd.add(printGraphArg);
    cmd.add(printMaxDepthArg);
    cmd.add(rawNNArg);
    cmd.parseArgs(args);

    modelFile = cmd.getModelFile();
    sgfFile = sgfFileArg.getValue();
    moveNum = moveNumArg.getValue();
    printBranch = printBranchArg.getValue();
    string print = printArg.getValue();
    extraMoves = extraMovesArg.getValue();
    string extra = extraArg.getValue();
    avoidMoves = avoidMovesArg.getValue();
    hintLoc = hintLocArg.getValue();
    maxVisits = (int64_t)visitsArg.getValue();
    numThreads = threadsArg.getValue();
    printRootNNValues = printRootNNValuesArg.getValue();
    printPolicy = printPolicyArg.getValue();
    printLogPolicy = printLogPolicyArg.getValue();
    printDirichletShape = printDirichletShapeArg.getValue();
    printAvgShorttermError = printAvgShorttermErrorArg.getValue();
    printGraph = printGraphArg.getValue();
    printMaxDepth = printMaxDepthArg.getValue();
    rawNN = rawNNArg.getValue();

    if(printBranch.length() > 0 && print.length() > 0) {
      cerr << "Error: -print-branch and -print both specified" << endl;
      return 1;
    }
    if(printBranch.length() <= 0)
      printBranch = print;

    if(extraMoves.length() > 0 && extra.length() > 0) {
      cerr << "Error: -extra-moves and -extra both specified" << endl;
      return 1;
    }
    if(extraMoves.length() <= 0)
      extraMoves = extra;

    cmd.getConfig(cfg);
  }
  catch (TCLAP::ArgException &e) {
    cerr << "Error: " << e.error() << " for argument " << e.argId() << endl;
    return 1;
  }

  //Parse rules -------------------------------------------------------------------
  Rules defaultRules = Rules::getTrompTaylorish();
  Player perspective = Setup::parseReportAnalysisWinrates(cfg,P_BLACK);

  //Parse sgf file and board ------------------------------------------------------------------

  CompactSgf* sgf = CompactSgf::loadFile(sgfFile);

  Board board;
  Player nextPla;
  BoardHistory hist;

  auto setUpBoardUsingRules = [&board,&nextPla,&hist,moveNum,&sgf,&extraMoves](const Rules& initialRules) {
    sgf->setupInitialBoardAndHist(initialRules, board, nextPla, hist);
    vector<Move>& moves = sgf->moves;


    if(moveNum < 0)
      throw StringError("Move num " + Global::intToString(moveNum) + " requested but must be non-negative");
    if(moveNum > moves.size())
      throw StringError("Move num " + Global::intToString(moveNum) + " requested but sgf has only " + Global::int64ToString(moves.size()));

    sgf->playMovesTolerant(board,nextPla,hist,moveNum);

    vector<Loc> extraMoveLocs = Location::parseSequence(extraMoves,board);
    for(size_t i = 0; i<extraMoveLocs.size(); i++) {
      Loc loc = extraMoveLocs[i];
      if(!hist.isLegal(board,loc,nextPla)) {
        cerr << board << endl;
        cerr << "Extra illegal move for " << PlayerIO::colorToChar(nextPla) << ": " << Location::toString(loc,board) << endl;
        throw StringError("Illegal extra move");
      }
      hist.makeBoardMoveAssumeLegal(board,loc,nextPla);
      nextPla = getOpp(nextPla);
    }
  };

  Rules initialRules = sgf->getRulesOrWarn(
    defaultRules,
    [](const string& msg) { cout << msg << endl; }
  );
  setUpBoardUsingRules(initialRules);

  //Parse move sequence arguments------------------------------------------

  PrintTreeOptions options;
  options = options.maxDepth(printMaxDepth);
  if(printBranch.length() > 0)
    options = options.onlyBranch(board,printBranch);
  options = options.printAvgShorttermError(printAvgShorttermError);

  //Load neural net and start bot------------------------------------------

  const bool logToStdoutDefault = true;
  Logger logger(&cfg, logToStdoutDefault);
  logger.write("Engine starting...");

  SearchParams params = Setup::loadSingleParams(cfg,Setup::SETUP_FOR_GTP);
  if(maxVisits < -1 || maxVisits == 0)
    throw StringError("maxVisits: invalid value");
  else if(maxVisits == -1)
    logger.write("No max visits specified on cmdline, using defaults in " + cfg.getFileName());
  else {
    params.maxVisits = maxVisits;
    params.maxPlayouts = maxVisits; //Also set this so it doesn't cap us either
  }
  if(numThreads < -1 || numThreads == 0)
    throw StringError("numThreads: invalid value");
  else if(numThreads == -1)
    logger.write("No num threads specified on cmdline, using defaults in " + cfg.getFileName());
  else {
    params.numThreads = numThreads;
  }

  string searchRandSeed;
  if(cfg.contains("searchRandSeed"))
    searchRandSeed = cfg.getString("searchRandSeed");
  else
    searchRandSeed = Global::uint64ToString(seedRand.nextUInt64());

  NNEvaluator* nnEval;
  {
    Setup::initializeSession(cfg);
    int maxConcurrentEvals = params.numThreads * 2 + 16; // * 2 + 16 just to give plenty of headroom
    int expectedConcurrentEvals = params.numThreads;
    int defaultMaxBatchSize = std::max(8,((params.numThreads+3)/4)*4);
    bool defaultRequireExactNNLen = true;
    bool disableFP16 = false;
    string expectedSha256 = "";
    nnEval = Setup::initializeNNEvaluator(
      modelFile,modelFile,expectedSha256,cfg,logger,seedRand,maxConcurrentEvals,expectedConcurrentEvals,
      board.x_size,board.y_size,defaultMaxBatchSize,defaultRequireExactNNLen,disableFP16,
      Setup::SETUP_FOR_GTP
    );
  }
  logger.write("Loaded neural net");

  {
    bool rulesWereSupported;
    Rules supportedRules = nnEval->getSupportedRules(initialRules,rulesWereSupported);
    if(!rulesWereSupported) {
      cout << "Warning: Rules " << initialRules << " from sgf not supported by neural net, using " << supportedRules << " instead" << endl;
      //Attempt to re-set-up the board using supported rules
      setUpBoardUsingRules(supportedRules);
    }
  }


  //Check for unused config keys
  cfg.warnUnusedKeys(cerr,&logger);

  if(rawNN) {
    NNResultBuf buf;
    bool skipCache = true;
    MiscNNInputParams nnInputParams;
    nnInputParams.noResultUtilityForWhite = params.noResultUtilityForWhite;
    nnEval->evaluate(board,hist,nextPla,nnInputParams,buf,skipCache);

    cout << "Rules: " << hist.rules << endl;
    Board::printBoard(cout, board, Board::NULL_LOC, &(hist.moveHistory));
    buf.result->debugPrint(cout,board);
    return 0;
  }

  AsyncBot* bot = new AsyncBot(params, nnEval, &logger, searchRandSeed);

  bot->setPosition(nextPla,board,hist);
  if(hintLoc != "") {
    bot->setRootHintLoc(Location::ofString(hintLoc,board));
  }

  if(avoidMoves != "") {
    vector<Loc> avoidMoveLocs = Location::parseSequence(avoidMoves,board);
    vector<int> avoidMoveUntilByLoc(Board::MAX_ARR_SIZE,0);
    for(Loc loc: avoidMoveLocs)
      avoidMoveUntilByLoc[loc] = 1;
    bot->setAvoidMoveUntilByLoc(avoidMoveUntilByLoc,avoidMoveUntilByLoc);
  }

  //Print initial state----------------------------------------------------------------
  const Search* search = bot->getSearchStopAndWait();
  ostringstream sout;
  sout << "Rules: " << hist.rules << endl;
  Board::printBoard(sout, board, Board::NULL_LOC, &(hist.moveHistory));

  if(options.branch_.size() > 0) {
    Board copy = board;
    BoardHistory copyHist = hist;
    Player pla = nextPla;
    for(int i = 0; i<options.branch_.size(); i++) {
      Loc loc = options.branch_[i];
      if(!copyHist.isLegal(copy,loc,pla)) {
        cerr << board << endl;
        cerr << "Branch Illegal move for " << PlayerIO::colorToChar(pla) << ": " << Location::toString(loc,board) << endl;
        return 1;
      }
      copyHist.makeBoardMoveAssumeLegal(copy,loc,pla);
      pla = getOpp(pla);
    }
    Board::printBoard(sout, copy, Board::NULL_LOC, &(copyHist.moveHistory));
  }

  sout << "\n";
  logger.write(sout.str());
  sout.clear();

  //Search!----------------------------------------------------------------

  ClockTimer timer;
  nnEval->clearStats();
  Loc loc = bot->genMoveSynchronous(bot->getSearch()->rootPla,TimeControls());
  (void)loc;

  //Postprocess------------------------------------------------------------


  if(printRootNNValues) {
    const NNOutput* nnOutput = search->rootNode->getNNOutput();
    if(nnOutput != NULL) {
      cout << "White win: " << nnOutput->whiteWinProb << endl;
      cout << "White loss: " << nnOutput->whiteLossProb << endl;
      cout << "White noresult: " << nnOutput->whiteNoResultProb << endl;
    }
  }

  // {
  //   ReportedSearchValues values;
  //   bool suc = search->getRootValues(values);
  //   if(!suc)
  //     cout << "Unsuccessful getting root values" << endl;
  //   else
  //     cout << values << endl;
  // }
  // {
  //   ReportedSearchValues values;
  //   bool suc = search->getPrunedRootValues(values);
  //   if(!suc)
  //     cout << "Unsuccessful getting pruned root values" << endl;
  //   else
  //     cout << values << endl;
  // }


  if(printPolicy) {
    const NNOutput* nnOutput = search->rootNode->getNNOutput();
    if(nnOutput != NULL) {
      const float* policyProbs = nnOutput->getPolicyProbsMaybeNoised();
      cout << "Root policy: " << endl;
      for(int y = 0; y<board.y_size; y++) {
        for(int x = 0; x<board.x_size; x++) {
          int pos = NNPos::xyToPos(x,y,nnOutput->nnXLen);
          double prob = policyProbs[pos];
          if(prob < 0)
            cout << "  -  " << " ";
          else
            cout << Global::strprintf("%5.2f",prob*100) << " ";
        }
        cout << endl;
      }
      double prob = policyProbs[NNPos::locToPos(Board::PASS_LOC,board.x_size,nnOutput->nnXLen,nnOutput->nnYLen)];
      cout << "Pass " << Global::strprintf("%5.2f",prob*100) << endl;
    }
  }
  if(printLogPolicy) {
    const NNOutput* nnOutput = search->rootNode->getNNOutput();
    if(nnOutput != NULL) {
      const float* policyProbs = nnOutput->getPolicyProbsMaybeNoised();
      cout << "Root policy: " << endl;
      for(int y = 0; y<board.y_size; y++) {
        for(int x = 0; x<board.x_size; x++) {
          int pos = NNPos::xyToPos(x,y,nnOutput->nnXLen);
          double prob = policyProbs[pos];
          if(prob < 0)
            cout << "  _  " << " ";
          else
            cout << Global::strprintf("%+5.2f",log(prob)) << " ";
        }
        cout << endl;
      }
      double prob = policyProbs[NNPos::locToPos(Board::PASS_LOC,board.x_size,nnOutput->nnXLen,nnOutput->nnYLen)];
      cout << "Pass " << Global::strprintf("%+5.2f",log(prob)) << endl;
    }
  }

  if(printDirichletShape) {
    const NNOutput* nnOutput = search->rootNode->getNNOutput();
    if(nnOutput != NULL) {
      const float* policyProbs = nnOutput->getPolicyProbsMaybeNoised();
      double alphaDistr[NNPos::MAX_NN_POLICY_SIZE];
      int policySize = nnOutput->nnXLen * nnOutput->nnYLen;
      Search::computeDirichletAlphaDistribution(policySize, policyProbs, alphaDistr);
      cout << "Dirichlet alphas with 10.83 total concentration: " << endl;
      for(int y = 0; y<board.y_size; y++) {
        for(int x = 0; x<board.x_size; x++) {
          int pos = NNPos::xyToPos(x,y,nnOutput->nnXLen);
          double alpha = alphaDistr[pos];
          if(alpha < 0)
            cout << "  -  " << " ";
          else
            cout << Global::strprintf("%5.4f",alpha * 10.83) << " ";
        }
        cout << endl;
      }
      double alpha = alphaDistr[NNPos::locToPos(Board::PASS_LOC,board.x_size,nnOutput->nnXLen,nnOutput->nnYLen)];
      cout << "Pass " << Global::strprintf("%5.2f",alpha * 10.83) << endl;
    }
  }


  sout << "Time taken: " << timer.getSeconds() << "\n";
  sout << "Root visits: " << search->getRootVisits() << "\n";
  sout << "NN rows: " << nnEval->numRowsProcessed() << endl;
  sout << "NN batches: " << nnEval->numBatchesProcessed() << endl;
  sout << "NN avg batch size: " << nnEval->averageProcessedBatchSize() << endl;
  std::vector<SearchNode*> nodes = bot->getSearchStopAndWait()->enumerateTreePostOrder();
  sout << "True number of tree nodes: " << nodes.size() << endl;
  sout << "PV: ";
  search->printPV(sout, search->rootNode, 25);
  sout << "\n";
  sout << "Tree:\n";
  search->printTree(sout, search->rootNode, options, perspective);
  logger.write(sout.str());


  if(printGraph) {
    std::reverse(nodes.begin(),nodes.end());
    std::map<SearchNode*,size_t> idxOfNode;
    for(size_t nodeIdx = 0; nodeIdx<nodes.size(); nodeIdx++)
      idxOfNode[nodes[nodeIdx]] = nodeIdx;

    for(int nodeIdx = 0; nodeIdx<nodes.size(); nodeIdx++) {
      SearchNode& node = *(nodes[nodeIdx]);
      int childrenCapacity;
      SearchChildPointer* children = node.getChildren(childrenCapacity);
      for(int i = 0; i<childrenCapacity; i++) {
        SearchNode* child = children[i].getIfAllocated();
        if(child == NULL)
          break;
        cout << nodeIdx << " -> " << idxOfNode[child] << "\n";
      }
    }
    cout << endl;
  }

  delete bot;
  delete nnEval;
  NeuralNet::globalCleanup();
  delete sgf;

  return 0;
}
