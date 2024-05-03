#pragma once
#include "../NNUEglobal.h"
#include "../HashTable/VCFHashTable.h"
static_assert(NNUE::NU_LOC_NULL >= (MaxBS + 6)* (MaxBS + 6)+1||NNUE::NU_LOC_NULL<0);//保证loc_null不是棋盘上的点

namespace VCF_NNUE {

  struct alignas(int64_t) PT
  {
    int16_t shapeDir;
    NU_Loc shapeLoc,loc1, loc2;
    PT():shapeDir(0), shapeLoc(NNUE::NU_LOC_NULL),loc1(NNUE::NU_LOC_NULL),loc2(NNUE::NU_LOC_NULL){}
    //PT(NU_Loc loc1,NU_Loc loc2):loc1(loc1),loc2(loc2){}
  };

  struct zobristTable
  {
    Hash128 boardhash[2][(MaxBS + 6) * (MaxBS + 6)];
    Hash128 isWhite;
    Hash128 basicRuleHash[3];
    zobristTable(int64_t seed);
  };

  enum PlayResult : int16_t {
    PR_Win,                  //双四或者连五或者抓禁
    PR_OneFourWithThree,     //单个冲四同时生成眠三
    PR_OneFourWithTwo,     //单个冲四同时生成眠二，不生成眠三
    PR_OneFourWithoutTwo,  //单个冲四同时不生成眠二和三
    PR_Lose                  //不合法(被抓禁)或者没有生成冲四
  };
  //无眠三扣NoThreeDecrease分，无眠二扣NoTwoDecrease分
  //初始分是InitialBound，之后每次搜索加BoundIncrease分
  static const int NormalDecrease = 1;
  static const int NoThreeDecrease = 5;
  static const int NoTwoDecrease = 20;

  //fullsearch中第n次搜索使用的bound
  static inline int bound_n(int n)
  {
    if (n <= 2)return n;
    else return 2 + 6 * (n - 2);
  }
  //fullsearch中第n次搜索使用的searchFactor
  static inline float searchFactor(int n)
  {
    if (n <= 2)return 10000;
    else return pow(0.6,n-2);
  }

  enum SearchResult : int16_t {
    SR_Win = -1,//有解
    SR_Uncertain = 0,//完全不知道有没有解，还没搜
    SR_Lose = 32767//确定无解
    //其他值代表暂时无解，若SearchResult=n，说明“允许n-1次无新眠三的冲四无解”
  };


  inline NU_Loc xytoshapeloc(int x, int y) { return NU_Loc((MaxBS + 6) * (y + 3) + x + 3); }
  static const NU_Loc dirs[4] = { 1, MaxBS + 6, MaxBS + 6 + 1, -MaxBS - 6 + 1 };//+x +y +x+y +x-y
  //为了方便常量调用，分开再写一遍
  static const NU_Loc dir0 = 1;
  static const NU_Loc dir1 =  MaxBS + 6;
  static const NU_Loc dir2 =  MaxBS + 6 + 1;
  static const NU_Loc dir3 = -MaxBS - 6 + 1;
  static const int ArrSize = (MaxBS + 6) * (MaxBS + 6);//考虑额外3圈后的棋盘格点数



}  // namespace VCF

class VCFsolver_NNUE
{

  //hashTable
  static VCFHashTable_NNUE hashtable;
  static VCF_NNUE::zobristTable zobrist;


public:
  const int H, W;
  const bool isWhite;//如果进攻方是黑棋，则false，进攻方是白棋则true。若true，color全是反向
  const int basicRule;

  Color board[(MaxBS + 6) * (MaxBS + 6)];  //预留3圈
  // shape=1*己方棋子+8*长连+64*对方棋子+512*对手长连+4096*出界
  int16_t shape[4][(MaxBS + 6) * (MaxBS + 6)];  //预留3圈
  Hash128 boardHash;

  int     ptNum;             // pts里面前多少个有效
  VCF_NNUE::PT pts[8 * MaxBS * MaxBS];  //眠三

  int64_t nodeNumThisSearch;//记录这次搜索已经搜索了多少节点了，用来提前终止，每次fullSearch开始时清零
  int movenum;//手数，从开始vcf到现在的局面已经多少手了
  NU_Loc PV[MaxBS * MaxBS];//记录路线
  int PVlen;

  VCFsolver_NNUE() :VCFsolver_NNUE(C_BLACK,NNUE::DEFAULT_RULE) {}
  VCFsolver_NNUE(int basicRule,Color pla) : VCFsolver_NNUE(MaxBS, MaxBS, basicRule, pla) {}
  VCFsolver_NNUE(int h, int w, int basicRule, Color pla);
  void reset();

  //两种board
  //b是外部的棋盘，pla是进攻方
  //katagoType是否是katago的棋盘，false对应loc=x+y*MaxBS，true对应loc=x+1+(y+1)*(W+1)
  //colorType是源棋盘的表达形式，=false 己方对方，=true 黑色白色
  void setBoard(const Color* b, bool katagoType, bool colorType);


  VCF_NNUE::SearchResult fullSearch(float factor,int maxLayer, NU_Loc& bestmove, bool katagoType);//factor是搜索因数，保证factor正比于节点数。
  inline int getPVlen() { return PVlen; };
  std::vector<NU_Loc> getPV();//比较慢
  std::vector<NU_Loc> getPVreduced();//进攻方的PV

  
  //用于外部调用，更新棋盘。保证shape正确，不保证pts正确。
  //locType=0 vcf内部loc格式，=1 x+y*MaxBS，=2 katago格式
  //colorType=false 己方对方，=true 黑色白色
  void playOutside(NU_Loc loc, Color color, int locType,bool colorType);
  void undoOutside(NU_Loc loc, int locType);//用于外部调用，更新棋盘。保证shape正确，不保证pts正确。

  //debug
  void printboard();

private:

  //重置pts，完整搜索前一定调用这个。
  //同时检查己方，对方有没有连五冲四
  VCF_NNUE::SearchResult resetPts(NU_Loc& onlyLoc);

  //找两个空点
  VCF_NNUE::PT findEmptyPoint2(NU_Loc loc,int dir);
  //找一个空点
  NU_Loc findEmptyPoint1(NU_Loc loc,NU_Loc bias);//bias=dirs[dir]

  //走一个回合，先冲四再防守
  //loc1是攻，loc2是守，nextForceLoc是白棋冲四
  VCF_NNUE::PlayResult playTwo(NU_Loc loc1,NU_Loc loc2, NU_Loc& nextForceLoc);
  void undo(NU_Loc loc);
  //maxNoThree：最多maxNoThree步没有新眠三，挡对手反四的冲四不算
  //forceLoc：下一步必须走这里，因为白棋有冲四
  //winLoc：返回获胜点
  //ptNumOld：上一步活三个数，搜索之后还原
  VCF_NNUE::SearchResult search(int maxNoThree, NU_Loc forceLoc);

  // shape=1*己方棋子+8*长连+64*对方棋子+512*对手长连+4096*出界
  inline bool shape_isMyFive(int16_t s) { return (s & 0170777) == 5; }
  inline bool shape_isMyFour(int16_t s) { return (s & 0170777) == 4; }
  inline bool shape_isMyThree(int16_t s) { return (s & 0170777) == 3; }
  inline bool shape_isMyTwo(int16_t s) { return (s & 0170777) == 2; }
  inline bool shape_isOppFive(int16_t s) { return (s & 0177707) == 5*64; }
  inline bool shape_isOppFour(int16_t s) { return (s & 0177707) == 4*64; }



};
