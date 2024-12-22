#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

#include "../game/board.h"

//typedef int8_t         Color;
//static constexpr Color C_EMPTY = 0;
//static constexpr Color C_BLACK = 1;
//static constexpr Color C_WHITE = 2;
//static constexpr Color C_WALL = 3;

//static inline Color getOpp(Color c) { return c ^ 3; }

typedef int16_t NU_Loc;
const int MaxBS = Board::MAX_LEN; // = Board::MAX_LEN when in KataGo

namespace NNUE
{

  static constexpr Color C_MY = 1;
  static constexpr Color C_OPP = 2;
  static constexpr NU_Loc NU_LOC_NULL = -1;
  static constexpr NU_Loc NU_LOC_PASS = MaxBS * MaxBS;

  const int MAX_MCTS_CHILDREN = 64;

  const float policyQuantFactor = 32;


  enum MCTSsureResult : int16_t { MC_Win = 1, MC_LOSE = -1, MC_DRAW = 2, MC_UNCERTAIN = 0 };
  inline NU_Loc MakeLoc(int x, int y) { return NU_Loc(x + y * MaxBS); }
  inline std::string locstr(NU_Loc loc)
  {
    return std::string(1, char('A' + loc % MaxBS)) + std::to_string(int(MaxBS - loc / MaxBS));
  }
  inline std::ostream& operator<<(std::ostream& os, std::vector<NU_Loc> pv)
  {
    for (size_t i = 0; i < pv.size(); i++)
      os << " " + !i << locstr(pv[i]);
    return os;
  }

  typedef int32_t  PolicyType;
  const PolicyType MIN_POLICY = -5e8;
  const PolicyType MYFIVE_POLICY = 1e8;
  const PolicyType OPPFOUR_POLICY = 1e7;
  const PolicyType MYFOUR_POLICY = 1e6;
  const PolicyType VCF_POLICY = 1e5;

  const double WIN_VALUE = 1;
  const double LOSE_VALUE = -1;

  struct ValueType
  {
    float win, loss, draw;
    ValueType(float win, float loss, float draw) : win(win), loss(loss), draw(draw)
    {
      self_softmax();
    }
    void self_softmax()
    {
      // std::cout<<win<<" "<<loss<<" "<<draw << std::endl;
      float maxvalue = std::max(std::max(win, loss), draw);
      win = exp(win - maxvalue);
      loss = exp(loss - maxvalue);
      draw = exp(draw - maxvalue);
      float total = win + loss + draw;
      win = win / total;
      loss = loss / total;
      draw = draw / total;
    }
    float winlossrate() { return win - loss; }
  };

  struct ValueSum
  {
    double win, loss, draw;
    ValueSum();
    ValueSum(double win, double loss, double draw);
    ValueSum(ValueType vt);
    ValueSum inverse();
  };
  ValueSum operator+(ValueSum a, ValueSum b);
  ValueSum operator-(ValueSum a, ValueSum b);
  ValueSum operator*(ValueSum a, double b);
  ValueSum operator*(double b, ValueSum a);


  inline std::string dbg_board(const Color* board)
  {
    std::ostringstream os;
    for (int i = 0; i < MaxBS; i++) {
      for (int j = 0; j < MaxBS; j++) {
        switch (board[j + i * MaxBS]) {
        case C_BLACK: os << "X "; break;
        case C_WHITE: os << "O "; break;
        case C_EMPTY: os << ". "; break;
        }
      }
      os << '\n';
    }

    return os.str();
  }

  inline int64_t now_ms()
  {
    auto dur = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(dur).count();
  }

  const int32_t pow3[] =
  { 1, 3, 9, 27, 81, 243, 729, 2187, 6561, 19683, 59049, 177147, 531441, 1594323, 4782969 };

}