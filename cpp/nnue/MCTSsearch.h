#pragma once
#include "Evaluator.h"
#include "VCF/VCFsolver.h"
#include "ExtraStates.h"
const double policyQuant = 50000;
const double policyQuantInv = 1/policyQuant;
 
struct MCTSnode;
class MCTSsearch;

struct MCTSchild
{
  MCTSnode* ptr;
  NU_Loc loc;
  uint16_t policy;//除以policyQuant是原始policy
};

struct MCTSnode
{
  NNUE::MCTSsureResult sureResult;
  int16_t childrennum;
  int16_t legalChildrennum;//=min(合法招法，MAX_MCTS_CHILDREN)
  MCTSchild children[NNUE::MAX_MCTS_CHILDREN];
  

  uint64_t visits;//包括自己
  NNUE::ValueSum WRtotal;//以下一次落子的这一方的视角,1胜-1负
  //平均胜率=WRtotal/visits

  Color nextColor;
  
  MCTSnode(MCTSsearch* search,  Color nextColor,double policyTemp);
  MCTSnode(NNUE::MCTSsureResult sureResult, Color nextColor);
  ~MCTSnode();
  
};

class MCTSsearch 
{
public:





  NNUEHashTable* hashTable;
  MCTSnode   *rootNode;
  Color       board[MaxBS * MaxBS];
  Hash128 posHash;// only board
  ExtraStates states;

  Evaluator evaluator;  
  VCFsolver_NNUE vcfSolver[2];


  std::atomic_bool terminate;

  struct Option
  {
    size_t maxNodes=0;
  } option;
  struct Param
  {
    double expandFactor = 0.2;//传统的mcts对应0.0, 也就是每次playout固定新增1个叶子节点
    double puct = 2;
    double puctPow = 0.75;//传统的mcts对应0.5
    double puctBase = 10;
    double fpuReduction = 0.1;
    double policyTemp = 1.1;

  }params;



  MCTSsearch(const NNUEV2::ModelWeight* w, NNUEHashTable* ht);
  MCTSsearch(const NNUEV2::ModelWeight* w, NNUEHashTable* ht, MCTSsearch::Param p);
  float    fullsearch(Color color, double factor, NU_Loc &bestmove);
  void play(Color color, NU_Loc loc);
  void undo( NU_Loc loc);
  void clearBoard(); 

  NU_Loc bestRootMove() const;
  float getRootValue() const;
  int64_t getRootVisit() const;
  void stop() { terminate.store(true, std::memory_order_relaxed); }

  void             setOptions(size_t maxNodes) { option.maxNodes = maxNodes; }
  void loadParamFile(std::string filename);
  ~MCTSsearch() { if(rootNode!=NULL)delete rootNode; }

private:

  NU_Loc        locbuf[MaxBS * MaxBS];
  NNUE::PolicyType pbuf1[MaxBS * MaxBS], pbuf2[NNUE::MAX_MCTS_CHILDREN];
  float      pbuf3[NNUE::MAX_MCTS_CHILDREN];
  float      gfbuf[NNUEV2::globalFeatureNum];

  friend MCTSnode::MCTSnode(MCTSsearch *search,
                            Color       nextColor,
                            double      policyTemp);
  struct SearchResult
  {
    uint64_t newVisits;
    NNUE::ValueSum WRchange;
  };
  void playForSearch(Color color, NU_Loc loc);
  void undoForSearch(NU_Loc loc);

  SearchResult search(MCTSnode* node, uint64_t remainVisits,bool isRoot);
  NNUE::MCTSsureResult checkSureResult(NU_Loc nextMove, Color color);//check VCF
  int            selectChildIDToSearch(MCTSnode *node);
  void      getGlobalFeatureInput(Color nextPlayer);
  Hash128            getStateHash(Color nextPlayer);



};

