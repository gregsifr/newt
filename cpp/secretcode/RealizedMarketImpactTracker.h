/*
  RealizedMarketImpactTracker.h

  Widget that tracks realized market impact for liquidity-taking orders.
*/


#ifndef __REALIZEDMARKETIMPACT_H__
#define __REALIZEDMARKETIMPACT_H__

#include "DataManager.h"
#include "TradeLogicComponent.h"
#include "JoinQueueComponent.h"
#include "FollowableInvisOrderComponent.h"
#include "FollowLeaderJoinQueueBackup.h"
#include "CrossComponent.h"

#include "TakeInvisibleComponent.h"
#include "AlphaSignal.h"

#include <vector>
using std::vector;

/*
  Simple widget that keeps track of realized market-impact per trade-request, aka
    market impact for orders that are actually filled.
  Notes:
  - Keep running estimate of market-impact (per stock) for liquidity taking orders
    only.
  - Does not keep estimates of market-impact for liquidity providing orders, as the 
    current market impact model for liquidity proviing orders is based on aggregate
    volume traded (and potentially scheduling), not on individual fills.
  - Periodically dumps heartbeat message with summary info on running totals of 
    these values.
  - Share and impact estimates should be *absolutely signed* quantities, not signed
    relative to trade direction - e.g. -200 for SELL 200 shares, +300 for BUY 300 shares.
  Basic algorithm / internal-workings:
  - Listens to bcast of OrderPlacementSuggestions.
    - These are assumed to include martket impact estimate (for liquidity-taking orders).
    - Keeps map from orderID --> OrderPlacementSuggestion.
    - As component is only designed to care about liquidity-taking orders, can flush this
      map on fairly-frequent timer to keep it from filling up.. 
*/
class RealizedMarketImpactTracker : public OrderHandler::listener,   // listens to OrderUpdates to catch fills.
  public TradeRequestsHandler::listener,  // listens to TradeRequests to know whwn to print summary info & clear totals.
  public TimeHandler::listener            // periodically prints summary info on open orders.
{
 protected:
  /*
    Internal State.
  */
  factory<DataManager>::pointer         _dm;
  factory<debug_stream>::pointer        _ddebug;
  factory<TakeLiquidityMarketImpactModel>::pointer _tlMIM;  // Used to guesstimate market-impact of liquidity taking. 
  factory<CentralOrderRepo>::pointer    _orderRepo;      // Used to map from orderID --> order details.   
  int                                   _printMS;        // How frequently to print summary info, in MS.
  bool                                  _marketOpen;     // Is market currently open for normal trading session?
  TimeVal                               _lastPrintTV;    // Time as of last suammry printing.
  // Shares traded, in current request (per stock):
  // - deliberately taking liquidity.
  // - deliberarely providing liquidity.
  // - trying to provide liquidity, but probably accidentally taking liquidity.
  vector<int>                           _shsTL;      
  vector<int>                           _shsPL;   
  vector<int>                           _shsAccTL;   

  // Accumulated market impact, in current trade-request:
  // - from orders that deliberately took liquidity.
  // - from orders that were intended to take provide liquidity, but
  //   seem to have accidentally taken liquidity.
  vector<double>                        _impactTL;
  vector<double>                        _impactAccTL;

  // Last received trade-request, per stock.
  vector<TradeRequest>                  _lastTRV;

  /*
    Non-public member functions.
  */
  void clear(int cid);  // Clear state for specified stock.
  void printState();
  void printStockState(int cid, string &type);
  virtual void onMarketOpen(const TimeUpdate &au);
  virtual void onMarketClose(const TimeUpdate &au);
  virtual void onTimerUpdate(const TimeUpdate &au);
  bool extractOrderInfo(const OrderUpdate &ou, double &fillMI, Liq::Liq &fillLT);
 public:
  
  RealizedMarketImpactTracker();
  virtual ~RealizedMarketImpactTracker();

  /*
    UpdateListener functions.
  */
  virtual void update (const OrderUpdate &ops);
  virtual void update (const TradeRequest &tr);
  virtual void update (const TimeUpdate &au);

  /*
    Accessor functions....
  */
  int shsTL(int cid) const                {return _shsTL[cid];}
  int shsPL(int cid) const                {return _shsPL[cid];}
  int shsAccTL(int cid) const             {return _shsAccTL[cid];}
  double impactTL(int cid) const          {return _impactTL[cid];}
  double impactAccTL(int cid) const       {return _impactAccTL[cid];}
  TradeRequest lastTR(int cid) const      {return _lastTRV[cid];}
};

#endif  // __REALIZEDMARKETIMPACT_H__
