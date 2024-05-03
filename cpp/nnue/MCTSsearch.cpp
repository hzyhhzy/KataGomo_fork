#include "MCTSsearch.h"
using namespace NNUE;

inline ValueSum sureResultWR(MCTSsureResult sr)
{
  if (sr == MC_Win)
    return ValueSum(1, 0, 0);
  else if (sr == MC_LOSE)
    return ValueSum(0, 1, 0);
  else if (sr == MC_DRAW)
    return ValueSum(0, 0, 1);
  else
    return ValueSum(-1e100, -1e100, -1e100);
}

inline double MCTSpuctFactor(double totalVisit, double puct, double puctPow, double puctBase)
{
  return  puct * pow((totalVisit + puctBase) / puctBase, puctPow);
}

inline double MCTSselectionValue(double puctFactor,
  double value,
  double draw,
  double   parentdraw,
  double   childVisit,
  double   childPolicy)
{
  return value - 0.5 * draw * (1 - parentdraw) + puctFactor * childPolicy / (childVisit + 1);
}

MCTSnode::MCTSnode(MCTSsearch* search, Color nextColor,double policyTemp) :nextColor(nextColor)
{
  sureResult = MC_UNCERTAIN;
  childrennum = 0;
  for (int i = 0; i < MAX_MCTS_CHILDREN; i++)children[i].ptr = NULL;
  visits = 1;

  Hash128 stateHash = search->posHash;
  stateHash ^= NNUEHashTable::ZOBRIST_nextPlayer[nextColor];
  //TODO: hash of search->states
  bool suc = search->hashTable->get(stateHash, *this);
  if (suc)return;//hash matches, skip calculating nnue
  
  search->getGlobalFeatureInput(nextColor);

  NU_Loc *locbuf = search->locbuf;
  PolicyType *pbuf1 = search->pbuf1;
  PolicyType *pbuf2 = search->pbuf2;
  float  *pbuf3 = search->pbuf3;

  //calculate policy
  WRtotal = ValueSum(search->evaluator.evaluateFull(search->gfbuf,nextColor, pbuf1));
  for (NU_Loc loc = 0; loc < MaxBS * MaxBS; loc++)
  {
    if (search->board[loc] != C_EMPTY)
      pbuf1[loc] = MIN_POLICY;
  }

  //policy sort
  std::iota(locbuf, locbuf + MaxBS * MaxBS, 0);
  std::partial_sort(locbuf,locbuf+MAX_MCTS_CHILDREN, locbuf + MaxBS * MaxBS, [&](NU_Loc a, NU_Loc b) {
    return pbuf1[a] > pbuf1[b];
    });

  //count legal moves
  legalChildrennum = MAX_MCTS_CHILDREN;
  for (int i = 0; i < MAX_MCTS_CHILDREN; i++)
  {
    pbuf2[i] = pbuf1[locbuf[i]];
    if (search->board[locbuf[i]] != C_EMPTY)
    {
      legalChildrennum = i;
      break;
    }
  }

  //check draw
  if (legalChildrennum == 0)
  {

    sureResult = MC_DRAW;
    return;
  }


  PolicyType maxRawPolicy = *std::max_element(pbuf2, pbuf2 + legalChildrennum);
  std::transform(pbuf2, pbuf2 + legalChildrennum, pbuf3, [=](auto& p) {
    const double invQ = 1.0 / policyQuantFactor / policyTemp;
    return (float)std::exp((p - maxRawPolicy) * invQ);
    // return (float)std::max(p, PolicyType(0));
    });
  float policySum = std::reduce(pbuf3, pbuf3 + legalChildrennum);
  float k = 1 / policySum;
  std::transform(pbuf3, pbuf3 + legalChildrennum, pbuf3, [=](auto& p) {
    return p * k;
    });

  for (int i = 0; i < legalChildrennum; i++)
  {
    children[i].loc = locbuf[i];
    children[i].policy = uint16_t(pbuf3[i] * policyQuant) + 1;
  }

  search->hashTable->set(stateHash, *this);
}

MCTSnode::MCTSnode(MCTSsureResult sureResult, Color nextColor) :nextColor(nextColor), sureResult(sureResult)
{
  visits = 1;
  WRtotal = sureResultWR(sureResult);
  childrennum = 0;
  legalChildrennum = 0;
}

MCTSnode::~MCTSnode()
{
  for (int i = 0; i < childrennum; i++)
  {
    if (children[i].ptr != NULL)delete children[i].ptr;
  }
}

MCTSsearch::MCTSsearch(const NNUEV2::ModelWeight* w, NNUEHashTable* ht) :MCTSsearch(w, ht, MCTSsearch::Param())
{
}

MCTSsearch::MCTSsearch(const NNUEV2::ModelWeight* w, NNUEHashTable* ht, MCTSsearch::Param p)
    :rootNode(NULL),
    evaluator(w),
    hashTable(ht),
    params(p),
    vcfSolver {{MaxBS, MaxBS, DEFAULT_RULE, C_BLACK}, {MaxBS, MaxBS, DEFAULT_RULE, C_WHITE}}
{
  for (int i = 0; i < MaxBS * MaxBS; i++)
    board[i] = C_EMPTY;
  posHash = Hash128(0, 0);
  vcfSolver[0].setBoard(board, false, true);
  vcfSolver[1].setBoard(board, false, true);
}

float MCTSsearch::fullsearch(Color color, double factor, NU_Loc& bestmove)
{
  vcfSolver[0].setBoard(board, false, true);
  vcfSolver[1].setBoard(board, false, true);

  //check VCF
  VCF_NNUE::SearchResult VCFresult=vcfSolver[color - 1].fullSearch(10000, 10, bestmove, false);
  if (VCFresult == VCF_NNUE::SR_Win)
  {
    //直接vcf，不需要mcts
    return 1;
  }


  if (factor != 0)option.maxNodes = factor;
  terminate = false;

  //if root node is NULL, create root node
  if (rootNode == NULL)
  {
    rootNode=new MCTSnode(this, color,params.policyTemp);
  }
  search(rootNode, option.maxNodes, true);
  bestmove = bestRootMove();
  return getRootValue();
}

void MCTSsearch::play(Color color, NU_Loc loc)
{
    playForSearch(color, loc);


  //树重用

  //if (rootNode != NULL)delete rootNode;
  //rootNode = NULL;
  //return;


  if (rootNode != NULL)
  {
    //VCF没保存后续招法，所以删掉根节点重新计算
    if (rootNode->sureResult != MC_UNCERTAIN) { delete rootNode; rootNode = NULL; }
    //落子方不对，所以重新计算
    else if (rootNode->nextColor != color) { delete rootNode; rootNode = NULL; }
    //检查是否存在对应的子分支
    else
    {
      bool haveThisChild = false;
      for (int i = 0; i < rootNode->childrennum; i++)
      {
        if (rootNode->children[i].loc == loc)
        {
          haveThisChild = true;
          MCTSnode* nextRootNode = rootNode->children[i].ptr;
          rootNode->children[i].ptr = NULL;//防止被rootNode的析构函数删除。
          delete rootNode;
          rootNode = nextRootNode;
          //新的rootNode可能是叶子节点，所以检查一下
          if (rootNode->sureResult != MC_UNCERTAIN) { haveThisChild=false; }
          break;
        }
      }
      if (!haveThisChild)
      {
        delete rootNode;
        rootNode = NULL;
      }
    }
  }
}

void MCTSsearch::undo(NU_Loc loc)
{
    undoForSearch(loc);

  delete rootNode;
  rootNode = NULL;
}

void MCTSsearch::clearBoard()
{
  evaluator.clear();
  for (int i = 0; i < MaxBS * MaxBS; i++)
    board[i] = C_EMPTY;
  posHash = Hash128(0, 0);
  if (rootNode != NULL)delete rootNode;
  rootNode = NULL;
}

void MCTSsearch::setBoard(const Board& board) {
  clearBoard();
  if(board.x_size != MaxBS || board.y_size != MaxBS)
    throw StringError("NNUE only support fixed board size");
  for(int y = 0; y < MaxBS; y++)
    for(int x = 0; x < MaxBS; x++) {
      Color color = board.colors[Location::getLoc(x,y,board.x_size)];
      if(color == C_BLACK || color == C_WHITE)
        play(color, MakeLoc(x, y));
    }
}

void MCTSsearch::loadParamFile(std::string filename)
{
  using namespace std;
  ifstream fs(filename);
  if (!fs.good())return;

  string varname;

  fs >> varname;
  if (varname != "expandFactor") {
    cout << "Wrong parameter name 1:" << varname << endl;
    return;
  }
  fs >> params.expandFactor;


  fs >> varname;
  if (varname != "puct") {
    cout << "Wrong parameter name 2:" << varname << endl;
    return;
  }
  fs >> params.puct;



  fs >> varname;
  if (varname != "puctPow") {
    cout << "Wrong parameter name 3:" << varname << endl;
    return;
  }
  fs >> params.puctPow;


  fs >> varname;
  if (varname != "puctBase") {
    cout << "Wrong parameter name 4:" << varname << endl;
    return;
  }
  fs >> params.puctBase;


  fs >> varname;
  if (varname != "fpuReduction") {
    cout << "Wrong parameter name 5:" << varname << endl;
    return;
  }
  fs >> params.fpuReduction;


  fs >> varname;
  if (varname != "policyTemp") {
    cout << "Wrong parameter name 6:" << varname << endl;
    return;
  }
  fs >> params.policyTemp;
}

void MCTSsearch::playForSearch(Color color, NU_Loc loc)
{
  if (board[loc] != C_EMPTY)
    std::cout << "MCTSsearch: Illegal Move\n";
  board[loc] = color;
  posHash ^= NNUEHashTable::ZOBRIST_loc[color][loc];

  evaluator.play(color, loc);
  vcfSolver[0].playOutside(loc, color, 1, true);
  vcfSolver[1].playOutside(loc, color, 1, true);
}

void MCTSsearch::undoForSearch(NU_Loc loc)
{
  Color color = board[loc];
  if (color == C_EMPTY)
    std::cout << "MCTSsearch: Illegal Undo\n";
  board[loc] = C_EMPTY;

  posHash ^= NNUEHashTable::ZOBRIST_loc[color][loc];

  evaluator.undo(color, loc);
  vcfSolver[0].undoOutside(loc, 1);
  vcfSolver[1].undoOutside(loc, 1);
}

MCTSsearch::SearchResult MCTSsearch::search(MCTSnode* node, uint64_t remainVisits, bool isRoot)
{
  if (remainVisits == 0)remainVisits = INT64_MAX;

  //正常情况是一次playout一个visit，为了降低开销直接让visit变成（1+expandFactor）倍，新增expandFactor倍的visit
  if (!isRoot)remainVisits = std::min(remainVisits, uint64_t(params.expandFactor * double(node->visits))+1);

  SearchResult SR = {0, {0, 0, 0}};

  if (node->sureResult != MC_UNCERTAIN)
  {
    node->visits += remainVisits;
    SR.newVisits = remainVisits;
    SR.WRchange = sureResultWR(node->sureResult) * remainVisits;
    node->WRtotal = node->WRtotal+SR.WRchange;
    return SR;
  }

  Color color = node->nextColor;
  Color opp = getOpp(color);
  while (remainVisits > 0 && !terminate)
  {
    int nextChildID=selectChildIDToSearch(node);
    NU_Loc nextChildLoc = node->children[nextChildID].loc;
    SearchResult childSR;
    if (nextChildID >= node->childrennum)//new child
    {
      node->childrennum++;
      MCTSsureResult sr=checkSureResult(nextChildLoc, color);
      if (sr != MC_UNCERTAIN)node->children[nextChildID].ptr = new MCTSnode(sr, opp);
      else
      {
        playForSearch(color, nextChildLoc);
        node->children[nextChildID].ptr = new MCTSnode(this, opp,params.policyTemp);
        undoForSearch(nextChildLoc);
      }

      childSR.newVisits = 1;
      childSR.WRchange = node->children[nextChildID].ptr->WRtotal;
    }
    else
    {
      playForSearch(color, nextChildLoc);
      childSR = search(node->children[nextChildID].ptr, remainVisits, false);
      undoForSearch(nextChildLoc);
    }
    //update stats
    remainVisits -= childSR.newVisits;
    //std::cout << "debug: " << childSR.newVisits << " " << childSR.WRchange<<"\n";
    node->visits += childSR.newVisits;
    node->WRtotal = node->WRtotal + childSR.WRchange.inverse();
    SR.newVisits += childSR.newVisits;
    SR.WRchange = SR.WRchange + childSR.WRchange.inverse();
  }

  return SR;
}

MCTSsureResult MCTSsearch::checkSureResult(NU_Loc nextMove, Color color)
{
  //return MC_UNCERTAIN;//to disable VCF
  
  //evaluator没有play，因为evaluator play的开销较大
  Color opp = getOpp(color);
  vcfSolver[opp-1].playOutside(nextMove, color, 1, true);
  NU_Loc vcfloc;
  VCF_NNUE::SearchResult sr=vcfSolver[opp - 1].fullSearch(5000, 4, vcfloc, false);
  vcfSolver[opp-1].undoOutside(nextMove, 1);

  if (sr == VCF_NNUE::SR_Win)return MC_Win;
  return MC_UNCERTAIN;

}

int MCTSsearch::selectChildIDToSearch(MCTSnode* node)
{
  int childrennum = node->childrennum;
  if (childrennum == 0)return 0;

  double bestSelectionValue = -1e20;
  int bestChildID = -1;

  double totalVisit = node->visits;
  double puctFactor =
      MCTSpuctFactor(totalVisit, params.puct, params.puctPow, params.puctBase);
  double parentdraw = node->WRtotal.draw / node->visits;
  
  double totalChildPolicy=0;
  for (int i = 0; i < childrennum; i++)
  {
    const MCTSnode* child = node->children[i].ptr;
    double          visit  = child->visits;
    double          value  = -(child->WRtotal.win - child->WRtotal.loss) / visit;
    double          draw   = child->WRtotal.draw / visit;
    double          policy = double(node->children[i].policy) * policyQuantInv;
    totalChildPolicy += policy;

    double selectionValue =
        MCTSselectionValue(puctFactor, value, draw, parentdraw, visit, policy);
    if (selectionValue > bestSelectionValue)
    {
      bestSelectionValue = selectionValue;
      bestChildID = i;
    }


  }

  //check new child
  if(childrennum<node->legalChildrennum)
  {
    double value = (node->WRtotal.win - node->WRtotal.loss) / totalVisit
                   - sqrt(totalChildPolicy) * params.fpuReduction;
    double policy = double(node->children[childrennum].policy) * policyQuantInv;
    double visit = 0;
    double selectionValue =
        MCTSselectionValue(puctFactor, value, parentdraw, parentdraw, visit, policy);
    if (selectionValue > bestSelectionValue)bestChildID = childrennum;
  }

  return bestChildID;


}

NU_Loc MCTSsearch::bestRootMove() const
{
  if (rootNode == NULL)return NU_LOC_NULL;
  if (rootNode->legalChildrennum <= 0)return NU_LOC_NULL;
  if (rootNode->childrennum <= 0)return rootNode->children[0].loc;
  uint64_t bestVisit = 0;
  NU_Loc bestLoc = NU_LOC_NULL;
  for (int i = 0; i < rootNode->childrennum; i++)
  {
    uint64_t visit = rootNode->children[i].ptr->visits;
    if (visit > bestVisit)
    {
      bestVisit = visit;
      bestLoc = rootNode->children[i].loc;
    }
  }
  return bestLoc;
}

float MCTSsearch::getRootValue() const
{
  if (rootNode == NULL)return 0;
  return (rootNode->WRtotal.win - rootNode->WRtotal.loss)/ double(rootNode->visits);
}

int64_t MCTSsearch::getRootVisit() const
{
  if (rootNode == NULL)return 0;
  return rootNode->visits;
}


void MCTSsearch::getGlobalFeatureInput(Color nextPlayer) {
    states.getGlobalFeatureInput_States(gfbuf, nextPlayer);

    for (int i = 3; i < 8; i++)gfbuf[i] = 0;


    //disable gf
    //for (int i = 0; i < NNUEV2::globalFeatureNum; i++)gfbuf[i] = 0;
    //return;


    //VCF::SearchResult myvcf = vcfSolver[nextPlayer - 1].fullSearch(5000, 4, tmp, false);
    //assume nextPlayer has no vcf
    gfbuf[3] = 1;
    gfbuf[4] = 0;

    Color opp = getOpp(nextPlayer);
    NU_Loc tmp;
    VCF_NNUE::SearchResult oppvcf = vcfSolver[opp - 1].fullSearch(5000, 4, tmp, false);
    if (oppvcf == VCF_NNUE::SR_Win)gfbuf[5] = 1;
    else if (oppvcf == VCF_NNUE::SR_Lose)gfbuf[6] = 1;
    else gfbuf[7] = 1;

}