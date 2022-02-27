#include "VCFsolver.h"
#include <random>
#include <iostream>
#include <cassert>
using namespace std;

const Hash128 VCFsolver::zob_plaWhite = Hash128(0xb6f9e465597a77eeULL, 0xf1d583d960a4ce7fULL);
const Hash128 VCFsolver::zob_plaBlack = Hash128(0x853E097C279EBF4EULL, 0xE3153DEF9E14A62CULL);
Hash128 VCFsolver::zob_board[2][sz][sz]; 
#ifdef FORGOMOCUP
VCFHashTable VCFsolver::hashtable(20, 2);
uint64_t VCFsolver::MAXNODE = 50000;
#else
VCFHashTable VCFsolver::hashtable(27, 21);
uint64_t VCFsolver::MAXNODE = 100000;
#endif


uint64_t VCFsolver::totalAborted;
uint64_t VCFsolver::totalSolved;
uint64_t VCFsolver::totalnodenum;

inline bool resultNotSure(int32_t result)
{
  return result <= 0 && result != -10000;
}


void VCFsolver::init()
{
  totalSolved = 0;
  totalnodenum = 0;
  mt19937_64 rand(0);
  for (int i = 0; i < 2; i++)
    for (int j = 0; j < sz; j++)
      for (int k = 0; k < sz; k++)
        zob_board[i][j][k] = Hash128(rand(), rand());
}
uint32_t VCFsolver::findEmptyPos(int t, int y, int x)
{
  uint32_t pos = 0;
  switch (t)
  { 
    case 0:
    {
      for (int i = 0; i < 5; i++)
      {
        if (board[y][x+i] == 0)
        {
          pos = pos << 16;
          pos = pos | (sz*y + (x+i));
        }
      }
      return pos;
    }
    case 1:
    {
      for (int i = 0; i < 5; i++)
      {
        if (board[y+i][x] == 0)
        {
          pos = pos << 16;
          pos = pos | (sz*(y+i) + x );
        }
      }
      return pos;
    }
    case 2:
    {
      for (int i = 0; i < 5; i++)
      {
        if (board[y + i][x+i] == 0)
        {
          pos = pos << 16;
          pos = pos | (sz*(y + i) + (x+i));
        }
      }
      return pos;
    }
    case 3:
    {
      for (int i = 0; i < 5; i++)
      {
        if (board[y - i][x + i] == 0)
        {
          pos = pos << 16;
          pos = pos | (sz*(y - i) + (x + i));
        }
      }
      return pos;
    }
    default:return 0;

  }
}

uint32_t VCFsolver::findDefendPosOfFive(int y, int x)
{
  uint32_t emptypos = -1;
  auto check = [&](int t, int y, int x)
  {
    if (mystonecount[t][y][x] == 4 && oppstonecount[t][y][x]%6 == 0 )emptypos= findEmptyPos(t, y, x);
    
  };

  //x
  for (int i = 0; i < 5; i++)
  {
    int x1 = x - i;
    if (x1 < 0)break;
    if (x1 >= xsize - 4)continue;
    check(0, y, x1);
    if (emptypos != -1)return emptypos;
  }
  //y
  for (int i = 0; i < 5; i++)
  {
    int y1 = y - i;
    if (y1 < 0)break;
    if (y1 >= ysize - 4)continue;
    check(1, y1, x);
    if (emptypos != -1)return emptypos;
  }
  //+x+y
  for (int i = 0; i < 5; i++)
  {
    int x1 = x - i;
    int y1 = y - i;
    if (x1 < 0)break;
    if (x1 >= xsize - 4)continue;
    if (y1 < 0)break;
    if (y1 >= ysize - 4)continue;
    check(2, y1, x1);
    if (emptypos != -1)return emptypos;
  }
  //+x+y
  for (int i = 0; i < 5; i++)
  {
    int x1 = x - i;
    int y1 = y + i;
    if (x1 < 0)break;
    if (x1 >= xsize - 4)continue;
    if (y1 >= ysize)break;
    if (y1 < 4)continue;
    check(3, y1, x1);
    if (emptypos != -1)return emptypos;
  }
  return -1;
}

void VCFsolver::addNeighborSix(int y, int x, uint8_t pla,int factor)
{
  //cout << int(pla);
  if (pla == 0)return;
  auto stonecount = (pla == C_MY) ? mystonecount: oppstonecount;
  //8 directions
  int x1, y1,t;

  //+x
  t = 0;
  x1 = x + 1;
  y1 = y ;
  if (x1 < xsize - 4)
    stonecount[t][y1][x1] +=factor;

  //-x
  t = 0;
  x1 = x - 5;
  y1 = y;
  if (x1 >= 0)
    stonecount[t][y1][x1] += factor;

  //+y
  t = 1;
  x1 = x;
  y1 = y+1;
  if (y1 < ysize - 4)
    stonecount[t][y1][x1] += factor;

  //-y
  t = 1;
  x1 = x;
  y1 = y -5;
  if (y1>=0)
    stonecount[t][y1][x1] += factor;

  //+x+y
  t = 2;
  x1 = x+1;
  y1 = y + 1;
  if (x1 < xsize - 4&& y1 < ysize - 4)
    stonecount[t][y1][x1] += factor;

  //-x-y
  t = 2;
  x1 = x-5;
  y1 = y -5;
  if (x1 >= 0&& y1 >= 0)
    stonecount[t][y1][x1] += factor;

  //+x-y
  t = 3;
  x1 = x + 1;
  y1 = y - 1;
  if (x1 < xsize - 4 && y1 >= 4)
    stonecount[t][y1][x1] += factor;

  //-x+y
  t = 3;
  x1 = x -5;
  y1 = y +5;
  if (x1>=0 && y1 <ysize)
    stonecount[t][y1][x1] += factor;
}

void VCFsolver::solve(const Board& kataboard, uint8_t pla, uint8_t& res, uint16_t& loc)
{
  if (zob_board[0][0][0].hash0 == 0)cout << "VCFSolver::zob_board not init";
  int32_t result=setBoard(kataboard,pla); 
  if (resultNotSure(result))
  {
    auto resultAndLoc = hashtable.get(boardhash);
    result = resultAndLoc & 0xFFFFFFFF;
    rootresultpos = resultAndLoc >> 32;
  }
  if(resultNotSure(result))result=solveIter(true);
  if (nodenum >= MAXNODE && result <= 0)res = 3;
  else if (result > 0)res = 1;
  else if (result == -10000)res = 2;
  else
  {
    cout << "No result!";
  }
  int x = rootresultpos % sz, y = rootresultpos / sz;
  loc = x + 1 + (y + 1) * (xsize + 1);


  //some debug information
  totalnodenum += nodenum;
  totalSolved++;

  if (nodenum > 50000)
  {
  //  cout << "nodenum " << nodenum << "  res " << int(res) << endl;
 //   print();
  }
  if (nodenum >= MAXNODE)
  {
    totalAborted++;
//    cout << "Hit vcf upper bound: nodenum=" << nodenum << endl;
    //print();
  }
  if(totalSolved%100000==0)
    {
  //   cout << "  totalSolved " << totalSolved << "  totalNode " << totalnodenum <<  "  totalAborted " << totalAborted << endl;
    }
}

inline void printnum2(int n)
{
  char c1, c2;
  c1 = n / 10;
  c2 = n % 10;
  if (c1 == 0)cout << " ";
  else cout << char(c1 + '0');
  cout<< char(c2 + '0');
}


void VCFsolver::print()
{
  cout << "  ";
  for (int i = 0; i < xsize; i++)printnum2(i);
  cout << endl;
  for (int y = 0; y < ysize; y++)
  {
    printnum2(y);
    cout << " ";
    for (int x = 0; x < xsize; x++)
    {
      auto c = board[y][x];
      if (c == 0)cout << ". ";
      else if (c == 1)cout << "x ";
      else if (c == 2)cout << "o ";
    }
    cout << endl;
  }
  cout << endl;
}
void VCFsolver::printRoot()
{
  cout << "  ";
  for (int i = 0; i < xsize; i++)printnum2(i);
  cout << endl;
  for (int y = 0; y < ysize; y++)
  {
    printnum2(y);
    cout << " ";
    for (int x = 0; x < xsize; x++)
    {
      auto c = rootboard[y][x];
      if (c == 0)cout << ". ";
      else if (c == 1)cout << "x ";
      else if (c == 2)cout << "o ";
    }
    cout << endl;
  }
  cout << endl;
}

int32_t VCFsolver::setBoard(const Board& b, uint8_t pla)
{
  xsize = b.x_size;
  ysize = b.y_size;
  if(rules.basicRule==Rules::BASICRULE_RENJU)
    forbiddenSide = (pla == C_BLACK) ? C_MY : C_OPP;//如果自己是黑棋则为1，否则为2
  movenum = 0;
  bestmovenum = 10000;
  nodenum = 0;
  threeCount = 0;
  oppFourPos = -1;
  boardhash = Rules::ZOBRIST_BASIC_RULE_HASH[rules.basicRule];
  if (pla == C_WHITE)boardhash ^= zob_plaWhite;
  else boardhash ^= zob_plaBlack;

  int32_t result = 0;

  //clear
  for (int i = 0; i < 4; i++)
    for (int j = 0; j < sz; j++)
      for (int k = 0; k < sz; k++)
      {
        mystonecount[i][j][k] = 0;
        oppstonecount[i][j][k] = 0;
      }
  for (int y = 0; y < ysize; y++)
    for (int x = 0; x < xsize; x++)
    {
      short loc = (x + 1) + (y + 1)*(xsize + 1);
      auto c = b.colors[loc];

      if (c == 0)board[y][x] = 0;
      else if (c == pla)
      {
        movenum++;
        board[y][x] = C_MY;
        boardhash ^= zob_board[0][y][x];
        if(rules.basicRule==Rules::BASICRULE_STANDARD ||
          (rules.basicRule==Rules::BASICRULE_RENJU && forbiddenSide== C_MY))
            addNeighborSix(y, x, C_MY, 6);
      }
      else
      {
        board[y][x] = C_OPP;
        boardhash ^= zob_board[1][y][x];
        if(rules.basicRule==Rules::BASICRULE_STANDARD ||
          (rules.basicRule==Rules::BASICRULE_RENJU && forbiddenSide== C_OPP))
          addNeighborSix(y, x, C_OPP, 6);
      }
      rootboard[y][x] = board[y][x];
    }

  //count stone num

  //x
  for (int y = 0; y < ysize; y++)
    for (int x = 0; x < xsize-4; x++)
    {
      int mycount = 0, oppcount = 0;
      for (int i = 0; i < 5; i++)
      {
        auto c = board[y][x + i];
        if (c == C_MY)mycount++;
        else if (c == C_OPP)oppcount++;
      }
      mystonecount[0][y][x] += mycount;
      oppstonecount[0][y][x] += oppcount;
    }
  //y
  for (int x = 0; x < xsize; x++)
    for (int y = 0; y < ysize-4; y++)
    {
      int mycount = 0, oppcount = 0;
      for (int i = 0; i < 5; i++)
      {
        auto c = board[y+i][x];
        if (c == C_MY)mycount++;
        else if (c == C_OPP)oppcount++;
      }
      mystonecount[1][y][x] += mycount;
      oppstonecount[1][y][x] += oppcount;
    }
  //+x+y
  for (int x = 0; x < xsize-4; x++)
    for (int y = 0; y < ysize - 4; y++)
    {
      int mycount = 0, oppcount = 0;
      for (int i = 0; i < 5; i++)
      {
        auto c = board[y + i][x+i];
        if (c == C_MY)mycount++;
        else if (c == C_OPP)oppcount++;
      }
      mystonecount[2][y][x] += mycount;
      oppstonecount[2][y][x] += oppcount;
    }
  //+x-y
  for (int x = 0; x < xsize - 4; x++)
    for (int y = 4; y < ysize; y++)
    {
      int mycount = 0, oppcount = 0;
      for (int i = 0; i < 5; i++)
      {
        auto c = board[y - i][x + i];
        if (c == C_MY)mycount++;
        else if (c == C_OPP)oppcount++;
      }
      mystonecount[3][y][x] += mycount;
      oppstonecount[3][y][x] += oppcount;
    }

  

  for (int t = 0; t < 4; t++)
    for (int y = 0; y < ysize; y++)
      for (int x = 0; x < xsize; x++)
      {
        int my = mystonecount[t][y][x];
        int opp = oppstonecount[t][y][x];
        if (my == 5 || opp == 5)
        {
          cout << "Why you give a finished board here\n";
          print();
          return 0;
        }
        if (my == 4&& opp%6 == 0)
        {
          rootresultpos = findEmptyPos(t, y, x);
          result = 10000-1-movenum;
          return result;
        }
        if (my % 6 == 0&& opp ==4)
        {
          if(oppFourPos==-1)oppFourPos = findEmptyPos(t, y, x);
          else
          {
            auto anotherOppFourPos = findEmptyPos(t, y, x);
            if(anotherOppFourPos!= oppFourPos)result = -10000;//对手双四了，但不能直接return，因为自己有可能直接连五
          }
          continue;
        }
        if (my == 3 && opp%6 == 0)//眠三
        {
          uint64_t locs= findEmptyPos(t, y, x);
          uint64_t threeEntry = (uint64_t(uint64_t(t)*sz*sz + uint64_t(y) * sz + x) << 32) | locs;
          threes[threeCount] = threeEntry;
          threeCount++;
        }
      }
  return result;
}

int32_t VCFsolver::play(int x, int y, uint8_t pla, bool updateHash)
{
  board[y][x] = pla;
  if(updateHash)boardhash ^= zob_board[pla-1][y][x];

  //带有_forRenju后缀的变量，保证只在renju规则下使用

  bool isPlaForbidden_forRenju = rules.basicRule==Rules::BASICRULE_RENJU && forbiddenSide== pla; //only for renju
  if(rules.basicRule==Rules::BASICRULE_STANDARD ||
    isPlaForbidden_forRenju)
    addNeighborSix(y, x, pla, 6);

  int32_t result = 0;

  if (pla == C_MY)
  {

    //不需要考虑连五解禁，因为提前一步已经判断出来胜负了
    //检查双四的方法：如果发现两个连五点，活四当且仅当两个点的距离为[0,+-5],[+-5,0],[+-5,+-5]
    //活四的下一手一定可以胜，因为连五没禁手。当然，代码层面已经排除了对手提前一步冲四
    //如果发现更多连五点，一定是双四禁手
    bool lifeFour_forRenju = false;
    bool isForbidden_forRenju = false;

    //活三判据：在同一条线上同时产生连续两个或者三个三。之后对交点进行判断是否禁手，如果一条线产生的1或2个活四点都不是禁手说明是活三
    int8_t threeCountDir_forRenju[4] = {0, 0, 0, 0};//每个方向的新3个数,大于等于2说明可能有活三
   // int16_t maybeLifeFourPoses[4][3] ;//所有可能的下一手的活四点
    //第一个维度是4个方向，第二个维度的第一个数是活四点个数1或2，第二，三个是活四点

    movenum++;
    int32_t fourPos =-1;//这一步棋形成的冲四的防守点
    bool winThisMove = false;//无禁手双四
  //  cout << "s\n";
    auto addandcheck = [&](int t, int y, int x)
    {
      mystonecount[t][y][x]++;
      auto msc = mystonecount[t][y][x];
   //   cout << int(msc) << endl;

      if (isPlaForbidden_forRenju)
      {
        if (msc > 5 && msc % 6 == 5)isForbidden_forRenju = true;//长连
      }
      if (oppstonecount[t][y][x]%6 != 0|| msc <= 2 || msc>5)return;
      if (msc == 3)
      {

        if (isPlaForbidden_forRenju)
        {
          threeCountDir_forRenju[t]++;
        }
        uint64_t locs = findEmptyPos(t, y, x);
        uint64_t threeEntry = (uint64_t(uint64_t(t) * sz * sz + uint64_t(y) * sz + x) << 32) | locs;
        threes[threeCount] =threeEntry;
        threeCount++;
      }
      else if (msc == 4)
      {

        if (fourPos == -1)
        {
          fourPos = findEmptyPos(t, y, x);
          //todo white renju,check if black can't play at "fourPos" 
        }
        else
        {
          auto anotherFourPos = findEmptyPos(t, y, x);
          if (anotherFourPos != fourPos)
          {
            //检查是否是活四
            if (isPlaForbidden_forRenju)
            {
              if (lifeFour_forRenju)isForbidden_forRenju = true;
              else
              {
                int x1 = fourPos % sz, x2 = anotherFourPos % sz, y1 = fourPos / sz, y2 = anotherFourPos / sz;
                int dx = x1 - x2, dy = y1 - y2;
                if ((dx == 0 || dx == 5 || dx == -5) && (dy == 0 || dy == 5 || dy == -5))lifeFour_forRenju = true;
                else isForbidden_forRenju = true;
              }
            }

            if (!isForbidden_forRenju)
            {
                winThisMove = true;//这个是无禁的双四
            }
          }
        }
      }
      else /*if (mystonecount[t][y][x] == 5)*/
      {
        cout << "how can you reach here 3" << endl;
        print();
        printRoot();
        result = 1;
      }
    };

    //x
    //todo standard renju: add 6 
    for (int i = 0; i < 5; i++)
    {
      int x1 = x - i;
      if (x1 < 0)break;
      if (x1>=xsize-4)continue;
      addandcheck(0, y, x1);
    }
    //y
    for (int i = 0; i < 5; i++)
    {
      int y1 = y - i;
      if (y1 < 0)break;
      if (y1 >= ysize - 4)continue;
      addandcheck(1, y1, x);
    }
    //+x+y
    for (int i = 0; i < 5; i++)
    {
      int x1 = x - i;
      int y1 = y - i;
      if (x1 < 0)break;
      if (x1 >= xsize - 4)continue;
      if (y1 < 0)break;
      if (y1 >= ysize - 4)continue;
      addandcheck(2, y1, x1);
    }
    //+x+y
    for (int i = 0; i < 5; i++)
    {
      int x1 = x - i;
      int y1 = y + i;
      if (x1 < 0)break;
      if (x1 >= xsize - 4)continue;
      if (y1 >= ysize)break;
      if (y1 < 4)continue;
      addandcheck(3, y1, x1);
    }

    //todo renju:check if is forbidden
    //只检查33,因为 44 长连都检查了
    if (isPlaForbidden_forRenju&&(!isForbidden_forRenju))
    {
      int maybeLife3 = 0;
      for (int i = 0; i < 4; i++)
      {
        if (threeCountDir_forRenju[i] >= 2)maybeLife3++;
      }
      if (maybeLife3 >= 2)
      {
        int life3 = 0;
        for (int i = 0; i < 4; i++)
        {
          if (checkLife3(y,x,i))life3++;
        }
        if (life3 >= 2)isForbidden_forRenju = true;
      }
    }
    winThisMove = winThisMove && (!isForbidden_forRenju);

    if (winThisMove)
    {
      result = 10000 - movenum - 1;//双四或活四
      if (bestmovenum > movenum + 1)
      {
        bestmovenum = movenum + 1;
      }
      else if (bestmovenum != movenum + 1)
      {
        cout << "how can you reach here 4\n";
      }
    }
    
    if (fourPos==-1)//这手棋不是冲四,只可能是暂时挡对手冲四才可能到这里
    {
      if (oppFourPos == -1)
      {
        cout << "how can you reach here 1  " <<x<<" "<<y<< endl;
        print();
      }
      result = -10000;
    }
    if (isForbidden_forRenju)
      result = -10000;
  }
  else if (pla == C_OPP)
  {
  //不需要考虑连五解禁，因为提前一步已经判断出来胜负了
  //检查双四的方法：如果发现两个连五点，活四当且仅当两个点的距离为[0,+-5],[+-5,0],[+-5,+-5]
  //活四的下一手一定可以胜，因为连五没禁手。当然，代码层面已经排除了对手提前一步冲四
  //如果发现更多连五点，一定是双四禁手
  bool lifeFour_forRenju = false;
  bool isForbidden_forRenju = false;

  //活三判据：在同一条线上同时产生连续两个或者三个三。之后对交点进行判断是否禁手，如果一条线产生的1或2个活四点都不是禁手说明是活三
  int8_t threeCountDir_forRenju[4] = { 0, 0, 0, 0 };//每个方向的新3个数,大于等于2说明可能有活三
 // int16_t maybeLifeFourPoses[4][3] ;//所有可能的下一手的活四点
  //第一个维度是4个方向，第二个维度的第一个数是活四点个数1或2，第二，三个是活四点

    //todo renju:check if is forbidden
    oppFourPos = -1;//对手冲四直接记录在oppFourPos里

    bool winThisMove = false;//无禁手双四

    auto addandcheck = [&](int t, int y, int x)
    {
      oppstonecount[t][y][x]++;
      auto osc = oppstonecount[t][y][x];

      if (isPlaForbidden_forRenju)
      {
        if (osc > 5 && osc % 6 == 5)isForbidden_forRenju = true;//长连
      }
      if (mystonecount[t][y][x]%6 != 0 || osc < 3 || osc >5)return;//无威胁
      if (osc == 3)
      {

        if (isPlaForbidden_forRenju)
        {
          threeCountDir_forRenju[t]++;
        }
      }
      else if (osc == 4)
      {
        if (oppFourPos == -1)oppFourPos = findEmptyPos(t, y, x);
        else
        {
          auto anotherFourPos = findEmptyPos(t, y, x);
          if (anotherFourPos != oppFourPos)
          {
            //检查是否是活四
            if (isPlaForbidden_forRenju)
            {
              if (lifeFour_forRenju)isForbidden_forRenju = true;
              else
              {
                int x1 = oppFourPos % sz, x2 = anotherFourPos % sz, y1 = oppFourPos / sz, y2 = anotherFourPos / sz;
                int dx = x1 - x2, dy = y1 - y2;
                if ((dx == 0 || dx == 5 || dx == -5) && (dy == 0 || dy == 5 || dy == -5))lifeFour_forRenju = true;
                else isForbidden_forRenju = true;
              }
            }

            if (!isForbidden_forRenju)
            {
              winThisMove = true;//这个是无禁的双四
            }


          }
        }
      }
      else// if (oppstonecount[t][y][x] == 5)
      {
        cout << "how can you reach here 2" << endl;
        result = 2;
      }
    };

    //x
    //todo standard renju: add 6 
    for (int i = 0; i < 5; i++)
    {
      int x1 = x - i;
      if (x1 < 0)break;
      if (x1 >= xsize - 4)continue;
      addandcheck(0, y, x1);
    }
    //y
    for (int i = 0; i < 5; i++)
    {
      int y1 = y - i;
      if (y1 < 0)break;
      if (y1 >= ysize - 4)continue;
      addandcheck(1, y1, x);
    }
    //+x+y
    for (int i = 0; i < 5; i++)
    {
      int x1 = x - i;
      int y1 = y - i;
      if (x1 < 0)break;
      if (y1 < 0)break;
      if (x1 >= xsize - 4)continue;
      if (y1 >= ysize - 4)continue;
      addandcheck(2, y1, x1);
    }
    //+x+y
    for (int i = 0; i < 5; i++)
    {
      int x1 = x - i;
      int y1 = y + i;
      if (x1 < 0)break;
      if (y1 >= ysize)break;
      if (x1 >= xsize - 4)continue;
      if (y1 < 4)continue;
      addandcheck(3, y1, x1);
    }

    //todo renju:check if is forbidden
    //只检查33,因为 44 长连都检查了
    if (isPlaForbidden_forRenju && (!isForbidden_forRenju))
    {
      int maybeLife3 = 0;
      for (int i = 0; i < 4; i++)
      {
        if (threeCountDir_forRenju[i] >= 2)maybeLife3++;
      }
      if (maybeLife3 >= 2)
      {
        int life3 = 0;
        for (int i = 0; i < 4; i++)
        {
          if (checkLife3(y, x, i))life3++;
        }
        if (life3 >= 2)isForbidden_forRenju = true;
      }
    }
    winThisMove = winThisMove && (!isForbidden_forRenju);

    if (winThisMove)
    {
      result = -10000;
    }

    if (isForbidden_forRenju)//抓禁成功
    {
      result = 10000 - movenum - 1;//双四或活四
      if (bestmovenum > movenum + 1)
      {
        bestmovenum = movenum + 1;
      }
      else if (bestmovenum != movenum + 1)
      {
        cout << "how can you reach here 5\n";
      }
    }
  }
  return result;
}

void VCFsolver::undo(int x, int y, int64_t oppFourPos1, uint64_t threeCount1, bool updateHash)
{
  threeCount = threeCount1;
  oppFourPos = oppFourPos1;//这两个没必要传进来，可以原地修改，但是怕忘
  //result = 0;
  auto pla = board[y][x];
  board[y][x] = 0;


  bool isPlaForbidden_forRenju = rules.basicRule==Rules::BASICRULE_RENJU && forbiddenSide== pla; //only for renju
  if(rules.basicRule==Rules::BASICRULE_STANDARD ||
      isPlaForbidden_forRenju)
    addNeighborSix(y, x, pla, -6);

  if (updateHash) boardhash ^= zob_board[pla - 1][y][x];

  //这地方代码复用很差劲，如果要改，需要改一大堆
  if (pla == C_MY)
  {
    movenum--;
    //x
    //todo standard renju:-6 
    for (int i = 0; i < 5; i++)
    {
      int x1 = x - i;
      if (x1 < 0)break;
      if (x1 >= xsize - 4)continue;
      mystonecount[0][y][x1]--;
    }
    //y
    for (int i = 0; i < 5; i++)
    {
      int y1 = y - i;
      if (y1 < 0)break;
      if (y1 >= ysize - 4)continue;
      mystonecount[1][y1][x]--;
    }
    //+x+y
    for (int i = 0; i < 5; i++)
    {
      int x1 = x - i;
      int y1 = y - i;
      if (x1 < 0)break;
      if (y1 < 0)break;
      if (x1 >= xsize - 4)continue;
      if (y1 >= ysize - 4)continue;
      mystonecount[2][y1][x1]--;
    }
    //+x+y
    for (int i = 0; i < 5; i++)
    {
      int x1 = x - i;
      int y1 = y + i;
      if (x1 < 0)break;
      if (y1 >= ysize)break;
      if (x1 >= xsize - 4)continue;
      if (y1 < 4)continue;
      mystonecount[3][y1][x1]--;
    }
  }
  else if (pla == C_OPP)
  {
    //x
    //todo standard renju:-6 
    for (int i = 0; i < 5; i++)
    {
      int x1 = x - i;
      if (x1 < 0)break;
      if (x1 >= xsize - 4)continue;
      oppstonecount[0][y][x1]--;
    }
    //y
    for (int i = 0; i < 5; i++)
    {
      int y1 = y - i;
      if (y1 < 0)break;
      if (y1 >= ysize - 4)continue;
      oppstonecount[1][y1][x]--;
    }
    //+x+y
    for (int i = 0; i < 5; i++)
    {
      int x1 = x - i;
      int y1 = y - i;
      if (x1 < 0)break;
      if (y1 < 0)break;
      if (x1 >= xsize - 4)continue;
      if (y1 >= ysize - 4)continue;
      oppstonecount[2][y1][x1]--;
    }
    //+x+y
    for (int i = 0; i < 5; i++)
    {
      int x1 = x - i;
      int y1 = y + i;
      if (x1 < 0)break;
      if (y1 >= ysize)break;
      if (x1 >= xsize - 4)continue;
      if (y1 < 4)continue;
      oppstonecount[3][y1][x1]--;
    }
  }
  else
  {
    cout << "bug";
  }
}


int32_t VCFsolver::solveIter(bool isRoot)
{
  //查hash表在落子之后计算stonecount之前，不在这里
  nodenum++;
  if (nodenum>= MAXNODE)
  {
    return -(movenum+1);
  }

  int32_t maxMovenum = bestmovenum - 1;
  if (movenum + 2 > maxMovenum)return -(movenum + 2);//这一手肯定无法连五，那至少movenum+2手

  //先备份
  auto oppFourPos_old = oppFourPos;
  auto threeCount_old = threeCount;

  //先看看对手上一步是不是冲四
  if (oppFourPos != -1)
  {
    int solutionPos = -1;
    int x = oppFourPos % sz, y = oppFourPos / sz;
    int32_t result=play(x,y, C_MY,true);
    if (resultNotSure(result) )//说明自己的防守也是冲四,可以继续计算。不是双四，所以至少还需要movenum+2手
    {
        auto defendPos = findDefendPosOfFive(y, x);
        result = play(defendPos % sz, defendPos / sz, C_OPP, true);
        if (result == 0)result = solveIter(false);
        undo(defendPos % sz, defendPos / sz, oppFourPos_old, threeCount_old, true);
    }
    undo(x, y, oppFourPos_old, threeCount_old, true);
    //if (resultNotSure(result))cout << "bug: no result";
    if (result >0)//vcf成功
    {
      solutionPos = oppFourPos;
      if (isRoot)rootresultpos = oppFourPos;
    }

    //存hash表
    hashtable.set(boardhash, (int64_t(solutionPos) << 32) | int64_t(result));

    return result;
  }

  //否则是对手没有冲四的情况
  int solutionPos = -1;
  int bestresult = -10000;
  for (int threeID = threeCount - 1; threeID >= 0; threeID--)
  {

    auto threeEntry = threes[threeID];
    uint16_t pos1 = threeEntry & 0xFFFF, pos2 = (threeEntry >> 16) & 0xFFFF;
    threeEntry = threeEntry >> 32;
    int t = threeEntry/(sz*sz);
    threeEntry = threeEntry % (sz * sz);
    int y = threeEntry / sz;
    int x = threeEntry % sz;

    if (oppstonecount[t][y][x]%6 != 0 || mystonecount[t][y][x] != 3)continue;//这个眠三已经失效

    auto playandcalculate = [&](uint16_t posMy, uint16_t posOpp)
    {
      uint8_t x1 = posMy % sz, y1 = posMy / sz, x2 = posOpp % sz, y2 = posOpp / sz;
      auto hashchange = zob_board[0][y1][x1] ^ zob_board[1][y2][x2];
      boardhash ^= hashchange;

      int32_t result = 0;
      bool shouldCalculate = true;


      if (shouldCalculate)
      {
        result = play(x1, y1, C_MY,false);

        if (resultNotSure(result))
        {
            result = play(x2, y2, C_OPP, false);
            if (resultNotSure(result))
            {
              auto resultAndLoc = hashtable.get(boardhash);
              result = resultAndLoc & 0xFFFFFFFF;
              if (resultNotSure(result))
                result = solveIter(false);
            }
            undo(x2, y2, oppFourPos_old, threeCount_old, false);
          
        }
        undo(x1, y1, oppFourPos_old, threeCount_old, false);
        if (result == 0)cout << "Result=0";
      }
      boardhash ^= hashchange;
      if (result > bestresult)
      {
        bestresult = result;
        solutionPos = posMy;
      }
    };

    playandcalculate(pos1, pos2);
    if (bestresult >= 10000 - movenum - 2)break;//如果已经发现双四，则无需考虑其他走法
    playandcalculate(pos2, pos1);
    if (bestresult >= 10000 - movenum - 2)break;//如果已经发现双四，则无需考虑其他走法
  }
  hashtable.set(boardhash, (int64_t(solutionPos) << 32) | int64_t(bestresult));
  if (isRoot)rootresultpos = solutionPos;
  //cout << isRoot << " " << int(solutionPos) << endl;
  return bestresult;

}

bool VCFsolver::isForbiddenMove(int y, int x,bool fiveForbidden)//检查禁手
{
  if (board[y][x] != 0)return false;
  auto p = forbiddenSide;
  auto bstonecount = (p == C_MY) ? mystonecount : oppstonecount;
  auto wstonecount = (p == C_MY) ?oppstonecount: mystonecount ;
  board[y][x] = p;
  addNeighborSix(y, x, p, 6);

  bool isForbidden = false;

  bool five = false;
  bool lifeFour = false;

    //活三判据：在同一条线上同时产生连续两个或者三个三。之后对交点进行判断是否禁手，如果一条线产生的1或2个活四点都不是禁手说明是活三
    int8_t threeCountDir[4] = { 0, 0, 0, 0 };//每个方向的新3个数,大于等于2说明可能有活三

    int32_t fourPos = -1;//这一步棋形成的冲四的防守点
    auto addandcheck = [&](int t, int y, int x)
    {
      bstonecount[t][y][x]++;
      auto bsc = bstonecount[t][y][x];
     // cout <<int(bsc)<<" ";
        if (bsc > 5 && bsc % 6 == 5)isForbidden = true;//长连
      if (wstonecount[t][y][x] % 6 != 0 || bsc <= 2 || bsc > 5)return;
      if (bsc == 3)
      {
          threeCountDir[t]++;
      }
      else if (bsc == 4)
      {
        if (fourPos == -1)
        {
          fourPos = findEmptyPos(t, y, x);
        }
        else
        {
          auto anotherFourPos = findEmptyPos(t, y, x);
          if (anotherFourPos != fourPos)
          {
            //检查是否是活四
              if (lifeFour)isForbidden = true;
              else
              {
                int x1 = fourPos % sz, x2 = anotherFourPos % sz, y1 = fourPos / sz, y2 = anotherFourPos / sz;
                int dx = x1 - x2, dy = y1 - y2;
                if ((dx == 0 || dx == 5 || dx == -5) && (dy == 0 || dy == 5 || dy == -5))lifeFour = true;
                else isForbidden = true;
              }
            }

        }
      }
      else /*if (mystonecount[t][y][x] == 5)*/
      {
        five = true;
       // cout << x<<y<<"how can you reach here 6" << endl;
      //  print();
      }
    };

    //x
    for (int i = 0; i < 5; i++)
    {
      int x1 = x - i;
      if (x1 < 0)break;
      if (x1 >= xsize - 4)continue;
      addandcheck(0, y, x1);
    }
    //y
    for (int i = 0; i < 5; i++)
    {
      int y1 = y - i;
      if (y1 < 0)break;
      if (y1 >= ysize - 4)continue;
      addandcheck(1, y1, x);
    }
    //+x+y
    for (int i = 0; i < 5; i++)
    {
      int x1 = x - i;
      int y1 = y - i;
      if (x1 < 0)break;
      if (x1 >= xsize - 4)continue;
      if (y1 < 0)break;
      if (y1 >= ysize - 4)continue;
      addandcheck(2, y1, x1);
    }
    //+x+y
    for (int i = 0; i < 5; i++)
    {
      int x1 = x - i;
      int y1 = y + i;
      if (x1 < 0)break;
      if (x1 >= xsize - 4)continue;
      if (y1 >= ysize)break;
      if (y1 < 4)continue;
      addandcheck(3, y1, x1);
    }

    //只检查33,因为 44 长连都检查了
    if (!isForbidden)
    {
      int maybeLife3 = 0;
      for (int i = 0; i < 4; i++)
      {
        if (threeCountDir[i] >= 2)maybeLife3++;
      }
      if (maybeLife3 >= 2)
      {
        int life3 = 0;
        for (int i = 0; i < 4; i++)
        {
          if (checkLife3(y, x, i))life3++;
        }
        if (life3 >= 2)isForbidden = true;
      }
    }

    //恢复原样
    board[y][x] = 0;
    addNeighborSix(y, x, p, -6);
    auto subandcheck = [&](int t, int y, int x)
    {
      bstonecount[t][y][x]--;
    };

    //x
    for (int i = 0; i < 5; i++)
    {
      int x1 = x - i;
      if (x1 < 0)break;
      if (x1 >= xsize - 4)continue;
      subandcheck(0, y, x1);
    }
    //y
    for (int i = 0; i < 5; i++)
    {
      int y1 = y - i;
      if (y1 < 0)break;
      if (y1 >= ysize - 4)continue;
      subandcheck(1, y1, x);
    }
    //+x+y
    for (int i = 0; i < 5; i++)
    {
      int x1 = x - i;
      int y1 = y - i;
      if (x1 < 0)break;
      if (x1 >= xsize - 4)continue;
      if (y1 < 0)break;
      if (y1 >=ysize - 4)continue;
      subandcheck(2, y1, x1);
    }
    //+x+y
    for (int i = 0; i < 5; i++)
    {
      int x1 = x - i;
      int y1 = y + i;
      if (x1 < 0)break;
      if (x1 >= xsize - 4)continue;
      if (y1 >= ysize)break;
      if (y1 < 4)continue;
      subandcheck(3, y1, x1);
    }


    if (five)isForbidden = fiveForbidden;
    //注：活三判断的时候调用这个函数，fiveForbidden=true。连五当然不是禁手，但是这个函数只会在活三判断中调用，活三下一手是活四，如果活四同时生成五则不是活四，把连五当做禁手刚好可以让活四的判断变成正确的


   // print();
  //  cout << y << " " << x << " " << isForbidden << endl;
      return isForbidden;


}
bool VCFsolver::checkLife3(int y, int x, int t)//检查是否是活三
{
  //前提是已经落子并且stonecount计数了
  auto p = forbiddenSide;
  auto stonecount = (p == C_MY) ? mystonecount : oppstonecount;
  auto ostonecount = (p == C_MY) ? oppstonecount:mystonecount  ;
  int16_t pos1 = -1, pos2 = -1;//两个活四点，如果只有0个或者1个则用-1填充

  int ct=0;//出现了连续几个眠三，ct=1则无活四，ct=2则1个活四点，ct=3则两个活四点
  int ctstart=-1;//从第几个五元组开始连续出现眠三
  //根据t和stonecount找活四点

  switch (t)
  {

  case 0:
    //x
    for (int i = 0; i < 5; i++)
    {
      int y1 = y;
      int x1 = x - i;
      if (x1 < 0)break;
      if (x1 >= xsize - 4)continue;
      //以下这段复制4遍
      int sc = stonecount[t][y1][x1];
      int osc = ostonecount[t][y1][x1];
      if (sc == 3&&(osc%6==0))
      {
        ct++;
        if (ctstart == -1)
        {
          ctstart = i;
        }
        
      }
      else
      {
        if (ct >= 2)break;//找到连续的3了
        if (ct == 1)//孤立的一个3，忽略
        {
          ct = 0;
          ctstart = -1;
        }
      }
    }
    if (ct == 2)//跳活三或者隔一个被堵住的活三
    {
      //+xx+x+或+x+xx+或o+xxx++
      for (int i = 0; i < 4; i++)
      {
        int y1 = y ;
        int x1 = x - ctstart + i;
        if (board[y1][x1] == 0)
        {
          pos1 = y1 * sz + x1;
          break;
        }
      }
      if (pos1 == -1)
      {
        cout << "cant find 3\n";
        print();
      }
    }
    else if (ct == 3)//连着的活三
    {
      //+xxx+
      pos1 = (y)*sz + (x - ctstart + 3);
      pos2 = (y)*sz + (x - ctstart -1);
    }


    break;
  case 1:
    //y
    for (int i = 0; i < 5; i++)
    {
      int x1 = x;
      int y1 = y - i;
      if (y1 < 0)break;
      if (y1 >= ysize - 4)continue;
      int sc = stonecount[t][y1][x1];
      int osc = ostonecount[t][y1][x1];
      if (sc == 3 && (osc % 6 == 0))
      {
        ct++;
        if (ctstart == -1)
        {
          ctstart = i;
        }

      }
      else
      {
        if (ct >= 2)break;//找到连续的3了
        if (ct == 1)//孤立的一个3，忽略
        {
          ct = 0;
          ctstart = -1;
        }
      }
    }
    if (ct == 2)//跳活三或者隔一个被堵住的活三
    {
      //+xx+x+或+x+xx+或o+xxx++
      for (int i = 0; i < 4; i++)
      {
        int y1 = y - ctstart + i;
        int x1 = x;
        if (board[y1][x1] == 0)
        {
          pos1 = y1 * sz + x1;
          break;
        }
      }
      if(pos1==-1)
      {
        cout << "cant find 3\n";
        print();
      }
    }
    else if (ct == 3)//连着的活三
    {
      //+xxx+
      pos1 = (y - ctstart + 3)*sz + (x);
      pos2 = (y - ctstart - 1)*sz + (x );
    }

    break;
  case 2:
    //+x+y
    for (int i = 0; i < 5; i++)
    {
      int x1 = x - i;
      int y1 = y - i;
      if (x1 < 0)break;
      if (x1 >= xsize - 4)continue;
      if (y1 < 0)break;
      if (y1 >= ysize - 4)continue;
      int sc = stonecount[t][y1][x1];
      int osc = ostonecount[t][y1][x1];
      if (sc == 3 && (osc % 6 == 0))
      {
        ct++;
        if (ctstart == -1)
        {
          ctstart = i;
        }

      }
      else
      {
        if (ct >= 2)break;//找到连续的3了
        if (ct == 1)//孤立的一个3，忽略
        {
          ct = 0;
          ctstart = -1;
        }
      }
    }
    if (ct == 2)//跳活三或者隔一个被堵住的活三
    {
      //+xx+x+或+x+xx+或o+xxx++
      for (int i = 0; i < 4; i++)
      {
        int y1 = y - ctstart + i;
        int x1 = x - ctstart + i;
        if (board[y1][x1] == 0)
        {
          pos1 = y1 * sz + x1;
          break;
        }
      }
      if (pos1 == -1)
      {
        cout << "cant find 3\n";
        print();
      }
    }
    else if (ct == 3)//连着的活三
    {
      //+xxx+
      pos1 = (y - ctstart + 3) * sz + (x - ctstart + 3);
      pos2 = (y - ctstart - 1) * sz + (x - ctstart - 1);
    }

    break;
  case 3:
    //+x+y
    for (int i = 0; i < 5; i++)
    {
      int x1 = x - i;
      int y1 = y + i;
      if (x1 < 0)break;
      if (x1 >= xsize - 4)continue;
      if (y1 >= ysize)break;
      if (y1 < 4)continue;
      int sc = stonecount[t][y1][x1];
      int osc = ostonecount[t][y1][x1];
      if (sc == 3 && (osc % 6 == 0))
      {
        ct++;
        if (ctstart == -1)
        {
          ctstart = i;
        }

      }
      else
      {
        if (ct >= 2)break;//找到连续的3了
        if (ct == 1)//孤立的一个3，忽略
        {
          ct = 0;
          ctstart = -1;
        }
      }
    }
    if (ct == 2)//跳活三或者隔一个被堵住的活三
    {
      //+xx+x+或+x+xx+或o+xxx++
      for (int i = 0; i < 4; i++)
      {
        int y1 = y + ctstart - i;
        int x1 = x - ctstart + i;
        if (board[y1][x1] == 0)
        {
          pos1 = y1 * sz + x1;
          break;
        }
      }
      if (pos1 == -1)
      {
        cout << "cant find 3\n";
        print();
      }
    }
    else if (ct == 3)//连着的活三
    {
      //+xxx+
      pos1 = (y + ctstart - 3) * sz + (x - ctstart + 3);
      pos2 = (y + ctstart + 1) * sz + (x - ctstart - 1);
    }

    break;

  default:
    cout << "bug";
  }

  

  if (pos1 != -1 && (!isForbiddenMove(pos1 / sz, pos1 % sz,true)))return true;
    if (pos2 != -1 && (!isForbiddenMove(pos2 / sz, pos2 % sz, true)))return true;

    return false;
}
void VCFsolver::printForbiddenMap()
{
  cout << "  ";
  for (int i = 0; i < xsize; i++)printnum2(i);
  cout << endl;
  for (int y = 0; y < ysize; y++)
  {
    printnum2(y);
    cout << " ";
    for (int x = 0; x < xsize; x++)
    {
      auto c = board[y][x];
      if (isForbiddenMove(y, x))cout << "& ";
      else if (c == 0)cout << ". ";
      else if (c == 1)cout << "x ";
      else if (c == 2)cout << "o ";
    }
    cout << endl;
  }
  cout << endl;
}