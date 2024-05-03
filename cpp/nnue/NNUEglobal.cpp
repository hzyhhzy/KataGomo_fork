#include "NNUEglobal.h"
namespace NNUE
{

  ValueSum::ValueSum() : win(0), loss(0), draw(0) {}
  ValueSum::ValueSum(double win, double loss, double draw)
    : win(win)
    , loss(loss)
    , draw(draw)
  {}
  ValueSum::ValueSum(ValueType vt) : win(vt.win), loss(vt.loss), draw(vt.draw) {}
  ValueSum ValueSum::inverse() { return ValueSum(loss, win, draw); }
  ValueSum operator+(ValueSum a, ValueSum b)
  {
    return ValueSum(a.win + b.win, a.loss + b.loss, a.draw + b.draw);
  }
  ValueSum operator-(ValueSum a, ValueSum b)
  {
    return ValueSum(a.win - b.win, a.loss - b.loss, a.draw - b.draw);
  }
  ValueSum operator*(ValueSum a, double b)
  {
    return ValueSum(a.win * b, a.loss * b, a.draw * b);
  }
  ValueSum operator*(double b, ValueSum a)
  {
    return ValueSum(a.win * b, a.loss * b, a.draw * b);
  }
}