#ifndef PROGRAM_RANDOMOPENING_H_
#define PROGRAM_RANDOMOPENING_H_

#include "../program/play.h"
#include "../search/asyncbot.h"

//This is a grab-bag of various useful higher-level functions that select moves or evaluate the board in various ways.

namespace RandomOpening {

  //random play some black stones until the winrate near 50%
  void getRandomBalanceOpening(Search* bot, Board& board, Player& pla, Rand& rand);

  //some common openings, return false if no available opening for this boardsize
  bool getPredefinedOpening(Board& board, Player& pla, Rand& rand);
  //random move some stones
  void randomizePredefinedOpening(Search* bot, Board& board, Player& pla, Rand& rand);

  //randomly call getRandomBalanceOpening or getPredefinedOpening
  void getOpening(Search* bot, Board& board, Player& pla, double predefinedOpeningProb, Rand& rand);

  //old
  void getRandomInitialOpeningOld(Board& board, Player& pla, Rand& rand);
}


#endif //PROGRAM_RANDOMOPENING_H_
