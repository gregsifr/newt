#ifndef __RISKLIMITS_H__
#define __RISKLIMITS_H__

const static int DefmaxShs=20000;
const static double DefmaxNot=200000.0;

class RiskLimit{
 protected:
  int _maxShares;
  double _maxNotional;
 public:
  RiskLimit(int maxShs,double maxNot):
    _maxShares(maxShs),
    _maxNotional(maxNot)
    {}
  RiskLimit():
    _maxShares(DefmaxShs),
    _maxNotional(DefmaxNot)
    {}
  void setMaxNotional(double maxn){ _maxNotional = maxn; }
  void setMaxShs(int maxs) { _maxShares = maxs; }
  int maxShares() { return _maxShares;}
  double maxNotional() { return _maxNotional;}
};

#endif
