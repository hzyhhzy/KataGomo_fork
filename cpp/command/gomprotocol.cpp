#include "../core/global.h"
#include "../core/config_parser.h"
#include "../core/timer.h"
#include "../core/datetime.h"
#include "../core/makedir.h"
#include "../dataio/sgf.h"
#include "../search/asyncbot.h"
#include "../program/setup.h"
#include "../program/playutils.h"
#include "../program/play.h"
#include "../command/commandline.h"
#include "../main.h"

using namespace std;

static bool tryParseLoc(const string& s, const Board& b, Loc& loc) {
  return Location::tryOfString(s,b,loc);
}

static bool timeIsValid(const double& time) {
  if(isnan(time) || time < 0.0 || time > 1e50)
    return false;
  return true;
}


static bool getTwoRandomMove(const Board& board, Loc& whiteLoc, Loc& blackLoc, string& responseMoves) {
  if(board.movenum != 3)
    return false;
  double x1, x2, x3, y1, y2, y3; 
  int c = 0;
  for(int x = 0; x < 15; x++)
    for(int y = 0; y < 15; y++) {
      Loc loc = Location::getLoc(x, y, 15);
      if(board.colors[loc] != C_EMPTY) {
        c++;
        if(c == 1) {
          x1 = x;
          y1 = y;
        } else if(c == 2) {
          x2 = x;
          y2 = y;
        } else if(c == 3) {
          x3 = x;
          y3 = y;
        }
      }
    }
  if(c != 3)
    return false;
  double values[15][15];

  for(int x = 0; x < 15; x++)
    for(int y = 0; y < 15; y++) {
      Loc loc = Location::getLoc(x, y, 15);
      if((x == 0 || y == 0 || x == 15 || y == 15) && board.colors[loc] == C_EMPTY)
        values[x][y] = -1.0 / sqrt((x - x1) * (x - x1) + (y - y1) * (y - y1)) -
                       1.0 / sqrt((x - x2) * (x - x2) + (y - y2) * (y - y2)) -
                       1.0 / sqrt((x - x3) * (x - x3) + (y - y3) * (y - y3));
      else
        values[x][y] = -1e32;
    }
  double bestValue;
  int bestX, bestY;

  bestValue = -1e30;
  bestX = -1;
  bestY = -1;
  for(int x = 0; x < 15; x++)
    for(int y = 0; y < 15; y++) {
      if(values[x][y] > bestValue) {
        bestValue = values[x][y];
        bestX = x;
        bestY = y;
      }
    }
  values[bestX][bestY] = -1e31;
  bestValue = -1e30;
  bestX = -1;
  bestY = -1;
  for(int x = 0; x < 15; x++)
    for(int y = 0; y < 15; y++) {
      if(values[x][y] > bestValue) {
        bestValue = values[x][y];
        bestX = x;
        bestY = y;
      }
    }
  values[bestX][bestY] = -1e31;
  bestValue = -1e30;
  bestX = -1;
  bestY = -1;
  for(int x = 0; x < 15; x++)
    for(int y = 0; y < 15; y++) {
      if(values[x][y] > bestValue) {
        bestValue = values[x][y];
        bestX = x;
        bestY = y;
      }
    }
  values[bestX][bestY] = -1e31;
  whiteLoc = Location::getLoc(bestX, bestY, 15);
  responseMoves = to_string(bestX) + "," + to_string(bestY);
  bestValue = -1e30;
  bestX = -1;
  bestY = -1;
  for(int x = 0; x < 15; x++)
    for(int y = 0; y < 15; y++) {
      if(values[x][y] > bestValue) {
        bestValue = values[x][y];
        bestX = x;
        bestY = y;
      }
    }
  values[bestX][bestY] = -1e31;
  blackLoc = Location::getLoc(bestX, bestY, 15);
  responseMoves = responseMoves + " " + to_string(bestX) + "," + to_string(bestY);
  return true;
}



struct GomEngine {
  GomEngine(const GomEngine&) = delete;
  GomEngine& operator=(const GomEngine&) = delete;

  const string nnModelFile;
  const int analysisPVLen;

  double staticPlayoutDoublingAdvantage;
  double genmoveWideRootNoise;
  double analysisWideRootNoise;

  NNEvaluator* nnEval;
  AsyncBot* bot;
  Rules currentRules; //Should always be the same as the rules in bot, if bot is not NULL.

  //Stores the params we want to be using during genmoves or analysis
  SearchParams params;

  TimeControls bTimeControls;
  TimeControls wTimeControls;

  //This move history doesn't get cleared upon consecutive moves by the same side, and is used
  //for undo, whereas the one in search does.
  Board initialBoard;
  Player initialPla;
  vector<Move> moveHistory;

  vector<double> recentWinLossValues;
  double lastSearchFactor;

  Player perspective;

  double genmoveTimeSum;

  GomEngine(
    const string& modelFile, SearchParams initialParams, Rules initialRules,
    double staticPDA,
    double genmoveWRN, double analysisWRN,
    Player persp, int pvLen
  )
    :nnModelFile(modelFile),
     analysisPVLen(pvLen),
     staticPlayoutDoublingAdvantage(staticPDA),
     genmoveWideRootNoise(genmoveWRN),
     analysisWideRootNoise(analysisWRN),
     nnEval(NULL),
     bot(NULL),
     currentRules(initialRules),
     params(initialParams),
     bTimeControls(),
     wTimeControls(),
     initialBoard(),
     initialPla(P_BLACK),
     moveHistory(),
     recentWinLossValues(),
     lastSearchFactor(1.0),
     perspective(persp),
     genmoveTimeSum(0.0)
  {
  }

  ~GomEngine() {
    stopAndWait();
    delete bot;
    delete nnEval;
  }

  void stopAndWait() {
    bot->stopAndWait();
  }

  Rules getCurrentRules() {
    return currentRules;
  }

  void clearStatsForNewGame() {
    //Currently nothing
  }

  //Specify -1 for the sizes for a default
  void setOrResetBoardSize(ConfigParser& cfg, Logger& logger, Rand& seedRand, int boardXSize, int boardYSize) {
    if(nnEval != NULL && boardXSize == nnEval->getNNXLen() && boardYSize == nnEval->getNNYLen())
      return;
    if(nnEval != NULL) {
      assert(bot != NULL);
      bot->stopAndWait();
      delete bot;
      delete nnEval;
      bot = NULL;
      nnEval = NULL;
      logger.write("Cleaned up old neural net and bot");
    }

    bool wasDefault = false;
    if(boardXSize == -1 || boardYSize == -1) {
      boardXSize = Board::MAX_LEN;
      boardYSize = Board::MAX_LEN;
      wasDefault = true;
    }

    int maxConcurrentEvals = params.numThreads * 2 + 16; // * 2 + 16 just to give plenty of headroom
    int expectedConcurrentEvals = params.numThreads;
    int defaultMaxBatchSize = std::max(8,((params.numThreads+3)/4)*4);
    nnEval = Setup::initializeNNEvaluator(
      nnModelFile,nnModelFile,cfg,logger,seedRand,maxConcurrentEvals,expectedConcurrentEvals,
      boardXSize,boardYSize,defaultMaxBatchSize,
      Setup::SETUP_FOR_GTP
    );
    logger.write("Loaded neural net with nnXLen " + Global::intToString(nnEval->getNNXLen()) + " nnYLen " + Global::intToString(nnEval->getNNYLen()));

    {
      bool rulesWereSupported;
      nnEval->getSupportedRules(currentRules,rulesWereSupported);
      if(!rulesWereSupported) {
        throw StringError("Rules " + currentRules.toJsonStringNoKomi() + " from config file " + cfg.getFileName() + " are NOT supported by neural net");
      }
    }

    //On default setup, also override board size to whatever the neural net was initialized with
    //So that if the net was initalized smaller, we don't fail with a big board
    if(wasDefault) {
      boardXSize = nnEval->getNNXLen();
      boardYSize = nnEval->getNNYLen();
    }

    string searchRandSeed;
    if(cfg.contains("searchRandSeed"))
      searchRandSeed = cfg.getString("searchRandSeed");
    else
      searchRandSeed = Global::uint64ToString(seedRand.nextUInt64());

    bot = new AsyncBot(params, nnEval, &logger, searchRandSeed);

    Board board(boardXSize,boardYSize);
    Player pla = P_BLACK;
    BoardHistory hist(board,pla,currentRules);
    vector<Move> newMoveHistory;
    setPositionAndRules(pla,board,hist,board,pla,newMoveHistory);
    clearStatsForNewGame();
  }

  void setPositionAndRules(Player pla, const Board& board, const BoardHistory& h, const Board& newInitialBoard, Player newInitialPla, const vector<Move> newMoveHistory) {
    BoardHistory hist(h);

    currentRules = hist.rules;
    bot->setPosition(pla,board,hist);
    initialBoard = newInitialBoard;
    initialPla = newInitialPla;
    moveHistory = newMoveHistory;
    recentWinLossValues.clear();
  }

  void clearBoard() {
    assert(bot->getRootHist().rules == currentRules);
    int newXSize = bot->getRootBoard().x_size;
    int newYSize = bot->getRootBoard().y_size;
    Board board(newXSize,newYSize);
    Player pla = P_BLACK;
    BoardHistory hist(board,pla,currentRules);
    vector<Move> newMoveHistory;
    setPositionAndRules(pla,board,hist,board,pla,newMoveHistory);
    clearStatsForNewGame();
  }

  bool setPosition(const vector<Move>& initialStones) {
    assert(bot->getRootHist().rules == currentRules);
    int newXSize = bot->getRootBoard().x_size;
    int newYSize = bot->getRootBoard().y_size;
    Board board(newXSize,newYSize);
    for(int i = 0; i<initialStones.size(); i++) {
      if(!board.isOnBoard(initialStones[i].loc) || board.colors[initialStones[i].loc] != C_EMPTY) {
        return false;
      }
      bool suc = board.setStone(initialStones[i].loc, initialStones[i].pla);
      if(!suc) {
        return false;
      }
    }

    //Make sure nothing died along the way
    for(int i = 0; i<initialStones.size(); i++) {
      if(board.colors[initialStones[i].loc] != initialStones[i].pla) {
        return false;
      }
    }
    Player pla = P_BLACK;
    BoardHistory hist(board,pla,currentRules);
    vector<Move> newMoveHistory;
    setPositionAndRules(pla,board,hist,board,pla,newMoveHistory);
    clearStatsForNewGame();
    return true;
  }


  void setStaticPlayoutDoublingAdvantage(double d) {
    staticPlayoutDoublingAdvantage = d;
  }
  void setAnalysisWideRootNoise(double x) {
    analysisWideRootNoise = x;
  }
  void setRootPolicyTemperature(double x) {
    params.rootPolicyTemperature = x;
    bot->setParams(params);
    bot->clearSearch();
  }


  bool play(Loc loc, Player pla) {
    assert(bot->getRootHist().rules == currentRules);
    bool suc = bot->makeMove(loc,pla);
    if(suc)
      moveHistory.push_back(Move(loc,pla));
    return suc;
  }

  bool undo() {
    if(moveHistory.size() <= 0)
      return false;
    assert(bot->getRootHist().rules == currentRules);

    vector<Move> moveHistoryCopy = moveHistory;

    Board undoneBoard = initialBoard;
    BoardHistory undoneHist(undoneBoard,initialPla,currentRules);
    vector<Move> emptyMoveHistory;
    setPositionAndRules(initialPla,undoneBoard,undoneHist,initialBoard,initialPla,emptyMoveHistory);

    for(int i = 0; i<moveHistoryCopy.size()-1; i++) {
      Loc moveLoc = moveHistoryCopy[i].loc;
      Player movePla = moveHistoryCopy[i].pla;
      bool suc = play(moveLoc,movePla);
      assert(suc);
      (void)suc; //Avoid warning when asserts are off
    }
    return true;
  }

  void ponder() {
    bot->ponder(lastSearchFactor);
  }

  struct AnalyzeArgs {
    bool analyzing = false;
    bool lz = false;
    bool kata = false;
    int minMoves = 0;
    int maxMoves = 10000000;
    bool showOwnership = false;
    bool showPVVisits = false;
    double secondsPerReport = 1e30;
    vector<int> avoidMoveUntilByLocBlack;
    vector<int> avoidMoveUntilByLocWhite;
  };

  std::function<void(const Search* search)> getAnalyzeCallback(Player pla, AnalyzeArgs args) {
    std::function<void(const Search* search)> callback;
    //lz-analyze
    if(args.lz && !args.kata) {
      //Avoid capturing anything by reference except [this], since this will potentially be used
      //asynchronously and called after we return
      callback = [args,pla,this](const Search* search) {
        vector<AnalysisData> buf;
        search->getAnalysisData(buf,args.minMoves,false,analysisPVLen);
        if(buf.size() > args.maxMoves)
          buf.resize(args.maxMoves);
        if(buf.size() <= 0)
          return;

        const Board board = search->getRootBoard();
        for(int i = 0; i<buf.size(); i++) {
          if(i > 0)
            cout << " ";
          const AnalysisData& data = buf[i];
          double winrate = 0.5 * (1.0 + data.winLossValue);
          double lcb = PlayUtils::getHackedLCBForWinrate(search,data,pla);
          if(perspective == P_BLACK || (perspective != P_BLACK && perspective != P_WHITE && pla == P_BLACK)) {
            winrate = 1.0-winrate;
            lcb = 1.0 - lcb;
          }
          cout << "info";
          cout << " move " << Location::toString(data.move,board);
          cout << " visits " << data.numVisits;
          cout << " winrate " << round(winrate * 10000.0);
          cout << " prior " << round(data.policyPrior * 10000.0);
          cout << " lcb " << round(lcb * 10000.0);
          cout << " order " << data.order;
          cout << " pv ";
          data.writePV(cout,board);
          if(args.showPVVisits) {
            cout << " pvVisits ";
            data.writePVVisits(cout);
          }
        }
        cout << endl;
      };
    }
    //kata-analyze, analyze (sabaki)
    else {
      callback = [args,pla,this](const Search* search) {
        vector<AnalysisData> buf;
        search->getAnalysisData(buf,args.minMoves,false,analysisPVLen);
        if(buf.size() > args.maxMoves)
          buf.resize(args.maxMoves);
        if(buf.size() <= 0)
          return;

        vector<double> ownership;

        ostringstream out;
        if(!args.kata) {
          //Hack for sabaki - ensure always showing decimal point. Also causes output to be more verbose with trailing zeros,
          //unfortunately, despite doing not improving the precision of the values.
          out << std::showpoint;
        }

        const Board board = search->getRootBoard();
        for(int i = 0; i<buf.size(); i++) {
          if(i > 0)
            out << " ";
          const AnalysisData& data = buf[i];
          double winrate = 0.5 * (1.0 + data.winLossValue);
          double utility = data.utility;
          //We still hack the LCB for consistency with LZ-analyze
          double lcb = PlayUtils::getHackedLCBForWinrate(search,data,pla);
          ///But now we also offer the proper LCB that KataGo actually uses.
          double utilityLcb = data.lcb;
          double drawValue = //data.expectFinishGameMovenum;
            100.0*data.noResultValue;
          if(perspective == P_BLACK || (perspective != P_BLACK && perspective != P_WHITE && pla == P_BLACK)) {
            winrate = 1.0-winrate;
            lcb = 1.0 - lcb;
            utility = -utility;
            utilityLcb = -utilityLcb;
          }
          out << "info";
          out << " move " << Location::toString(data.move,board);
          out << " visits " << data.numVisits;
          out << " utility " << utility;
          out << " winrate " << winrate;
          out << " scoreMean " << drawValue;
          out << " scoreStdev " << data.scoreStdev;
          out << " scoreLead " << drawValue;
          out << " scoreSelfplay " << drawValue;
          out << " prior " << data.policyPrior;
          out << " lcb " << lcb;
          out << " utilityLcb " << utilityLcb;
          out << " order " << data.order;
          out << " pv ";
          data.writePV(out,board);
          if(args.showPVVisits) {
            out << " pvVisits ";
            data.writePVVisits(out);
          }
        }

        cout << out.str() << endl;
      };
    }
    return callback;
  }

  void genMove(
    Player pla,
    Logger& logger, double searchFactorWhenWinningThreshold, double searchFactorWhenWinning,
    bool cleanupBeforePass, bool ogsChatToStderr,
    bool allowResignation, double resignThreshold, int resignConsecTurns, double resignMinScoreDifference,
    bool logSearchInfo, bool debug, bool playChosenMove,
    string& response, bool& responseIsError, bool& maybeStartPondering,
    AnalyzeArgs args
  ) {
    ClockTimer timer;

    response = "";
    responseIsError = false;
    maybeStartPondering = false;

    nnEval->clearStats();
    TimeControls tc = pla == P_BLACK ? bTimeControls : wTimeControls;

    //Make sure we have the right parameters, in case someone ran analysis in the meantime.
    if(params.playoutDoublingAdvantage != staticPlayoutDoublingAdvantage) {
      params.playoutDoublingAdvantage = staticPlayoutDoublingAdvantage;
      bot->setParams(params);
    }
    
    if(params.wideRootNoise != genmoveWideRootNoise) {
      params.wideRootNoise = genmoveWideRootNoise;
      bot->setParams(params);
    }

    //Play faster when winning
    double searchFactor = PlayUtils::getSearchFactor(searchFactorWhenWinningThreshold,searchFactorWhenWinning,params,recentWinLossValues,pla);
    lastSearchFactor = searchFactor;
    Loc moveLoc;
    bot->setAvoidMoveUntilByLoc(args.avoidMoveUntilByLocBlack,args.avoidMoveUntilByLocWhite);
    if(args.analyzing) {
      std::function<void(const Search* search)> callback = getAnalyzeCallback(pla,args);
      moveLoc = bot->genMoveSynchronousAnalyze(pla, tc, searchFactor, args.secondsPerReport, callback);
      //Make sure callback happens at least once
      callback(bot->getSearch());
    }
    else {
      moveLoc = bot->genMoveSynchronous(pla,tc,searchFactor);
    }

    bool isLegal = bot->isLegalStrict(moveLoc,pla);
    if(moveLoc == Board::NULL_LOC || !isLegal) {
      responseIsError = true;
      response = "genmove returned null location or illegal move";
      ostringstream sout;
      sout << "genmove null location or illegal move!?!" << "\n";
      sout << bot->getRootBoard() << "\n";
      sout << "Pla: " << PlayerIO::playerToString(pla) << "\n";
      sout << "MoveLoc: " << Location::toString(moveLoc,bot->getRootBoard()) << "\n";
      logger.write(sout.str());
      genmoveTimeSum += timer.getSeconds();
      return;
    }

    ReportedSearchValues values;
    double winLossValue;
    {
      values = bot->getSearch()->getRootValuesRequireSuccess();
      winLossValue = values.winLossValue;
    }

    //Record data for resignation or adjusting handicap behavior ------------------------
    recentWinLossValues.push_back(winLossValue);

    //Decide whether we should resign---------------------
    bool resigned = false;


    //Snapshot the time NOW - all meaningful play-related computation time is done, the rest is just
    //output of various things.
    double timeTaken = timer.getSeconds();
    genmoveTimeSum += timeTaken;

    //Chatting and logging ----------------------------

      int64_t visits = bot->getSearch()->getRootVisits();
      double winrate = 0.5 * (1.0 + (values.winValue - values.lossValue));
      //Print winrate from desired perspective
      if(perspective == P_BLACK || (perspective != P_BLACK && perspective != P_WHITE && pla == P_BLACK)) {
        winrate = 1.0 - winrate;
      }
      cout << "MESSAGE "
        << "Visits " << visits
        << " Winrate " << Global::strprintf("%.2f%%", winrate * 100.0)
        << " Drawrate " << Global::strprintf("%.2f%%", values.noResultValue * 100.0)
        << " Time " << Global::strprintf("%.3f", timeTaken);
      if(params.playoutDoublingAdvantage != 0.0) {
        cerr << Global::strprintf(
          " (PDA %.2f)",
          bot->getSearch()->getRootPla() == getOpp(params.playoutDoublingAdvantagePla) ?
          -params.playoutDoublingAdvantage : params.playoutDoublingAdvantage);
      }
      cout << " PV ";
      bot->getSearch()->printPVForMove(cerr,bot->getSearch()->rootNode, moveLoc, analysisPVLen);
      cout << endl;

    if(logSearchInfo) {
      ostringstream sout;
      PlayUtils::printGenmoveLog(sout,bot,nnEval,moveLoc,timeTaken,perspective);
      logger.write(sout.str());
    }
    if(debug) {
      PlayUtils::printGenmoveLog(cerr,bot,nnEval,moveLoc,timeTaken,perspective);
    }

    //Actual reporting of chosen move---------------------
    int x = Location::getX(moveLoc, bot->getRootBoard().x_size);
    int y = Location::getY(moveLoc, bot->getRootBoard().x_size);
    response =to_string(x)+","+ to_string(y);

    if(moveLoc != Board::NULL_LOC && isLegal && playChosenMove) {
      bool suc = bot->makeMove(moveLoc,pla);
      if(suc)
        moveHistory.push_back(Move(moveLoc,pla));
      assert(suc);
      (void)suc; //Avoid warning when asserts are off

      maybeStartPondering = true;
    }


    return;
  }

  double searchAndGetValue(
    Player pla,
    Logger& logger,
    double searchTime,
    bool logSearchInfo,
    string& response,
    bool& responseIsError,
    AnalyzeArgs args) {
    ClockTimer timer;

    response = "";
    responseIsError = false;

    nnEval->clearStats();
    TimeControls tc;
    tc.perMoveTime = searchTime;

    // Make sure we have the right parameters, in case someone ran analysis in the meantime.
    if(params.playoutDoublingAdvantage != staticPlayoutDoublingAdvantage) {
      params.playoutDoublingAdvantage = staticPlayoutDoublingAdvantage;
      bot->setParams(params);
    }

    if(params.wideRootNoise != genmoveWideRootNoise) {
      params.wideRootNoise = genmoveWideRootNoise;
      bot->setParams(params);
    }

    // Play faster when winning
    double searchFactor = 1.0;
    lastSearchFactor = searchFactor;
    Loc moveLoc;
    bot->setAvoidMoveUntilByLoc(args.avoidMoveUntilByLocBlack, args.avoidMoveUntilByLocWhite);
    moveLoc = bot->genMoveSynchronous(pla, tc, searchFactor);

    bool isLegal = bot->isLegalStrict(moveLoc, pla);
    if(moveLoc == Board::NULL_LOC || !isLegal) {
      responseIsError = true;
      response = "genmove returned null location or illegal move";
      //cout<< "genmove returned null location or illegal move";
      ostringstream sout;
      sout << "genmove null location or illegal move!?!"
           << "\n";
      sout << bot->getRootBoard() << "\n";
      sout << "Pla: " << PlayerIO::playerToString(pla) << "\n";
      sout << "MoveLoc: " << Location::toString(moveLoc, bot->getRootBoard()) << "\n";
      logger.write(sout.str());
      genmoveTimeSum += timer.getSeconds();
      return 0;
    }

    ReportedSearchValues values;
    double winLossValue;
    {
      values = bot->getSearch()->getRootValuesRequireSuccess();
      winLossValue = values.winLossValue;
    }


    // Snapshot the time NOW - all meaningful play-related computation time is done, the rest is just
    // output of various things.
    double timeTaken = timer.getSeconds();
    genmoveTimeSum += timeTaken;

    // Chatting and logging ----------------------------

    int64_t visits = bot->getSearch()->getRootVisits();
    double winrate = 0.5 * (1.0 + (values.winValue - values.lossValue));
    // Print winrate from desired perspective
    if(perspective == P_BLACK || (perspective != P_BLACK && perspective != P_WHITE && pla == P_BLACK)) {
      winrate = 1.0 - winrate;
    }
    cout << "MESSAGE "
         << "Visits " << visits << " Winrate " << Global::strprintf("%.2f%%", winrate * 100.0) << " Drawrate "
         << Global::strprintf("%.2f%%", values.noResultValue * 100.0) << " Time "
         << Global::strprintf("%.3f", timeTaken);
    if(params.playoutDoublingAdvantage != 0.0) {
      cerr << Global::strprintf(
        " (PDA %.2f)",
        bot->getSearch()->getRootPla() == getOpp(params.playoutDoublingAdvantagePla) ? -params.playoutDoublingAdvantage
                                                                                     : params.playoutDoublingAdvantage);
    }
    cout << " PV ";
    bot->getSearch()->printPVForMove(cerr, bot->getSearch()->rootNode, moveLoc, analysisPVLen);
    cout << endl;

    // Actual reporting of chosen move---------------------
    int x = Location::getX(moveLoc, bot->getRootBoard().x_size);
    int y = Location::getY(moveLoc, bot->getRootBoard().x_size);
    response = to_string(x) + "," + to_string(y);

    if(logSearchInfo) {
      ostringstream sout;
      PlayUtils::printGenmoveLog(sout, bot, nnEval, moveLoc, timeTaken, perspective);
      logger.write(sout.str());
    }

    return values.winValue - values.lossValue;
  }

  void clearCache() {
    bot->clearSearch();
    nnEval->clearCache();
  }


  void analyze(Player pla, AnalyzeArgs args) {
    assert(args.analyzing);
    //Analysis should ALWAYS be with the static value to prevent random hard-to-predict changes
    //for users.
    if(params.playoutDoublingAdvantage != staticPlayoutDoublingAdvantage) {
      params.playoutDoublingAdvantage = staticPlayoutDoublingAdvantage;
      bot->setParams(params);
    }
    //Also wide root, if desired
    if(params.wideRootNoise != analysisWideRootNoise) {
      params.wideRootNoise = analysisWideRootNoise;
      bot->setParams(params);
    }

    std::function<void(const Search* search)> callback = getAnalyzeCallback(pla,args);
    bot->setAvoidMoveUntilByLoc(args.avoidMoveUntilByLocBlack,args.avoidMoveUntilByLocWhite);

    double searchFactor = 1e40; //go basically forever
    bot->analyzeAsync(pla, searchFactor, args.secondsPerReport, callback);
  }

  SearchParams getParams() {
    return params;
  }

  void setParams(SearchParams p) {
    params = p;
    bot->setParams(params);
  }
};



int MainCmds::gomprotocol(int argc, const char* const* argv) {
  Board::initHash();
   
  Rand seedRand;

  ConfigParser cfg;
  string nnModelFile;
  string overrideVersion;
  try {
    KataGoCommandLine cmd("Run KataGo main GTP engine for playing games or casual analysis.");
    cmd.addConfigFileArg(KataGoCommandLine::defaultGtpConfigFileName(),"gtp_example.cfg");
    cmd.addModelFileArg();
    cmd.setShortUsageArgLimit();
    cmd.addOverrideConfigArg();

    TCLAP::ValueArg<string> overrideVersionArg("","override-version","Force KataGo to say a certain value in response to gtp version command",false,string(),"VERSION");
    cmd.add(overrideVersionArg);
    cmd.parse(argc,argv);
    nnModelFile = cmd.getModelFile();
    overrideVersion = overrideVersionArg.getValue();

    cmd.getConfig(cfg);
  }
  catch (TCLAP::ArgException &e) {
    cerr << "Error: " << e.error() << " for argument " << e.argId() << endl;
    return 1;
  }

  Logger logger;
  if(cfg.contains("logFile") && cfg.contains("logDir"))
    throw StringError("Cannot specify both logFile and logDir in config");
  else if(cfg.contains("logFile"))
    logger.addFile(cfg.getString("logFile"));
  else if(cfg.contains("logDir")) {
    MakeDir::make(cfg.getString("logDir"));
    Rand rand;
    logger.addFile(cfg.getString("logDir") + "/" + DateTime::getCompactDateTimeString() + "-" + Global::uint32ToHexString(rand.nextUInt()) + ".log");
  }

  const bool logAllGTPCommunication = cfg.getBool("logAllGTPCommunication");
  const bool logSearchInfo = cfg.getBool("logSearchInfo");
  bool loggingToStderr = false;

  bool logTimeStamp = cfg.contains("logTimeStamp") ? cfg.getBool("logTimeStamp") : true;
  if(!logTimeStamp)
    logger.setLogTime(false);

  bool startupPrintMessageToStderr = true;
  if(cfg.contains("startupPrintMessageToStderr"))
    startupPrintMessageToStderr = cfg.getBool("startupPrintMessageToStderr");

  if(cfg.contains("logToStderr") && cfg.getBool("logToStderr")) {
    loggingToStderr = true;
    logger.setLogToStderr(true);
  }

  logger.write("GTP Engine starting...");
  logger.write(Version::getKataGoVersionForHelp());
  //Also check loggingToStderr so that we don't duplicate the message from the log file

  //Defaults to 7.5 komi, gtp will generally override this
  Rules initialRules = Setup::loadSingleRulesExceptForKomi(cfg);
  logger.write("Using " + initialRules.toStringNoKomiMaybeNice() + " rules initially, unless GTP/GUI overrides this");
  bool isForcingKomi = false;
  float forcedKomi = 0;
  if(cfg.contains("ignoreGTPAndForceKomi")) {
    isForcingKomi = true;
    forcedKomi = cfg.getFloat("ignoreGTPAndForceKomi", Rules::MIN_USER_KOMI, Rules::MAX_USER_KOMI);
    initialRules.komi = forcedKomi;
  }

  SearchParams initialParams = Setup::loadSingleParams(cfg);
  logger.write("Using " + Global::intToString(initialParams.numThreads) + " CPU thread(s) for search");
  //Set a default for conservativePass that differs from matches or selfplay
  if(!cfg.contains("conservativePass") && !cfg.contains("conservativePass0"))
    initialParams.conservativePass = true;

  const bool ponderingEnabled = cfg.getBool("ponderingEnabled");
  const bool cleanupBeforePass = cfg.contains("cleanupBeforePass") ? cfg.getBool("cleanupBeforePass") : true;
  const bool allowResignation = cfg.contains("allowResignation") ? cfg.getBool("allowResignation") : false;
  const double resignThreshold = cfg.contains("allowResignation") ? cfg.getDouble("resignThreshold",-1.0,0.0) : -1.0; //Threshold on [-1,1], regardless of winLossUtilityFactor
  const int resignConsecTurns = cfg.contains("resignConsecTurns") ? cfg.getInt("resignConsecTurns",1,100) : 3;
  const double resignMinScoreDifference = cfg.contains("resignMinScoreDifference") ? cfg.getDouble("resignMinScoreDifference",0.0,1000.0) : -1e10;

  Setup::initializeSession(cfg);

  const double searchFactorWhenWinning = cfg.contains("searchFactorWhenWinning") ? cfg.getDouble("searchFactorWhenWinning",0.01,1.0) : 1.0;
  const double searchFactorWhenWinningThreshold = cfg.contains("searchFactorWhenWinningThreshold") ? cfg.getDouble("searchFactorWhenWinningThreshold",0.0,1.0) : 1.0;
  const bool ogsChatToStderr = cfg.contains("ogsChatToStderr") ? cfg.getBool("ogsChatToStderr") : false;
  const int analysisPVLen = cfg.contains("analysisPVLen") ? cfg.getInt("analysisPVLen",1,1000) : 13;
  const bool assumeMultipleStartingBlackMovesAreHandicap =
    cfg.contains("assumeMultipleStartingBlackMovesAreHandicap") ? cfg.getBool("assumeMultipleStartingBlackMovesAreHandicap") : true;
  const bool preventEncore = cfg.contains("preventCleanupPhase") ? cfg.getBool("preventCleanupPhase") : true;
  const double dynamicPlayoutDoublingAdvantageCapPerOppLead =
    cfg.contains("dynamicPlayoutDoublingAdvantageCapPerOppLead") ? cfg.getDouble("dynamicPlayoutDoublingAdvantageCapPerOppLead",0.0,0.5) : 0.045;
  double staticPlayoutDoublingAdvantage = initialParams.playoutDoublingAdvantage;

  const int defaultBoardXSize =
    cfg.contains("defaultBoardXSize") ? cfg.getInt("defaultBoardXSize",2,Board::MAX_LEN) :
    cfg.contains("defaultBoardSize") ? cfg.getInt("defaultBoardSize",2,Board::MAX_LEN) :
    -1;
  const int defaultBoardYSize =
    cfg.contains("defaultBoardYSize") ? cfg.getInt("defaultBoardYSize",2,Board::MAX_LEN) :
    cfg.contains("defaultBoardSize") ? cfg.getInt("defaultBoardSize",2,Board::MAX_LEN) :
    -1;
  const bool forDeterministicTesting =
    cfg.contains("forDeterministicTesting") ? cfg.getBool("forDeterministicTesting") : false;

  if(forDeterministicTesting)
    seedRand.init("forDeterministicTesting");

  double swap2time = 5400;//swap2局时，只用于开局阶段计算
  const double genmoveWideRootNoise = initialParams.wideRootNoise;
  const double analysisWideRootNoise =
    cfg.contains("analysisWideRootNoise") ? cfg.getDouble("analysisWideRootNoise",0.0,5.0) : genmoveWideRootNoise;

  Player perspective = Setup::parseReportAnalysisWinrates(cfg,C_EMPTY);

  GomEngine* engine = new GomEngine(
    nnModelFile,initialParams,initialRules,
    staticPlayoutDoublingAdvantage,
    genmoveWideRootNoise,analysisWideRootNoise,
    perspective,analysisPVLen
  );
  engine->setOrResetBoardSize(cfg,logger,seedRand,defaultBoardXSize,defaultBoardYSize);

  //If nobody specified any time limit in any way, then assume a relatively fast time control
  if(!cfg.contains("maxPlayouts") && !cfg.contains("maxVisits") && !cfg.contains("maxTime")) {
    TimeControls tc;
    tc.perMoveTime = 10;
    engine->bTimeControls = tc;
    engine->wTimeControls = tc;
  }

  //Check for unused config keys
  cfg.warnUnusedKeys(cerr,&logger);

  logger.write("Loaded config " + cfg.getFileName());
  logger.write("Loaded model "+ nnModelFile);
  logger.write("Model name: "+ (engine->nnEval == NULL ? string() : engine->nnEval->getInternalModelName()));
  logger.write("GTP ready, beginning main protocol loop");
  //Also check loggingToStderr so that we don't duplicate the message from the log file
  cout << "MESSAGE Katagomo 2021.4.27 by HZY" << endl;
  cout << "MESSAGE Opensourced on github.com/hzyhhzy/katago/tree/gomoku" << endl;
  cout << "MESSAGE QQ:2658628026,  QQ Group:1049389629" << endl;
  cout << "MESSAGE Modified from Katago(github.com/lightvector/katago)" << endl;
#ifdef FORGOMOCUP
  cout << "MESSAGE This is a special version for Gomocup. It only supports single thread(maybe you can run it with "
          "multithread, but some bugs may occur), and works only on CPU. If you want full strength version, please "
          "download it on github.com/hzyhhzy/katago/tree/gomoku. You can download packages on release page(suggested), "
          "or compile it yourself"
       << endl;
  
#endif
#if RULE == FREESTYLE
  string rulestring = "freestyle";
#elif RULE == STANDARD
  string rulestring = "standard";
#elif RULE == RENJU
  string rulestring = "renju";
#endif
  cout << "MESSAGE Engine Rule: " << rulestring << endl;
  cout << "MESSAGE Board Size: " << MAX_FLEN << endl;
  cout << "MESSAGE Loaded config " << cfg.getFileName() << endl;
  cout << "MESSAGE Loaded model " << nnModelFile << endl;
  cout << "MESSAGE Model name: " + (engine->nnEval == NULL ? string() : engine->nnEval->getInternalModelName()) << endl;
  cout << "MESSAGE GTP ready, beginning main protocol loop" << endl;

  bool currentlyAnalyzing = false;
  string line;
  while(getline(cin,line)) {
    //Parse command, extracting out the command itself, the arguments, and any GTP id number for the command.
    string command;
    vector<string> pieces;
    bool hasId = false;
    int id = 0;
    {
      //Filter down to only "normal" ascii characters. Also excludes carrage returns.
      //Newlines are already handled by getline
      size_t newLen = 0;
      for(size_t i = 0; i < line.length(); i++)
        if(((int)line[i] >= 32 && (int)line[i] <= 126) || line[i] == '\t')
          line[newLen++] = line[i];

      line.erase(line.begin()+newLen, line.end());

      //Remove comments
      size_t commentPos = line.find("#");
      if(commentPos != string::npos)
        line = line.substr(0, commentPos);

      //Convert tabs to spaces
      for(size_t i = 0; i < line.length(); i++)
        if(line[i] == '\t'||line[i] == ',')
          line[i] = ' ';

      line = Global::trim(line);

      //Upon any input line at all, stop any analysis and output a newline
      if(currentlyAnalyzing) {
        currentlyAnalyzing = false;
        engine->stopAndWait();
        cout << endl;
      }

      if(line.length() == 0)
        continue;

      if(logAllGTPCommunication)
        logger.write("Controller: " + line);

      //Parse id number of command, if present
      size_t digitPrefixLen = 0;
      while(digitPrefixLen < line.length() && Global::isDigit(line[digitPrefixLen]))
        digitPrefixLen++;
      if(digitPrefixLen > 0) {
        hasId = true;
        try {
          id = Global::parseDigits(line,0,digitPrefixLen);
        }
        catch(const IOError& e) {
          cout << "? GTP id '" << id << "' could not be parsed: " << e.what() << endl;
          continue;
        }
        line = line.substr(digitPrefixLen);
      }

      line = Global::trim(line);
      if(line.length() <= 0) {
        cout << "? empty command" << endl;
        continue;
      }

      pieces = Global::split(line,' ');
      for(size_t i = 0; i<pieces.size(); i++)
        pieces[i] = Global::trim(pieces[i]);
      assert(pieces.size() > 0);

      command = pieces[0];
      pieces.erase(pieces.begin());
    }

    bool responseIsError = false;
    bool suppressResponse = false;
    bool shouldQuitAfterResponse = false;
    bool maybeStartPondering = false;
    string response;

    if(command == "ABOUT") {
      response = "name=\"Katagomo\", version=\"2021.4\", author=\"HZY\", country=\"China\", email=\"2658628026@qq.com\", others=\"Based on Katago v1.7.0 by Lightvector";
    }

    else if(command == "END") {
      shouldQuitAfterResponse = true;
      logger.write("Quit requested by controller");
    }

    else if(command == "START") {
      response = "OK";
      /*
      int newXSize = 0;
      int newYSize = 0;
      bool suc = false;

      if(pieces.size() == 1) {
        if(contains(pieces[0],':')) {
          vector<string> subpieces = Global::split(pieces[0],':');
          if(subpieces.size() == 2 && Global::tryStringToInt(subpieces[0], newXSize) && Global::tryStringToInt(subpieces[1], newYSize))
            suc = true;
        }
        else {
          if(Global::tryStringToInt(pieces[0], newXSize)) {
            suc = true;
            newYSize = newXSize;
          }
        }
      }
      else if(pieces.size() == 2) {
        if(Global::tryStringToInt(pieces[0], newXSize) && Global::tryStringToInt(pieces[1], newYSize))
          suc = true;
      }

      if(!suc) {
        responseIsError = true;
        response = "Expected int argument for boardsize or pair of ints but got '" + Global::concat(pieces," ") + "'";
      }
      else if(newXSize !=Board::MAX_LEN || newYSize != Board::MAX_LEN) {
        responseIsError = true;
        response = "this version only support "+to_string(Board::MAX_LEN)+"x"+ to_string(Board::MAX_LEN) + " board";
      }
      else {
        engine->setOrResetBoardSize(cfg,logger,seedRand,newXSize,newYSize);
        response = "OK";
      }*/
    }

    else if(command == "RESTART") {
      engine->clearBoard();
      response = "OK";
    }

    else if (command == "INFO")
    {
      if (pieces.size() == 0) {}
      else
      {
        string subcommand = pieces[0];
        if (subcommand == "time_left")
        {
          double time = 0;
          if (pieces.size() != 2 || !Global::tryStringToDouble(pieces[1], time)) {
            responseIsError = true;
            response = "Expected 1 arguments for info:time_left but got '" + Global::concat(pieces, " ") + "'";
          }
          else
          {
            engine->bTimeControls.mainTimeLeft = time / 1000.0;
            engine->wTimeControls.mainTimeLeft = time / 1000.0;
          }
        }
        else if (subcommand == "timeout_turn")
        {
          double time = 0;
          if (pieces.size() != 2 || !Global::tryStringToDouble(pieces[1], time)) {
            responseIsError = true;
            response = "Expected 1 arguments for info:time_left but got '" + Global::concat(pieces, " ") + "'";
          }
          else
          {
            engine->bTimeControls.perMoveTime = time / 1000.0;
            engine->wTimeControls.perMoveTime = time / 1000.0;
          }
        }
      }
    }

    else if (command == "BOARD") {
      engine->clearCache();
      engine->clearBoard();

      string moveline;
      vector<Move> initialStones;
      Player p = P_BLACK;
      while (getline(cin, moveline))
      {
        //Convert , to spaces
        for (size_t i = 0; i < moveline.length(); i++)
          if (moveline[i] == ',')
            moveline[i] = ' ';

        moveline = Global::trim(moveline);
        //cout << moveline;
        if (moveline == "DONE")
        {
          bool debug = false;
          bool playChosenMove = true;
          engine->setPosition(initialStones);
          engine->genMove(
            p,
            logger, searchFactorWhenWinningThreshold, searchFactorWhenWinning,
            cleanupBeforePass, ogsChatToStderr,
            false, resignThreshold, resignConsecTurns,0,
            logSearchInfo, debug, playChosenMove,
            response, responseIsError, maybeStartPondering,
            GomEngine::AnalyzeArgs()
          );
          break;
        }
        else {
          stringstream ss(moveline);
          int x, y;
          ss >> x >> y;
          if (x < 0 || x >= Board::MAX_LEN || y < 0 || y >= Board::MAX_LEN)
          {
            responseIsError = true;
            response = "Move Outside Board";
          }
          else
          {
            Loc loc = Location::getLoc(x, y, Board::MAX_LEN);
            initialStones.push_back(Move(loc, p));
            p = getOpp(p);
          }
        }
      }
    } 
    
    else if(command == "SWAP2BOARD") {
#if RULE != 1
      throw StringError("SWAP2 is only for STANDARD rule");
#endif
      engine->clearCache();
      engine->clearBoard();

      string moveline;
      vector<Move> initialStones;
      Player p = P_BLACK;
      while(getline(cin, moveline)) {
        // Convert , to spaces
        for(size_t i = 0; i < moveline.length(); i++)
          if(moveline[i] == ',')
            moveline[i] = ' ';

        moveline = Global::trim(moveline);
        // cout << moveline;
        if(moveline == "DONE") {
          bool debug = false;
          bool playChosenMove = false;
          int swap2num = initialStones.size();
          if(swap2num == 0) {
            static const string openings[10] = {
              "5,0 6,0 6,1", 
              "3,14 12,14 4,14", 
              "7,0 11,0 5,0", 
              "7,0 9,0 7,1", 
              "1,1 2,1 3,2",
              "7,0 5,0 6,1",
              "1,2 0,0 2,1",
              "0,0 0,3 3,2",
              "8,1 9,0 10,0",
              "7,0 6,0 8,1",
            };
            int choice = seedRand.nextUInt(10);
            response = openings[choice];
          } 
          else if(swap2num == 3) {
            engine->setPosition(initialStones);
            double value = engine->searchAndGetValue(
              p, logger, swap2time / 10, logSearchInfo, response, responseIsError, GomEngine::AnalyzeArgs());

            string response1 = response;
            if(value < -0.15) {
              response = "SWAP";  
              cout << "MESSAGE White winrate = " << 50 * (value + 1) << "%, So engine plays black" << endl;
            } 
            else if(value > 0.15) {
              cout << "MESSAGE White winrate = " << 50 * (value + 1) << "%, So engine plays white" << endl;
            }  
            else {
              cout << "MESSAGE White winrate = " << 50 * (value + 1) << "%, So randomly plays 2 moves" << endl;
              Loc blackLoc, whiteLoc;
              string random2response;
              getTwoRandomMove(engine->bot->getRootBoard(), whiteLoc, blackLoc, random2response);

              bool suc1 = engine->play(whiteLoc, C_WHITE);
              bool suc2 = engine->play(blackLoc, C_BLACK);
              if(!suc1 || !suc2) {
                cout << "DEBUG unknown error" << endl;
                response = "SWAP";
              }
              double value2 = engine->searchAndGetValue(
                p, logger, swap2time / 20, logSearchInfo, response, responseIsError, GomEngine::AnalyzeArgs());
              if(value2 > -0.25 && value2 < 0.25) {
                cout << "MESSAGE After these two moves, white winrate = " << 50 * (value2 + 1)
                     << "%, So engine plays these two moves" << endl;
                response = random2response;
              } else {
                if(value < 0)
                  response = "SWAP"; 
                else {
                  cout << "MESSAGE After these two moves, white winrate = " << 50 * (value + 1)
                       << "%, So not play these two moves" << endl;
                  response = response1;
                }  
              }
            }
          }

          else if(swap2num == 5) {
            engine->setPosition(initialStones);
            double value = engine->searchAndGetValue(
              p, logger, swap2time / 10, logSearchInfo, response, responseIsError, GomEngine::AnalyzeArgs());
            if(value < 0) {
              cout << "MESSAGE White winrate = " << 50 * (value + 1) << "%, So engine plays black" << endl;
              response = "SWAP";
            } else {
              cout << "MESSAGE White winrate = " << 50 * (value + 1) << "%, So engine plays white" << endl;
            }
          }
          break;

        } 
        else {
          stringstream ss(moveline);
          int x, y;
          ss >> x >> y;
          if(x < 0 || x >= Board::MAX_LEN || y < 0 || y >= Board::MAX_LEN) {
            responseIsError = true;
            response = "Move Outside Board";
          } else {
            Loc loc = Location::getLoc(x, y, Board::MAX_LEN);
            initialStones.push_back(Move(loc, p));
            p = getOpp(p);
          }
        }
      }
    }
    /*
    else if(command == "play") {
      Player pla;
      Loc loc;
      if(pieces.size() != 2) {
        responseIsError = true;
        response = "Expected two arguments for play but got '" + Global::concat(pieces," ") + "'";
      }
      else if(!PlayerIO::tryParsePlayer(pieces[0],pla)) {
        responseIsError = true;
        response = "Could not parse color: '" + pieces[0] + "'";
      }
      else if(!tryParseLoc(pieces[1],engine->bot->getRootBoard(),loc)) {
        responseIsError = true;
        response = "Could not parse vertex: '" + pieces[1] + "'";
      }
      else {
        bool suc = engine->play(loc,pla);
        if(!suc) {
          responseIsError = true;
          response = "illegal move";
        }
        maybeStartPondering = true;
      }
    }
    */
    else if(command == "BEGIN") {
      const Board& b = engine->bot->getRootBoard();
      Player nextPla = b.movenum % 2 ? P_WHITE : P_BLACK;
      bool debug = false;
      bool playChosenMove = true;
      nextPla = getOpp(nextPla);
      engine->genMove(
        nextPla,
        logger,
        searchFactorWhenWinningThreshold,
        searchFactorWhenWinning,
        cleanupBeforePass,
        ogsChatToStderr,
        allowResignation,
        resignThreshold,
        resignConsecTurns,
        0,
        logSearchInfo,
        debug,
        playChosenMove,
        response,
        responseIsError,
        maybeStartPondering,
        GomEngine::AnalyzeArgs());
    } 
    else if (command == "TURN") {
      const Board& b = engine->bot->getRootBoard();
      Player nextPla = b.movenum % 2 ? P_WHITE : P_BLACK;
      Loc loc;
      if (pieces.size() != 2) {
        responseIsError = true;
        response = "Expected 2 arguments for TURN but got '" + Global::concat(pieces, " ") + "'";
      }
      else {
        int x = stoi(pieces[0]), y = stoi(pieces[1]);
        loc = Location::getLoc(x, y, engine->bot->getRootBoard().x_size);
        bool suc = engine->play(loc, nextPla);
        if (!suc) {
          responseIsError = true;
          response = "illegal move";
        }
      }
      if (!responseIsError)
      {
        bool debug = false;
        bool playChosenMove = true;
        nextPla = getOpp(nextPla);
        engine->genMove(
          nextPla,
          logger, searchFactorWhenWinningThreshold, searchFactorWhenWinning,
          cleanupBeforePass, ogsChatToStderr,
          allowResignation, resignThreshold, resignConsecTurns, 0,
          logSearchInfo, debug, playChosenMove,
          response, responseIsError, maybeStartPondering,
          GomEngine::AnalyzeArgs()
        );

      }
    }

    else if(command == "setswap2time") {
      float newSwap2Time = 5400;
      if(pieces.size() != 1 || !Global::tryStringToFloat(pieces[0], newSwap2Time) || newSwap2Time <= 1) {
        responseIsError = true;
        response = "Expected single float argument for setSwap2Time but got '" + Global::concat(pieces, " ") + "'";
      } else {
        swap2time = newSwap2Time;
      }
    }

    /*
    else if(command == "genmove" || command == "genmove_debug" || command == "search_debug") {
      Player pla;
      if(pieces.size() != 1) {
        responseIsError = true;
        response = "Expected one argument for genmove but got '" + Global::concat(pieces," ") + "'";
      }
      else if(!PlayerIO::tryParsePlayer(pieces[0],pla)) {
        responseIsError = true;
        response = "Could not parse color: '" + pieces[0] + "'";
      }
      else {
        bool debug = command == "genmove_debug" || command == "search_debug";
        bool playChosenMove = command != "search_debug";

        engine->genMove(
          pla,
          logger,searchFactorWhenWinningThreshold,searchFactorWhenWinning,
          cleanupBeforePass,ogsChatToStderr,
          allowResignation,resignThreshold,resignConsecTurns,resignMinScoreDifference,
          logSearchInfo,debug,playChosenMove,
          response,responseIsError,maybeStartPondering,
          GomEngine::AnalyzeArgs()
        );
      }
    }

    */
    else if(command == "clear_cache") {
      engine->clearCache();
    }
    else if(command == "showboard") {
      ostringstream sout;
      engine->bot->getRootHist().printBasicInfo(sout, engine->bot->getRootBoard());
      //Filter out all double newlines, since double newline terminates GTP command responses
      string s = sout.str();
      string filtered;
      for(int i = 0; i<s.length(); i++) {
        if(i > 0 && s[i-1] == '\n' && s[i] == '\n')
          continue;
        filtered += s[i];
      }
      response = Global::trim(filtered);
    }
    /*

    else if(command == "analyze" || command == "lz-analyze" || command == "kata-analyze") {
      Player pla = engine->bot->getRootPla();
      bool parseFailed = false;
      GomEngine::AnalyzeArgs args = parseAnalyzeCommand(command, pieces, pla, parseFailed, engine);

      if(parseFailed) {
        responseIsError = true;
        response = "Could not parse analyze arguments or arguments out of range: '" + Global::concat(pieces," ") + "'";
      }
      else {
        //Make sure the "equals" for GTP is printed out prior to the first analyze line, regardless of thread racing
        if(hasId)
          cout << "=" << Global::intToString(id) << endl;
        else
          cout << "=" << endl;

        engine->analyze(pla, args);

        //No response - currentlyAnalyzing will make sure we get a newline at the appropriate time, when stopped.
        suppressResponse = true;
        currentlyAnalyzing = true;
      }
    }


    else if(command == "cputime" || command == "gomill-cpu_time") {
      response = Global::doubleToString(engine->genmoveTimeSum);
    }

    else if(command == "stop") {
      //Stop any ongoing ponder or analysis
      engine->stopAndWait();
    }
    */
    else {
      responseIsError = true;
      response = "unknown command";
    }



    if(responseIsError)
      response = "ERROR " + response;

    if((!suppressResponse)&&(response!="")) {
      cout << response << endl;
    }

    if(logAllGTPCommunication)
      logger.write(response);

    if(shouldQuitAfterResponse)
      break;

    if(maybeStartPondering && ponderingEnabled)
      engine->ponder();

  } //Close read loop

  delete engine;
  engine = NULL;
  NeuralNet::globalCleanup();
   

  logger.write("All cleaned up, quitting");
  return 0;
}
