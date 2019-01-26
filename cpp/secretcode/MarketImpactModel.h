/*
  MarketImpactModel.h

  Models for estimating market impact of various trading activities. 

  Standard optimal scheduling / optimal execution literature postulates a simple
  market impact model in which:
  - Temporary market impact is a function of trading rate (participation rate * vol * k),
    but is assumed to be independent of exactly how orders are placed in the market.
  - Permanent market impact is invariant of trading rate, and of how orders are placed in
    the market.

  For large orders, permanent market impact can be the largest component of total 
  transaction costs, so the applicability of these assumptions can have a major impact
  on trading decisions.

  This market-impact code attempts a more sophisticated / complicated / (hopefully) realistic 
  market impact model in which:
  - Market impact is a function functions of exactly how/when orders
    orders are placed, pulled, filled, left out, etc., not generically of volume traded
    (independent of how that volume is traded).
  - Different ways of trading the same number of shares may thus have different market impacts.  
    In particular, permanent market impact is not assumed to be invariant of scheduling, and
    of how orders are placed. 
    
  This market impact model should probably be tested against the simpler (standard-assumption)
  market-impact model to determine whether the additional complexity is warranted.
*/

#ifndef __MARKETIMPACTMODEL_H__
#define __MARKETIMPACTMODEL_H__


#include <cl-util/factory.h>
using namespace clite::util;

#include "DataManager.h"
#include "AlphaSignal.h"
#include "ExchangeTracker.h"
#include "SpdTracker.h"
#include "FeeCalc.h"

#include <vector>
using std::vector;

#include "MarketImpactEstimate.h"
#include "TradingImpactModel.h"

class BasicKFRTSignal;
class ETFKFRTSignal;

/*
  Base-class/interface for market-impact model in which market impact calculations are 
    based on how/when trades are placed in market, as opposed to generic functions of 
    trading volume or trading rate.
  Conceptually, it would seem that the way in which orders are placed should have
    an impact on market impact.  E.g. buying 1 mm shs MSFT by crossing the whole thing at
    once should have a different market impact than scalping in.

  In theory, for this type of market impact model, we could define market impact functions
    for all of the major events in an orders (potential) life cycle, e.g.:
  - the market impact of placing an order.
  - the market impact that the order has while outstanding.
  - the market impact of cancelling an order.
  - the market impact of an order being filled.

  In practice, in the initial version of the framework, we assume that only two of the 
    above types of events have a marjet impact that should be worried about:
  - While outstanding (non-IOC) orders have a temporary market impact, based on how much
    they move stock fv by their presence.
    - These orders are assumed to have 0 permanent market impact if not filled.
    - IOC orders are assumed to have 0 temporary and 0 permenant market impact if not filled.
  - When filled, orders are assumed to have both a temporary and a permanent market impact.
    - The permanent market impact is then defined as the long-term change in market-estimated
      fair-value given that the order was filled.
    - The temporary market impact is then defined as the concession (or benefit) of fill price
      vs fair-value when the order was executed.

  This breakdown/distinction seems logical but is not standard from the literature,
    and should be empirically verified, especially:
  - zero impact from order placement (as opposed to while outstanding, or if filled).
  - unfilled orders having zero permanent market impact,
    - for IOC orders.
    - and for non-IOC (displayed) orders.

  Notes:
  - Market impact estimates should be absolutely signed quantities, aka
    a positive number implies a positive expected return on the stock from 
    the specified action, a negative number implies a negative expected
    return on the stock from the specified action.
*/
class MarketImpactModel {

 public:
  MarketImpactModel();
  virtual ~MarketImpactModel();

  //
  // Interface definition:
  //


  // Estimating impact that order has, while outstanding.  
  // - Applies to non-IOC orders only.
  // - Temporary component is assumed to last as long as order is outstanding.
  // - Permanent component is assumed to be zero.
  // - This impact is assumed to include/subsume any market impact from placing the order.
  virtual bool marketImpactOutstanding(int cid, ECN::ECN ecn, int size, double price,
				       Mkt::Side side, int timeout, bool invisible, MarketImpactEstimate &fv);

  // Estimate the market impact that *filling* the specified order would have.
  // This impact is assumed to happen *only after* the order is filled, aka it does not include
  //   the market impact while the order is oustanding.
  // For non IOC orders, this impact is assumed to include/subsume any market impact from placing the order or having 
  //   it outstanding.
  virtual bool marketImpactFill(int cid, ECN::ECN ecn, int size, double price,
				       Mkt::Side side, int timeout, bool invisible, MarketImpactEstimate &fv);
};



/*
  1st attempt at a specific market impact model for liquidity taking.
  Designed to be used as a true *order* impact model that maps orders --> impact,
    not an execution quality model.
  Uses fv-change from RT-signal on proposed trade to guesstimate market impact
    of fills.
  Notes:  
  - Current version of TakeLiquidityMarketImpactModel assumes that ETFKFRTSignal
    is favored KFRT signal variant for production use....
*/
class TakeLiquidityMarketImpactModel : public MarketImpactModel {
  factory<ETFKFRTSignal>::pointer _sig;
 public:
  TakeLiquidityMarketImpactModel();
  virtual ~TakeLiquidityMarketImpactModel();

  // Return false, aka does not apply.
  virtual bool marketImpactOutstanding(int cid, ECN::ECN ecn, int size, double price,
				       Mkt::Side side, int timeout, bool invisible, MarketImpactEstimate &fv);

  // Estimate the market impact that *filling* the specified order would have.
  // This impact is assumed to happen *only after* the order is filled, aka it does not include
  //   the market impact while the order is oustanding.
  virtual bool marketImpactFill(int cid, ECN::ECN ecn, int size, double price,
				       Mkt::Side side, int timeout, bool invisible, MarketImpactEstimate &fv);
};


#endif   // __MARKETIMPACTMODEL_H__
