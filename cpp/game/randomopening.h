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

}
