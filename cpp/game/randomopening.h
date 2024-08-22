#pragma once
#include "../search/asyncbot.h"

class Search;

namespace RandomOpening {
  void initializeSpecialOpening(
    Board& board,
    BoardHistory& hist,
    Player& nextPlayer,
    Rand& gameRand);
}
