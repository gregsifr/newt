#ifndef __MARKETIMPACTESTIMATE_H__
#define __MARKETIMPACTESTIMATE_H__


#include <cl-util/factory.h>
using namespace clite::util;

#include "Markets.h"
#include "DataManager.h"
#include "AlphaSignal.h"
#include "ExchangeTracker.h"
#include "SpdTracker.h"
#include "FeeCalc.h"

#include <vector>
using std::vector;

/*
  Estimate of market impact from an order placement or execution.
  We decompose market impact along 2 dimensions:
  - temporary vs permanent.
  - open orders vs filled orders.

  Temporary vs Permanent Market Impact:
  - Temporary market impact is assumed to last a short time, and to diffuse
    away quickly.  The exact impact time-scale of diffusion is not defined for 
    this purpose, but should be on the order of a few order-placemrnt intervals.
  - Permanent market impact is assumed to last a long time (at least until 
    the end of execution of a single customer trade request), and is assumed to
    not diffuse away on the time scale relevant to execution of customer orders.
  This breakdown/distinctions is fairly typical of the optimal execution literature.

  In this class, market impact is measured:
  - In bps, with a positive # indicating px increase, and negative # indicating px decrease.
  - 
*/
class MarketImpactEstimate {
 protected:
  int    _cid;                    // Which stock.
  int    _size;                   // Number of shares.
  Mkt::Trade _dir;                  // Direction that trade is trying to go.  SELL_LONG and SELL_SHORT treated identically.
  bool _goodEstimate;             // Do _temp and _perm estimate components represent valid #s or placeholders?
  double _temporaryImpact;        // Temporary impact, in bps.  Impact of entiree # of shs, not per-share.
  double _permanentImpact;        // Permanent impact, in bps.  Impact of entiree # of shs, not per-share.
 public:
  MarketImpactEstimate();
  virtual ~MarketImpactEstimate();

  double temporaryImpact () const {return _temporaryImpact;}
  double permanentImpact () const {return _permanentImpact;}
  bool goodEstimate() const {return _goodEstimate;}
  int cid() const {return _cid;}
  int size() const {return _size;}
  void setImpact(int cid, int size, Mkt::Trade dir, bool goodEstimate, double ti, double pi);
  void scaleSize(double sizeScale, double tiscale, double piScale);

  int snprint(char *s, int n) const;
};

#endif   // __MARKETIMPACTESTIMATE_H__
