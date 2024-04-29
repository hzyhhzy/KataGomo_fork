#pragma once
#include "../search/asyncbot.h"

class Search;

namespace RandomOpening {
  void initializeBalancedRandomOpening(
    Search* botB,
    Search* botW,
    Board& board,
    BoardHistory& hist,
    Player& nextPlayer,
    Rand& gameRand,
    bool forSelfplay);

  void initializeSpecialOpening(
    Search* botB,
    Search* botW,
    Board& board,
    BoardHistory& hist,
    Player& nextPlayer,
    Rand& gameRand);
  void initializeCompletelyRandomOpening(Board& board, BoardHistory& hist, Player& nextPlayer, Rand& gameRand, double areaPropAvg);

  
  void randomFillBoard(Board& board, Rand& gameRand, double bProb, double wProb);
}
