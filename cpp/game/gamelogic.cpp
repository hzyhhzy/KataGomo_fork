#include "../game/gamelogic.h"

/*
 * gamelogic.cpp
 * Logics of game rules
 * Some other game logics are in board.h/cpp
 *
 * Gomoku as a representive
 */

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

using namespace std;

static int8_t getColor(const int8_t* buf, int xs, int ys, int x, int y) {
  if(x < 0 || x >= xs || y < 0 || y >= ys)
    return 2;
  return buf[x + y * xs];
}

static bool checkConnectionHelper(int8_t* buf, int xs, int ys, int x0, int y0) {
  // 0 empty, 1 pla, 2 opp or marked as jump gap or outside board, 3 top connection, 4 bottom connection
  buf[x0 + y0 * xs] = 3;
  //connect locations
  static const int dxs[7] = {0, 1, 1, 0, -1, -1, 0};
  static const int dys[7] = {-1, -1, 0, 1, 1, 0, -1};
  for (int d = 0; d < 6; d++)
  {
    int x = x0 + dxs[d];
    int y = y0 + dys[d];
    int c = getColor(buf, xs, ys, x, y);
    if(c == 4)
      return true;
    else if(c == 1) {
      bool res = checkConnectionHelper(buf, xs, ys, x, y);
      if(res)
        return true;
    }
  }

  //jump locations
  static const int dxs2[6] = {1, 2, 1, -1, -2, -1};
  static const int dys2[6] = {-2, -1, 1, 2, 1, -1};
  for(int d = 0; d < 6; d++) {
    int x = x0 + dxs2[d];
    int y = y0 + dys2[d];
    int c = getColor(buf, xs, ys, x, y);
    if (c == 4)
    {
      if(
        getColor(buf, xs, ys, x0 + dxs[d], y0 + dys[d]) == 0 &&
        getColor(buf, xs, ys, x0 + dxs[d + 1], y0 + dys[d + 1]) == 0)
        return true;
    }
    else if(c == 1) {
      if (
        getColor(buf, xs, ys, x0 + dxs[d], y0 + dys[d]) == 0 &&
        getColor(buf, xs, ys, x0 + dxs[d + 1], y0 + dys[d + 1]) == 0)
      {
        buf[x0 + dxs[d] + xs * (y0 + dys[d])] = 2;
        buf[x0 + dxs[d + 1] + xs * (y0 + dys[d + 1])] = 2;
        bool res = checkConnectionHelper(buf, xs, ys, x, y);
        if(res)
          return true;
      }

    }
  }
  return false;

}
static bool checkConnection(Player pla, const Board& board)
{
  int xs = board.x_size, ys = board.y_size;
  Player opp = getOpp(pla);
  // firstly, copy the board
  //0 empty, 1 pla, 2 opp or marked as jump gap, 3 top connection, 4 bottom connection
  //always connect at y axis. if white, transpose
  int8_t* buf = new int8_t[xs * ys];
  if(pla == C_BLACK) {
    for(int y = 0; y < ys; y++)
      for(int x = 0; x < xs; x++) {
        Loc loc = Location::getLoc(x, y, xs);
        Color c = board.colors[loc];
        buf[x + y * xs] = c == pla ? 1 : c == opp ? 2 : 0;
      }
  }
  else {
    std::swap(xs, ys);
    for(int y = 0; y < ys; y++)
      for(int x = 0; x < xs; x++) {
        Loc loc = Location::getLoc(y, x, ys);
        Color c = board.colors[loc];
        buf[x + y * xs] = c == pla ? 1 : c == opp ? 2 : 0;
      }
  }
  assert(ys >= 4);
  //mark top connection and bottom connection
  // bottom 1st row
  for(int x = 0; x < xs; x++) {
    if(buf[(ys - 1) * xs + x] == 1)
      buf[(ys - 1) * xs + x] = 4;
  }
  // bottom 2nd row, considering jump connection
  for(int x = 1; x < xs; x++) {
    if(buf[(ys - 2) * xs + x] == 1 && buf[(ys - 1) * xs + x] == 0 && buf[(ys - 1) * xs + x - 1] == 0)
      buf[(ys - 2) * xs + x] = 4;
  }

  //search from top
  bool connected = false;
  // top 1st row
  for(int x = 0; x < xs; x++) {
    if(connected)
      break;
    if(buf[0 * xs + x] == 1)
      connected |= checkConnectionHelper(buf, xs, ys, x, 0);
  }
  // top 2nd row, considering jump connection
  for(int x = 0; x < xs - 1; x++) {
    if(connected)
      break;
    if(buf[1 * xs + x] == 1 && buf[0 * xs + x] == 0 && buf[0 * xs + x + 1] == 0)
      connected |= checkConnectionHelper(buf, xs, ys, x, 1);
  }


  delete buf;
  return connected;
}

void Board::initCaptureTable() {
  //first, dead locations
  //3 basic shapes
  {
    //  x x
    // x . x
    //  # #
    int t[6] = {1, 1, 1, 1, 0, 0};
    // two # loc
    for(int a = 0; a < 3; a++)
      for(int b = 0; b < 3; b++) {
        t[4] = a;
        t[5] = b;
        int id = t[0] + t[1] * 4 + t[2] * 16 + t[3] * 64 + t[4] * 256 + t[5] * 1024;
        CAPTURE_TABLE[id] = 1;
      }
  }
  {
    //  x x
    // x . #
    //  # o
    int t[6] = {1, 1, 1, 0, 2, 0};
    // two # loc
    for(int a = 0; a < 3; a++)
      for(int b = 0; b < 3; b++) {
        t[3] = a;
        t[5] = b;
        int id = t[0] + t[1] * 4 + t[2] * 16 + t[3] * 64 + t[4] * 256 + t[5] * 1024;
        CAPTURE_TABLE[id] = 1;
      }
  }
  {
    //  x #
    // x . o
    //  # o
    int t[6] = {1, 1, 0, 2, 2, 0};
    // two # loc
    for(int a = 0; a < 3; a++)
      for(int b = 0; b < 3; b++) {
        t[2] = a;
        t[5] = b;
        int id = t[0] + t[1] * 4 + t[2] * 16 + t[3] * 64 + t[4] * 256 + t[5] * 1024;
        CAPTURE_TABLE[id] = 1;
      }
  }

  // 6 rotates
  for(int id = 0; id < 4096; id++) {
    if(CAPTURE_TABLE[id] != 1)
      continue;
    int t = id;
    for(int rot = 0; rot < 6; rot++) {
      t = 1024 * (t % 4) + t / 4;
      CAPTURE_TABLE[t] = 1;
    }
  }
  // color inverse
  for(int id = 0; id < 4096; id++) {
    if(CAPTURE_TABLE[id] != 1)
      continue;
    int a = id;

    int t[6];
    for (int i = 0; i < 6; i++)
    {
      int c = a % 4;
      t[i] = c == 1 ? 2 : c == 2 ? 1 : 0;
      a /= 4;
    }
    int id2 = t[0] + t[1] * 4 + t[2] * 16 + t[3] * 64 + t[4] * 256 + t[5] * 1024;
    CAPTURE_TABLE[id2] = 1;
  }

  //next, captured or dominated 
  //replace any stone of dead shapes with C_EMPTY
  
  for(int id = 0; id < 4096; id++) {
    if(CAPTURE_TABLE[id] != 1)
      continue;
    for (int i = 0; i < 6; i++)
    {
      int id2 = id & (~(3 << (2 * i)));//replace each stone with 0
      if(CAPTURE_TABLE[id2] == 0)
        CAPTURE_TABLE[id2] = 2;
    }
  }

  //finally, consider "any" color
  //replace each location with 3

  for(int id = 0; id < 4096; id++) {
    if(CAPTURE_TABLE[id] == 0)
      continue;

    for(int k = 0; k < 64; k++) {
      int m = k;
      int c = 0;
      for (int i = 0; i < 6; i++)
      {
        c *= 4;
        if (m % 2)
        {
          c |= 3;
        }
        m /= 2;
      }
      int id2 = id | c;//replace each loc with 3

      if(CAPTURE_TABLE[id2] == 0)
        CAPTURE_TABLE[id2] = CAPTURE_TABLE[id];
      else if(CAPTURE_TABLE[id2] == 2 && CAPTURE_TABLE[id] == 1)
        CAPTURE_TABLE[id2] = 1;
    }

  }
  IS_CAPTURETABLE_INITALIZED = true;
}

bool Board::isDeadOrCaptured(Loc loc) const {
  assert(IS_CAPTURETABLE_INITALIZED);
  if(!isOnBoard(loc))
    return true;
  if(colors[loc] != C_EMPTY)
    return true;
  // connect locations
  static const int dxs[6] = {0, 1, 1, 0, -1, -1};
  static const int dys[6] = {-1, -1, 0, 1, 1, 0};

  int x0 = Location::getX(loc, x_size);
  int y0 = Location::getY(loc, x_size);

  int surroundings = 0;
  for (int d = 0; d < 6; d++) {
    int x = x0 + dxs[d];
    int y = y0 + dys[d];
    Color c = C_WALL;
    if(x >= 0 && x < x_size && y >= 0 && y < y_size)
      c = colors[Location::getLoc(x, y, x_size)];
    else if(x == -1) {
      if(y >= 1 && y < y_size)
        c = C_WHITE;
      else
        c = C_WALL;
    } 
    else if(x == x_size) {
      if(y >= 0 && y < y_size - 1)
        c = C_WHITE;
      else
        c = C_WALL;
    }
    else if(y == -1) {
      if(x >= 1 && x < x_size)
        c = C_BLACK;
      else
        c = C_WALL;
    } 
    else if(y == y_size) {
      if(x >= 0 && x < x_size - 1)
        c = C_BLACK;
      else
        c = C_WALL;
    }
    else ASSERT_UNREACHABLE;


    surroundings *= 4;
    surroundings += c;
  }

  return CAPTURE_TABLE[surroundings] != 0;

}

Color GameLogic::checkWinnerAfterPlayed(
  const Board& board,
  const BoardHistory& hist,
  Player pla,
  Loc loc) {

  if(checkConnection(pla, board))
    return pla;

  if(loc == Board::PASS_LOC)
    return getOpp(pla);  //pass is not allowed
  

  return C_WALL;
}

GameLogic::ResultsBeforeNN::ResultsBeforeNN() {
  inited = false;
  winner = C_WALL;
  myOnlyLoc = Board::NULL_LOC;
}

void GameLogic::ResultsBeforeNN::init(const Board& board, const BoardHistory& hist, Color nextPlayer) {
  //not used in Hex
  if(inited)
    return;
  inited = true;

  return;
}
