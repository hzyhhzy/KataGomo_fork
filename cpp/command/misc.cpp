#include "../core/global.h"
#include "../core/fileutils.h"
#include "../core/fancymath.h"
#include "../core/makedir.h"
#include "../core/config_parser.h"
#include "../core/parallel.h"
#include "../core/timer.h"
#include "../core/test.h"
#include "../dataio/sgf.h"
#include "../dataio/poswriter.h"
#include "../dataio/files.h"
#include "../search/asyncbot.h"
#include "../program/setup.h"
#include "../program/playutils.h"
#include "../program/play.h"
#include "../command/commandline.h"
#include "../main.h"

#include <chrono>
#include <csignal>

using namespace std;

static std::atomic<bool> sigReceived(false);
static std::atomic<bool> shouldStop(false);
static void signalHandler(int signal)
{
  if(signal == SIGINT || signal == SIGTERM) {
    sigReceived.store(true);
    shouldStop.store(true);
  }
}

static void writeLine(
  const Search* search, const BoardHistory& baseHist,
  const vector<double>& winLossHistory, const vector<double>& scoreHistory, const vector<double>& scoreStdevHistory
) {
  const Board board = search->getRootBoard();
  int nnXLen = search->nnXLen;
  int nnYLen = search->nnYLen;

  cout << board.x_size << " ";
  cout << board.y_size << " ";
  cout << nnXLen << " ";
  cout << nnYLen << " ";
  cout << baseHist.rules.komi << " ";
  if(baseHist.isGameFinished) {
    cout << PlayerIO::playerToString(baseHist.winner) << " ";
    cout << baseHist.isResignation << " ";
    cout << baseHist.finalWhiteMinusBlackScore << " ";
  }
  else {
    cout << "-" << " ";
    cout << "false" << " ";
    cout << "0" << " ";
  }

  //Last move
  Loc moveLoc = Board::NULL_LOC;
  if(baseHist.moveHistory.size() > 0)
    moveLoc = baseHist.moveHistory[baseHist.moveHistory.size()-1].loc;
  cout << NNPos::locToPos(moveLoc,board.x_size,nnXLen,nnYLen) << " ";

  cout << baseHist.moveHistory.size() << " ";
  cout << board.numBlackCaptures << " ";
  cout << board.numWhiteCaptures << " ";

  for(int y = 0; y<board.y_size; y++) {
    for(int x = 0; x<board.x_size; x++) {
      Loc loc = Location::getLoc(x,y,board.x_size);
      if(board.colors[loc] == C_BLACK)
        cout << "x";
      else if(board.colors[loc] == C_WHITE)
        cout << "o";
      else
        cout << ".";
    }
  }
  cout << " ";

  vector<AnalysisData> buf;
  if(!baseHist.isGameFinished) {
    int minMovesToTryToGet = 0; //just get the default number
    bool duplicateForSymmetries = true;
    search->getAnalysisData(buf,minMovesToTryToGet,false,9,duplicateForSymmetries);
  }
  cout << buf.size() << " ";
  for(int i = 0; i<buf.size(); i++) {
    const AnalysisData& data = buf[i];
    cout << NNPos::locToPos(data.move,board.x_size,nnXLen,nnYLen) << " ";
    cout << data.numVisits << " ";
    cout << data.winLossValue << " ";
    cout << data.scoreMean << " ";
    cout << data.scoreStdev << " ";
    cout << data.policyPrior << " ";
  }

  vector<double> ownership = search->getAverageTreeOwnership();
  for(int y = 0; y<board.y_size; y++) {
    for(int x = 0; x<board.x_size; x++) {
      int pos = NNPos::xyToPos(x,y,nnXLen);
      cout << ownership[pos] << " ";
    }
  }

  cout << winLossHistory.size() << " ";
  for(int i = 0; i<winLossHistory.size(); i++)
    cout << winLossHistory[i] << " ";
  cout << scoreHistory.size() << " ";
  assert(scoreStdevHistory.size() == scoreHistory.size());
  for(int i = 0; i<scoreHistory.size(); i++)
    cout << scoreHistory[i] << " " << scoreStdevHistory[i] << " ";

  cout << endl;
}

static void initializeDemoGame(Board& board, BoardHistory& hist, Player& pla, Rand& rand, AsyncBot* bot) {
  static const int numSizes = 9;
  int sizes[numSizes] = {19,13,9,15,11,10,12,14,16};
  int sizeFreqs[numSizes] = {240,18,12,6,2,1,1,1,1};

  const int size = sizes[rand.nextUInt(sizeFreqs,numSizes)];

  board = Board(size,size);
  pla = P_BLACK;
  hist.clear(board,pla,Rules::getTrompTaylorish());
  bot->setPosition(pla,board,hist);

  if(size == 19) {
    //Many games use a special opening
    if(rand.nextBool(0.6)) {
      auto g = [size](int x, int y) { return Location::getLoc(x,y,size); };
      const Move nb = Move(Board::NULL_LOC, P_BLACK);
      const Move nw = Move(Board::NULL_LOC, P_WHITE);
      Player b = P_BLACK;
      Player w = P_WHITE;
      vector<vector<Move>> specialOpenings = {
        //Sanrensei
        { Move(g(3,3), b), nw, Move(g(15,3), b), nw, Move(g(9,3), b) },
        //Low Chinese
        { Move(g(3,3), b), nw, Move(g(16,3), b), nw, Move(g(10,2), b) },
        //Low Chinese
        { Move(g(3,3), b), nw, Move(g(16,3), b), nw, Move(g(10,2), b) },
        //High chinese
        { Move(g(3,3), b), nw, Move(g(16,3), b), nw, Move(g(10,3), b) },
        //Low small chinese
        { Move(g(3,3), b), nw, Move(g(16,3), b), nw, Move(g(11,2), b) },
        //Kobayashi
        { Move(g(3,3), b), Move(g(15,15), w), Move(g(16,3), b), nw, Move(g(16,13), b), Move(g(13,16), w), Move(g(15,9), b) },
        //Kobayashi
        { Move(g(3,3), b), Move(g(15,15), w), Move(g(16,3), b), nw, Move(g(16,13), b), Move(g(13,16), w), Move(g(15,9), b) },
        //Mini chinese
        { Move(g(3,3), b), Move(g(15,15), w), Move(g(15,2), b), nw, Move(g(16,13), b), Move(g(13,16), w), Move(g(16,8), b) },
        //Mini chinese
        { Move(g(3,3), b), Move(g(15,15), w), Move(g(15,2), b), nw, Move(g(16,13), b), Move(g(13,16), w), Move(g(16,8), b) },
        //Micro chinese
        { Move(g(3,3), b), Move(g(15,15), w), Move(g(15,2), b), nw, Move(g(16,13), b), Move(g(13,16), w), Move(g(16,7), b) },
        //Micro chinese with variable other corner
        { Move(g(15,2), b), Move(g(15,15), w), nb, nw, Move(g(16,13), b), Move(g(13,16), w), Move(g(16,7), b) },
        //Boring star points
        { Move(g(15,3), b), Move(g(15,15), w), nb, nw, Move(g(16,13), b), Move(g(13,16), w), Move(g(15,9), b) },
        //High 3-4 counter approaches
        { Move(g(3,3), b), Move(g(15,16), w), Move(g(16,3), b), nw, Move(g(15,14), b), Move(g(14,3), w) },
        //Double 3-3
        { Move(g(2,2), b), nw, Move(g(16,2), b) },
        //Low enclosure
        { Move(g(2,3), b), nw, Move(g(4,2), b) },
        //High enclosure
        { Move(g(2,3), b), nw, Move(g(4,3), b) },
        //5-5 point
        { Move(g(4,4), b) },
        //5-3 point
        { Move(g(2,4), b) },
        //5-4 point
        { Move(g(3,4), b) },
        //3-3 point
        { Move(g(2,2), b) },
        //3-4 point far approach
        { Move(g(3,2), b), Move(g(2,5), w) },
        //Tengen
        { Move(g(9,9), b) },
        //2-2 point
        { Move(g(1,1), b) },
        //Shusaku fuseki
        { Move(g(16,15), b), Move(g(3,16), w), Move(g(15,2), b), Move(g(14,16), w), nb, Move(g(16,4), w), Move(g(15,14), b) },
        //Miyamoto fuseki
        { Move(g(16,13), b), Move(g(3,15), w), Move(g(13,2), b), nw, Move(g(9,16), b) },
        //4-4 1-space low pincer - shared side
        { Move(g(15,15), b), Move(g(3,15), w), nb, nw, Move(g(5,16), b), Move(g(7,16), w) },
        //4-4 2-space high pincer - shared side
        { Move(g(15,15), b), Move(g(3,15), w), nb, nw, Move(g(5,16), b), Move(g(8,15), w) },
        //4-4 1-space low pincer - opponent side
        { Move(g(15,15), b), Move(g(3,15), w), nb, nw, Move(g(2,13), b), Move(g(2,11), w) },
        //4-4 2-space high pincer - opponent side
        { Move(g(15,15), b), Move(g(3,15), w), nb, nw, Move(g(2,13), b), Move(g(3,10), w) },
        //3-4 1-space low approach - shusaku kosumi and long extend
        { Move(g(15,15), b), Move(g(3,16), w), nb, nw, Move(g(2,14), b), Move(g(4,15), w), Move(g(2,10), b) },
        //3-4 1-space low approach low pincer - opponent side
        { Move(g(15,15), b), Move(g(3,16), w), nb, nw, Move(g(2,14), b), Move(g(2,12), w) },
        //3-4 2-space low approach high pincer - opponent side
        { Move(g(15,15), b), Move(g(3,16), w), nb, nw, Move(g(2,14), b), Move(g(3,11), w) },
        //3-4 1-space high approach - opponent side
        { Move(g(15,15), b), Move(g(3,16), w), nb, nw, Move(g(3,14), b) },
        //3-4 1-space high approach low pincer - opponent side
        { Move(g(15,15), b), Move(g(3,16), w), nb, nw, Move(g(3,14), b), Move(g(2,12), w) },
        //3-4 2-space high approach high pincer - opponent side
        { Move(g(15,15), b), Move(g(3,16), w), nb, nw, Move(g(3,14), b), Move(g(3,11), w) },
        //Orthodox
        { Move(g(3,3), b), nw, Move(g(15,2), b), nw, Move(g(16,4), b), Move(g(9,2), w) },
        //Manchurian
        { Move(g(4,3), b), nw, Move(g(16,3), b), nw, Move(g(10,3), b) },
        //Upper Manchurian
        { Move(g(4,4), b), nw, Move(g(16,4), b), nw, Move(g(10,4), b) },
        //Great wall
        { Move(g(9,9), b), nw, Move(g(9,15), b), nw, Move(g(9,3), b), nw, Move(g(8,12), b), nw, Move(g(10,6), b) },
        //Small wall
        { Move(g(9,8), b), nw, Move(g(8,11), b), nw, Move(g(10,5), b) },
        //High approaches
        { Move(g(3,2), b), Move(g(3,4), w), Move(g(16,3), b), Move(g(14,3), w), Move(g(15,16), b), Move(g(15,14), w) },
        //Black hole
        { Move(g(12,14), b), nw, Move(g(14,6), b), nw, Move(g(4,12), b), nw, Move(g(6,4), b) },
        //Crosscut
        { Move(g(9,9), b), Move(g(9,10), w), Move(g(10,10), b), Move(g(10,9), w) },
        //One-point jump center
        { Move(g(9,8), b), nw, Move(g(9,10), b) },
      };

      vector<Move> chosenOpening = specialOpenings[rand.nextUInt((int)specialOpenings.size())];
      vector<vector<Move>> chosenOpenings;

      for(int j = 0; j<8; j++) {
        vector<Move> symmetric;
        for(int k = 0; k<chosenOpening.size(); k++) {
          Loc loc = chosenOpening[k].loc;
          Player movePla = chosenOpening[k].pla;
          if(loc == Board::NULL_LOC || loc == Board::PASS_LOC)
            symmetric.push_back(Move(loc,movePla));
          else {
            int x = Location::getX(loc,size);
            int y = Location::getY(loc,size);
            if(j & 1) x = size-1-x;
            if(j & 2) y = size-1-y;
            if(j & 4) std::swap(x,y);
            symmetric.push_back(Move(Location::getLoc(x,y,size),movePla));
          }
        }
        chosenOpenings.push_back(symmetric);
      }
      for(int j = (int)chosenOpenings.size()-1; j>=1; j--) {
        int r = rand.nextUInt(j+1);
        vector<Move> tmp = chosenOpenings[j];
        chosenOpenings[j] = chosenOpenings[r];
        chosenOpenings[r] = tmp;
      }

      vector<Move> movesPlayed;
      vector<Move> freeMovesPlayed;
      vector<Move> specifiedMovesPlayed;
      while(true) {
        auto withinRadius1 = [size](Loc l0, Loc l1) {
          if(l0 == Board::NULL_LOC || l1 == Board::NULL_LOC || l0 == Board::PASS_LOC || l1 == Board::PASS_LOC)
            return false;
          int x0 = Location::getX(l0,size);
          int y0 = Location::getY(l0,size);
          int x1 = Location::getX(l1,size);
          int y1 = Location::getY(l1,size);
          return std::abs(x0-x1) <= 1 && std::abs(y0-y1) <= 1;
        };
        auto symmetryIsGood = [&movesPlayed,&specifiedMovesPlayed,&freeMovesPlayed,&withinRadius1](const vector<Move>& moves) {
          assert(movesPlayed.size() <= moves.size());
          //Make sure the symmetry matches up to the desired point,
          //and that free moves are not within radius 1 of any specified move
          for(int j = 0; j<movesPlayed.size(); j++) {
            if(moves[j].loc == Board::NULL_LOC) {
              Loc actualLoc = movesPlayed[j].loc;
              for(int k = 0; k<specifiedMovesPlayed.size(); k++) {
                if(withinRadius1(specifiedMovesPlayed[k].loc,actualLoc))
                  return false;
              }
            }
            else if(movesPlayed[j].loc != moves[j].loc)
              return false;
          }

          //Make sure the next move will also not be within radius 1 of any free move.
          if(movesPlayed.size() < moves.size()) {
            Loc nextLoc = moves[movesPlayed.size()].loc;
            for(int k = 0; k<freeMovesPlayed.size(); k++) {
              if(withinRadius1(freeMovesPlayed[k].loc,nextLoc))
                return false;
            }
          }

          return true;
        };

        //Take the first good symmetry
        vector<Move> goodSymmetry;
        for(int i = 0; i<chosenOpenings.size(); i++) {
          if(symmetryIsGood(chosenOpenings[i])) {
            goodSymmetry = chosenOpenings[i];
            break;
          }
        }

        //If we have no further moves on that symmetry, we're done
        if(movesPlayed.size() >= goodSymmetry.size())
          break;

        Move nextMove = goodSymmetry[movesPlayed.size()];
        bool wasSpecified = true;

        if(nextMove.loc == Board::NULL_LOC) {
          wasSpecified = false;
          Search* search = bot->getSearchStopAndWait();
          NNResultBuf buf;
          MiscNNInputParams nnInputParams;
          nnInputParams.drawEquivalentWinsForWhite = search->searchParams.drawEquivalentWinsForWhite;
          search->nnEvaluator->evaluate(board,hist,pla,nnInputParams,buf,false,false);
          std::shared_ptr<NNOutput> nnOutput = std::move(buf.result);

          double temperature = 0.8;
          bool allowPass = false;
          Loc banMove = Board::NULL_LOC;
          Loc loc = PlayUtils::chooseRandomPolicyMove(nnOutput.get(), board, hist, pla, rand, temperature, allowPass, banMove);
          nextMove.loc = loc;
        }

        //Make sure the next move is legal
        if(!hist.isLegal(board,nextMove.loc,nextMove.pla))
          break;

        //Make the move!
        hist.makeBoardMoveAssumeLegal(board,nextMove.loc,nextMove.pla);
        pla = getOpp(pla);

        hist.clear(board,pla,hist.rules);
        bot->setPosition(pla,board,hist);

        movesPlayed.push_back(nextMove);
        if(wasSpecified)
          specifiedMovesPlayed.push_back(nextMove);
        else
          freeMovesPlayed.push_back(nextMove);

        bot->clearSearch();
        writeLine(bot->getSearch(),hist,vector<double>(),vector<double>(),vector<double>());
        std::this_thread::sleep_for(std::chrono::duration<double>(1.0));

      } //Close while(true)

      int numVisits = 20;
      double komi = hist.rules.komi + 0.3 * rand.nextGaussian();
      komi = 0.5 * round(2.0 * komi);
      hist.setKomi((float)komi);
      bot->setPosition(pla,board,hist);
    }
  }

  bot->clearSearch();
  writeLine(bot->getSearch(),hist,vector<double>(),vector<double>(),vector<double>());
  std::this_thread::sleep_for(std::chrono::duration<double>(2.0));

}


int MainCmds::demoplay(const vector<string>& args) {
  Board::initHash();
  ScoreValue::initTables();
  Rand seedRand;

  ConfigParser cfg;
  string logFile;
  string modelFile;
  try {
    KataGoCommandLine cmd("Self-play demo dumping status to stdout");
    cmd.addConfigFileArg("","");
    cmd.addModelFileArg();
    cmd.addOverrideConfigArg();

    TCLAP::ValueArg<string> logFileArg("","log-file","Log file to output to",false,string(),"FILE");
    cmd.add(logFileArg);
    cmd.parseArgs(args);

    modelFile = cmd.getModelFile();
    logFile = logFileArg.getValue();

    cmd.getConfig(cfg);
  }
  catch (TCLAP::ArgException &e) {
    cerr << "Error: " << e.error() << " for argument " << e.argId() << endl;
    return 1;
  }

  Logger logger(&cfg);
  logger.addFile(logFile);

  logger.write("Engine starting...");

  string searchRandSeed = Global::uint64ToString(seedRand.nextUInt64());

  SearchParams params = Setup::loadSingleParams(cfg,Setup::SETUP_FOR_OTHER);

  NNEvaluator* nnEval;
  {
    Setup::initializeSession(cfg);
    const int expectedConcurrentEvals = params.numThreads;
    const int defaultMaxBatchSize = -1;
    const bool defaultRequireExactNNLen = false;
    const bool disableFP16 = false;
    const string expectedSha256 = "";
    nnEval = Setup::initializeNNEvaluator(
      modelFile,modelFile,expectedSha256,cfg,logger,seedRand,expectedConcurrentEvals,
      NNPos::MAX_BOARD_LEN,NNPos::MAX_BOARD_LEN,defaultMaxBatchSize,defaultRequireExactNNLen,disableFP16,
      Setup::SETUP_FOR_OTHER
    );
  }
  logger.write("Loaded neural net");

  const bool allowResignation = cfg.contains("allowResignation") ? cfg.getBool("allowResignation") : false;
  const double resignThreshold = cfg.contains("allowResignation") ? cfg.getDouble("resignThreshold",-1.0,0.0) : -1.0; //Threshold on [-1,1], regardless of winLossUtilityFactor
  const double resignScoreThreshold = cfg.contains("allowResignation") ? cfg.getDouble("resignScoreThreshold",-10000.0,0.0) : -10000.0;

  const double searchFactorWhenWinning = cfg.contains("searchFactorWhenWinning") ? cfg.getDouble("searchFactorWhenWinning",0.01,1.0) : 1.0;
  const double searchFactorWhenWinningThreshold = cfg.contains("searchFactorWhenWinningThreshold") ? cfg.getDouble("searchFactorWhenWinningThreshold",0.0,1.0) : 1.0;

  //Check for unused config keys
  cfg.warnUnusedKeys(cerr,&logger);

  AsyncBot* bot = new AsyncBot(params, nnEval, &logger, searchRandSeed);
  bot->setAlwaysIncludeOwnerMap(true);
  Rand gameRand;

  //Done loading!
  //------------------------------------------------------------------------------------
  logger.write("Loaded all config stuff, starting demo");

  //Game loop
  while(true) {

    Player pla = P_BLACK;
    Board baseBoard;
    BoardHistory baseHist(baseBoard,pla,Rules::getTrompTaylorish());
    TimeControls tc;

    initializeDemoGame(baseBoard, baseHist, pla, gameRand, bot);

    bot->setPosition(pla,baseBoard,baseHist);

    vector<double> recentWinLossValues;
    vector<double> recentScores;
    vector<double> recentScoreStdevs;

    double callbackPeriod = 0.05;

    std::function<void(const Search*)> callback = [&baseHist,&recentWinLossValues,&recentScores,&recentScoreStdevs](const Search* search) {
      writeLine(search,baseHist,recentWinLossValues,recentScores,recentScoreStdevs);
    };

    //Move loop
    int maxMovesPerGame = 1600;
    for(int i = 0; i<maxMovesPerGame; i++) {
      if(baseHist.isGameFinished)
        break;

      callback(bot->getSearch());

      double searchFactor =
        //Speed up when either player is winning confidently, not just the winner only
        std::min(
          PlayUtils::getSearchFactor(searchFactorWhenWinningThreshold,searchFactorWhenWinning,params,recentWinLossValues,P_BLACK),
          PlayUtils::getSearchFactor(searchFactorWhenWinningThreshold,searchFactorWhenWinning,params,recentWinLossValues,P_WHITE)
        );
      Loc moveLoc = bot->genMoveSynchronousAnalyze(pla,tc,searchFactor,callbackPeriod,callbackPeriod,callback);

      bool isLegal = bot->isLegalStrict(moveLoc,pla);
      if(moveLoc == Board::NULL_LOC || !isLegal) {
        ostringstream sout;
        sout << "genmove null location or illegal move!?!" << "\n";
        sout << bot->getRootBoard() << "\n";
        sout << "Pla: " << PlayerIO::playerToString(pla) << "\n";
        sout << "MoveLoc: " << Location::toString(moveLoc,bot->getRootBoard()) << "\n";
        logger.write(sout.str());
        cerr << sout.str() << endl;
        throw StringError("illegal move");
      }

      double winLossValue;
      double expectedScore;
      double expectedScoreStdev;
      {
        ReportedSearchValues values = bot->getSearch()->getRootValuesRequireSuccess();
        winLossValue = values.winLossValue;
        expectedScore = values.expectedScore;
        expectedScoreStdev = values.expectedScoreStdev;
      }

      recentWinLossValues.push_back(winLossValue);
      recentScores.push_back(expectedScore);
      recentScoreStdevs.push_back(expectedScoreStdev);

      bool resigned = false;
      if(allowResignation) {
        const BoardHistory hist = bot->getRootHist();
        const Board initialBoard = hist.initialBoard;

        //Play at least some moves no matter what
        int minTurnForResignation = 1 + initialBoard.x_size * initialBoard.y_size / 6;

        Player resignPlayerThisTurn = C_EMPTY;
        if(winLossValue < resignThreshold && expectedScore < resignScoreThreshold)
          resignPlayerThisTurn = P_WHITE;
        else if(winLossValue > -resignThreshold && expectedScore > -resignScoreThreshold)
          resignPlayerThisTurn = P_BLACK;

        if(resignPlayerThisTurn == pla &&
           bot->getRootHist().moveHistory.size() >= minTurnForResignation)
          resigned = true;
      }

      if(resigned) {
        baseHist.setWinnerByResignation(getOpp(pla));
        break;
      }
      else {
        //And make the move on our copy of the board
        assert(baseHist.isLegal(baseBoard,moveLoc,pla));
        baseHist.makeBoardMoveAssumeLegal(baseBoard,moveLoc,pla);

        //If the game is over, skip making the move on the bot, to preserve
        //the last known value of the search tree for display purposes
        //Just immediately terminate the game loop
        if(baseHist.isGameFinished)
          break;

        bool suc = bot->makeMove(moveLoc,pla);
        assert(suc);
        (void)suc; //Avoid warning when asserts are off

        pla = getOpp(pla);
      }

    }

    //End of game display line
    writeLine(bot->getSearch(),baseHist,recentWinLossValues,recentScores,recentScoreStdevs);
    //Wait a bit before diving into the next game
    std::this_thread::sleep_for(std::chrono::seconds(10));

    bot->clearSearch();
  }

  delete bot;
  delete nnEval;
  NeuralNet::globalCleanup();
  ScoreValue::freeTables();

  logger.write("All cleaned up, quitting");
  return 0;

}

int MainCmds::printclockinfo(const vector<string>& args) {
  (void)args;
#ifdef OS_IS_WINDOWS
  cout << "Does nothing on windows, disabled" << endl;
#endif
#ifdef OS_IS_UNIX_OR_APPLE
  cout << "Tick unit in seconds: " << std::chrono::steady_clock::period::num << " / " <<  std::chrono::steady_clock::period::den << endl;
  cout << "Ticks since epoch: " << std::chrono::steady_clock::now().time_since_epoch().count() << endl;
#endif
  return 0;
}

static void handleStartAnnotations(Sgf* rootSgf) {
  std::function<bool(Sgf*)> hasStartNode = [&hasStartNode](Sgf* sgf) {
    for(SgfNode* node : sgf->nodes) {
      if(node->hasProperty("C")) {
        std::string comment = node->getSingleProperty("C");
        if(comment.find("%START%") != std::string::npos) {
          return true;
        }
      }
    }
    for(Sgf* child : sgf->children) {
      if(hasStartNode(child)) {
        return true;
      }
    }
    return false;
  };

  std::function<void(Sgf*)> markNodes = [&markNodes](Sgf* sgf) {
    bool isInStartSubtree = false;
    for(SgfNode* node : sgf->nodes) {
      if(node->hasProperty("C")) {
        std::string comment = node->getSingleProperty("C");
        if(comment.find("%START%") != std::string::npos) {
          isInStartSubtree = true;
          break;
        }
      }
      node->appendComment("%NOSAMPLE%");
      node->appendComment("%NOHINT%");
    }
    if(!isInStartSubtree) {
      for(Sgf* child : sgf->children)
        markNodes(child);
    }
  };

  if(hasStartNode(rootSgf)) {
    markNodes(rootSgf);
  }
}

static bool maybeGetValuesAfterMove(
  Search* search, Loc moveLoc,
  Player nextPla, const Board& board, const BoardHistory& hist,
  double quickSearchFactor,
  ReportedSearchValues& values
) {
  Board newBoard = board;
  BoardHistory newHist = hist;
  Player newNextPla = nextPla;

  if(moveLoc != Board::NULL_LOC) {
    if(!hist.isLegal(newBoard,moveLoc,newNextPla))
      return false;
    newHist.makeBoardMoveAssumeLegal(newBoard,moveLoc,newNextPla);
    newNextPla = getOpp(newNextPla);
  }

  search->setPosition(newNextPla,newBoard,newHist);

  if(quickSearchFactor != 1.0) {
    SearchParams oldSearchParams = search->searchParams;
    SearchParams newSearchParams = oldSearchParams;
    newSearchParams.maxVisits = 1 + (int64_t)(oldSearchParams.maxVisits * quickSearchFactor);
    newSearchParams.maxPlayouts = 1 + (int64_t)(oldSearchParams.maxPlayouts * quickSearchFactor);
    search->setParamsNoClearing(newSearchParams);
    search->runWholeSearch(newNextPla,shouldStop);
    search->setParamsNoClearing(oldSearchParams);
  }
  else {
    search->runWholeSearch(newNextPla,shouldStop);
  }

  if(shouldStop.load(std::memory_order_acquire))
    return false;
  values = search->getRootValuesRequireSuccess();
  return true;
}



//We want surprising moves that turned out not poorly
//The more surprising, the more we will weight it
static double surpriseWeight(double policyProb, Rand& rand, bool alwaysAddWeight) {
  if(policyProb < 0)
    return 0;
  double weight = 0.12 / (policyProb + 0.02) - 0.5;
  if(alwaysAddWeight && weight < 1.0)
    weight = 1.0;

  if(weight <= 0)
    return 0;
  if(weight < 0.2) {
    if(rand.nextDouble() * 0.2 >= weight)
      return 0;
    return 0.2;
  }
  return weight;
}

struct PosQueueEntry {
  BoardHistory* hist;
  bool markedAsHintPos;
};






int MainCmds::trystartposes(const vector<string>& args) {
  Board::initHash();
  ScoreValue::initTables();
  Rand seedRand;

  ConfigParser cfg;
  string nnModelFile;
  vector<string> startPosesFiles;
  double minWeight;
  try {
    KataGoCommandLine cmd("Try running searches starting from startposes");
    cmd.addConfigFileArg("","");
    cmd.addModelFileArg();
    cmd.addOverrideConfigArg();

    TCLAP::MultiArg<string> startPosesFileArg("","startposes","Startposes file",true,"DIR");
    TCLAP::ValueArg<double> minWeightArg("","min-weight","Minimum weight of startpos to try",false,0.0,"WEIGHT");
    cmd.add(startPosesFileArg);
    cmd.add(minWeightArg);
    cmd.parseArgs(args);
    nnModelFile = cmd.getModelFile();
    startPosesFiles = startPosesFileArg.getValue();
    minWeight = minWeightArg.getValue();
    cmd.getConfig(cfg);
  }
  catch (TCLAP::ArgException &e) {
    cerr << "Error: " << e.error() << " for argument " << e.argId() << endl;
    return 1;
  }

  const bool logToStdoutDefault = true;
  Logger logger(&cfg, logToStdoutDefault);

  SearchParams params = Setup::loadSingleParams(cfg,Setup::SETUP_FOR_ANALYSIS);
  //Ignore temperature, noise
  params.chosenMoveTemperature = 0;
  params.chosenMoveTemperatureEarly = 0;
  params.rootNoiseEnabled = false;
  params.rootDesiredPerChildVisitsCoeff = 0;
  params.rootPolicyTemperature = 1.0;
  params.rootPolicyTemperatureEarly = 1.0;
  params.rootFpuReductionMax = params.fpuReductionMax * 0.5;

  //Disable dynamic utility so that utilities are always comparable
  params.staticScoreUtilityFactor += params.dynamicScoreUtilityFactor;
  params.dynamicScoreUtilityFactor = 0;

  NNEvaluator* nnEval;
  {
    Setup::initializeSession(cfg);
    const int expectedConcurrentEvals = params.numThreads;
    const int defaultMaxBatchSize = std::max(8,((params.numThreads+3)/4)*4);
    const bool defaultRequireExactNNLen = false;
    const bool disableFP16 = false;
    const string expectedSha256 = "";
    nnEval = Setup::initializeNNEvaluator(
      nnModelFile,nnModelFile,expectedSha256,cfg,logger,seedRand,expectedConcurrentEvals,
      NNPos::MAX_BOARD_LEN,NNPos::MAX_BOARD_LEN,defaultMaxBatchSize,defaultRequireExactNNLen,disableFP16,
      Setup::SETUP_FOR_ANALYSIS
    );
  }
  logger.write("Loaded neural net");

  vector<Sgf::PositionSample> startPoses;
  for(size_t i = 0; i<startPosesFiles.size(); i++) {
    const string& startPosesFile = startPosesFiles[i];
    vector<string> lines = FileUtils::readFileLines(startPosesFile,'\n');
    for(size_t j = 0; j<lines.size(); j++) {
      string line = Global::trim(lines[j]);
      if(line.size() > 0) {
        try {
          Sgf::PositionSample posSample = Sgf::PositionSample::ofJsonLine(line);
          startPoses.push_back(posSample);
        }
        catch(const StringError& err) {
          logger.write(string("ERROR parsing startpos:") + err.what());
        }
      }
    }
  }
  string searchRandSeed = Global::uint64ToString(seedRand.nextUInt64());
  Search* search = new Search(params,nnEval,&logger,searchRandSeed);

  // ---------------------------------------------------------------------------------------------------

  for(size_t s = 0; s<startPoses.size(); s++) {
    const Sgf::PositionSample& startPos = startPoses[s];
    if(startPos.weight < minWeight)
      continue;

    Rules rules = PlayUtils::genRandomRules(seedRand);
    Board board = startPos.board;
    Player pla = startPos.nextPla;
    BoardHistory hist;
    hist.clear(board,pla,rules);
    hist.setInitialTurnNumber(startPos.initialTurnNumber);
    bool allLegal = true;
    for(size_t i = 0; i<startPos.moves.size(); i++) {
      bool isLegal = hist.makeBoardMoveTolerant(board,startPos.moves[i].loc,startPos.moves[i].pla);
      if(!isLegal) {
        allLegal = false;
        break;
      }
      pla = getOpp(startPos.moves[i].pla);
    }
    if(!allLegal) {
      throw StringError("Illegal move in startpos: " + Sgf::PositionSample::toJsonLine(startPos));
    }


    Loc hintLoc = startPos.hintLoc;

    {
      ReportedSearchValues values;
      bool suc = maybeGetValuesAfterMove(search,Board::NULL_LOC,pla,board,hist,1.0,values);
      (void)suc;
      assert(suc);
      cout << "Searching startpos: " << "\n";
      cout << "Weight: " << startPos.weight << "\n";
      cout << "Training Weight: " << startPos.trainingWeight << "\n";
      cout << search->getRootHist().rules.toString() << "\n";
      Board::printBoard(cout, search->getRootBoard(), search->getChosenMoveLoc(), &(search->getRootHist().moveHistory));
      search->printTree(cout, search->rootNode, PrintTreeOptions().maxDepth(1),P_WHITE);
      cout << endl;
    }

    if(hintLoc != Board::NULL_LOC) {
      if(search->getChosenMoveLoc() == hintLoc) {
        cout << "There was a hintpos " << Location::toString(hintLoc,board) << ", but it was the chosen move" << "\n";
        cout << endl;
      }
      else {
        ReportedSearchValues values;
        cout << "There was a hintpos " << Location::toString(hintLoc,board) << ", re-searching after playing it: " << "\n";
        bool suc = maybeGetValuesAfterMove(search,hintLoc,pla,board,hist,1.0,values);
        (void)suc;
        assert(suc);
        Board::printBoard(cout, search->getRootBoard(), search->getChosenMoveLoc(), &(search->getRootHist().moveHistory));
        search->printTree(cout, search->rootNode, PrintTreeOptions().maxDepth(1),P_WHITE);
        cout << endl;
      }
    }
  }

  delete search;
  delete nnEval;
  NeuralNet::globalCleanup();
  ScoreValue::freeTables();
  return 0;
}


int MainCmds::viewstartposes(const vector<string>& args) {
  Board::initHash();
  ScoreValue::initTables();

  ConfigParser cfg;
  string modelFile;
  vector<string> startPosesFiles;
  double minWeight;
  int idxToView;
  bool checkLegality;
  try {
    KataGoCommandLine cmd("View startposes");
    cmd.addConfigFileArg("","",false);
    cmd.addModelFileArg();
    cmd.addOverrideConfigArg();

    TCLAP::MultiArg<string> startPosesFileArg("","start-poses-file","Startposes file",true,"DIR");
    TCLAP::ValueArg<double> minWeightArg("","min-weight","Min weight of startpos to view",false,0.0,"WEIGHT");
    TCLAP::ValueArg<int> idxArg("","idx","Index of startpos to view in file",false,-1,"IDX");
    TCLAP::SwitchArg checkLegalityArg("","check-legality","Print startposes that are illegal or that have illegal hints");
    cmd.add(startPosesFileArg);
    cmd.add(minWeightArg);
    cmd.add(idxArg);
    cmd.add(checkLegalityArg);
    cmd.parseArgs(args);
    startPosesFiles = startPosesFileArg.getValue();
    minWeight = minWeightArg.getValue();
    idxToView = idxArg.getValue();
    checkLegality = checkLegalityArg.getValue();

    cmd.getConfigAllowEmpty(cfg);
    if(cfg.getFileName() != "")
      modelFile = cmd.getModelFile();
  }
  catch (TCLAP::ArgException &e) {
    cerr << "Error: " << e.error() << " for argument " << e.argId() << endl;
    return 1;
  }

  Rand rand;

  const bool logToStdoutDefault = true;
  const bool logToStderrDefault = false;
  const bool logTimeDefault = true;
  const bool logConfigContents = cfg.getFileName() != "";
  Logger logger(&cfg, logToStdoutDefault, logToStderrDefault, logTimeDefault, logConfigContents);

  Rules rules;
  AsyncBot* bot = NULL;
  NNEvaluator* nnEval = NULL;
  if(cfg.getFileName() != "") {
    const bool loadKomiFromCfg = false;
    rules = Setup::loadSingleRules(cfg,loadKomiFromCfg);
    SearchParams params = Setup::loadSingleParams(cfg,Setup::SETUP_FOR_GTP);
    {
      Setup::initializeSession(cfg);
      const int expectedConcurrentEvals = params.numThreads;
      const int defaultMaxBatchSize = std::max(8,((params.numThreads+3)/4)*4);
      const bool defaultRequireExactNNLen = false;
      const bool disableFP16 = false;
      const string expectedSha256 = "";
      nnEval = Setup::initializeNNEvaluator(
        modelFile,modelFile,expectedSha256,cfg,logger,rand,expectedConcurrentEvals,
        Board::MAX_LEN,Board::MAX_LEN,defaultMaxBatchSize,defaultRequireExactNNLen,disableFP16,
        Setup::SETUP_FOR_GTP
      );
    }
    logger.write("Loaded neural net");

    string searchRandSeed;
    if(cfg.contains("searchRandSeed"))
      searchRandSeed = cfg.getString("searchRandSeed");
    else
      searchRandSeed = Global::uint64ToString(rand.nextUInt64());

    bot = new AsyncBot(params, nnEval, &logger, searchRandSeed);
  }

  vector<Sgf::PositionSample> startPoses;
  for(size_t i = 0; i<startPosesFiles.size(); i++) {
    const string& startPosesFile = startPosesFiles[i];
    vector<string> lines = FileUtils::readFileLines(startPosesFile,'\n');
    for(size_t j = 0; j<lines.size(); j++) {
      string line = Global::trim(lines[j]);
      if(line.size() > 0) {
        try {
          Sgf::PositionSample posSample = Sgf::PositionSample::ofJsonLine(line);
          startPoses.push_back(posSample);
        }
        catch(const StringError& err) {
          cout << (string("ERROR parsing startpos:") + err.what()) << endl;
        }
      }
    }
  }

  for(size_t s = 0; s<startPoses.size(); s++) {
    const Sgf::PositionSample& startPos = startPoses[s];
    if(startPos.weight < minWeight)
      continue;
    if(idxToView >= 0 && s != idxToView)
      continue;

    Board board = startPos.board;
    Player pla = startPos.nextPla;
    BoardHistory hist;
    hist.clear(board,pla,rules);
    hist.setInitialTurnNumber(startPos.initialTurnNumber);

    bool allLegal = true;
    for(size_t i = 0; i<startPos.moves.size(); i++) {
      bool isLegal = hist.makeBoardMoveTolerant(board,startPos.moves[i].loc,startPos.moves[i].pla);
      if(!isLegal) {
        allLegal = false;
        break;
      }
      pla = getOpp(startPos.moves[i].pla);
    }
    if(!allLegal) {
      if(checkLegality) {
        cout << "Illegal move in startpos in " + Global::concat(startPosesFiles,",") + ": " + Sgf::PositionSample::toJsonLine(startPos) << endl;
        continue;
      }
      else
        throw StringError("Illegal move in startpos: " + Sgf::PositionSample::toJsonLine(startPos));
    }

    if(checkLegality) {
      if(startPos.moves.size() > 0 && startPos.moves[0].pla != startPos.nextPla) {
        cout << "Mismatching nextPla in startpos in " + Global::concat(startPosesFiles,",") + ": " + Sgf::PositionSample::toJsonLine(startPos) << endl;
      }
    }


    Loc hintLoc = startPos.hintLoc;
    if(checkLegality) {
      if(hintLoc != Board::NULL_LOC) {
        bool isLegal = hist.isLegal(board,hintLoc,pla);
        if(!isLegal) {
          cout << "Illegal hint in startpos in " + Global::concat(startPosesFiles,",") + ": " + Sgf::PositionSample::toJsonLine(startPos) << endl;
          Board::printBoard(cout, board, hintLoc, &(hist.moveHistory));
          continue;
        }
      }
    }

    if(bot != NULL || !checkLegality) {
      cout << "StartPos: " << s << "/" << startPoses.size() << "\n";
      cout << "Next pla: " << PlayerIO::playerToString(pla) << "\n";
      cout << "Weight: " << startPos.weight << "\n";
      cout << "TrainingWeight: " << startPos.trainingWeight << "\n";
      cout << "StartPosInitialNextPla: " << PlayerIO::playerToString(startPos.nextPla) << "\n";
      cout << "StartPosMoves: ";
      for(int i = 0; i<(int)startPos.moves.size(); i++)
        cout << (startPos.moves[i].pla == P_WHITE ? "w" : "b") << Location::toString(startPos.moves[i].loc,board) << " ";
      cout << "\n";
      cout << "Auto komi: " << hist.rules.komi << "\n";
      Board::printBoard(cout, board, hintLoc, &(hist.moveHistory));
      cout << endl;

      if(bot != NULL) {
        bot->setPosition(pla,board,hist);
        if(hintLoc != Board::NULL_LOC)
          bot->setRootHintLoc(hintLoc);
        else
          bot->setRootHintLoc(Board::NULL_LOC);
        bot->genMoveSynchronous(bot->getSearch()->rootPla,TimeControls());
        const Search* search = bot->getSearchStopAndWait();
        PrintTreeOptions options;
        Player perspective = P_WHITE;
        search->printTree(cout, search->rootNode, options, perspective);
      }
    }
  }

  if(bot != NULL)
    delete bot;
  if(nnEval != NULL)
    delete nnEval;

  ScoreValue::freeTables();
  return 0;
}


int MainCmds::sampleinitializations(const vector<string>& args) {
  Board::initHash();
  ScoreValue::initTables();

  ConfigParser cfg;
  string modelFile;
  int numToGen;
  bool evaluate;
  try {
    KataGoCommandLine cmd("View startposes");
    cmd.addConfigFileArg("","");
    cmd.addModelFileArg();
    cmd.addOverrideConfigArg();

    TCLAP::ValueArg<int> numToGenArg("","num","Num to gen",false,1,"N");
    TCLAP::SwitchArg evaluateArg("","evaluate","Print out values and scores on the inited poses");
    cmd.add(numToGenArg);
    cmd.add(evaluateArg);
    cmd.parseArgs(args);
    numToGen = numToGenArg.getValue();
    evaluate = evaluateArg.getValue();

    cmd.getConfigAllowEmpty(cfg);
    if(cfg.getFileName() != "")
      modelFile = cmd.getModelFile();
  }
  catch (TCLAP::ArgException &e) {
    cerr << "Error: " << e.error() << " for argument " << e.argId() << endl;
    return 1;
  }

  Rand rand;

  const bool logToStdoutDefault = true;
  Logger logger(&cfg, logToStdoutDefault);

  NNEvaluator* nnEval = NULL;
  if(cfg.getFileName() != "") {
    SearchParams params = Setup::loadSingleParams(cfg,Setup::SETUP_FOR_GTP);
    {
      Setup::initializeSession(cfg);
      const int expectedConcurrentEvals = params.numThreads;
      const int defaultMaxBatchSize = std::max(8,((params.numThreads+3)/4)*4);
      const bool defaultRequireExactNNLen = false;
      const bool disableFP16 = false;
      const string expectedSha256 = "";
      nnEval = Setup::initializeNNEvaluator(
        modelFile,modelFile,expectedSha256,cfg,logger,rand,expectedConcurrentEvals,
        Board::MAX_LEN,Board::MAX_LEN,defaultMaxBatchSize,defaultRequireExactNNLen,disableFP16,
        Setup::SETUP_FOR_GTP
      );
    }
    logger.write("Loaded neural net");
  }

  AsyncBot* evalBot;
  {
    SearchParams params = Setup::loadSingleParams(cfg,Setup::SETUP_FOR_DISTRIBUTED);
    params.maxVisits = 20;
    params.numThreads = 1;
    string seed = Global::uint64ToString(rand.nextUInt64());
    evalBot = new AsyncBot(params, nnEval, &logger, seed);
  }

  //Play no moves in game, since we're sampling initializations
  cfg.overrideKey("maxMovesPerGame","0");

  const bool isDistributed = false;
  PlaySettings playSettings = PlaySettings::loadForSelfplay(cfg, isDistributed);
  GameRunner* gameRunner = new GameRunner(cfg, playSettings, logger);

  for(int i = 0; i<numToGen; i++) {
    string seed = Global::uint64ToString(rand.nextUInt64());
    MatchPairer::BotSpec botSpec;
    botSpec.botIdx = 0;
    botSpec.botName = "";
    botSpec.nnEval = nnEval;
    botSpec.baseParams = Setup::loadSingleParams(cfg,Setup::SETUP_FOR_DISTRIBUTED);

    FinishedGameData* data = gameRunner->runGame(
      seed,
      botSpec,
      botSpec,
      NULL,
      NULL,
      logger,
      nullptr,
      nullptr,
      nullptr,
      nullptr,
      nullptr
    );

    cout << data->startHist.rules.toString() << endl;
    Board::printBoard(cout, data->startBoard, Board::NULL_LOC, &(data->startHist.moveHistory));
    cout << endl;
    if(evaluate) {
      evalBot->setPosition(data->startPla, data->startBoard, data->startHist);
      evalBot->genMoveSynchronous(data->startPla,TimeControls());
      ReportedSearchValues values = evalBot->getSearchStopAndWait()->getRootValuesRequireSuccess();
      cout << "Winloss: " << values.winLossValue << endl;
      cout << "Lead: " << values.lead << endl;
    }

    delete data;
  }

  delete gameRunner;
  delete evalBot;
  if(nnEval != NULL)
    delete nnEval;

  ScoreValue::freeTables();
  return 0;
}
