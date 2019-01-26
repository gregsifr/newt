#ifndef __JOINQUEUECOMPONENT_H__
#define __JOINQUEUECOMPONENT_H__

#include <cl-util/factory.h>
using namespace clite::util;

#include "TradeLogicComponent.h"
#include "ExchangeTracker.h"
#include "MarketCapacityModel.h"
#include "DataManager.h"
#include "TradeConstraints.h"
#include "StocksState.h"
#include "ImbTracker.h"

/// some details regarding the last placed order of a particular symbol, to avoid loops of place/pull
struct LastPlacedOrder {
  double  price;
  TimeVal time;

  LastPlacedOrder() : price(-1), time(TimeVal(0)) {}
  inline void update( double price_, TimeVal time_ ) { price = price_; time = time_; }
};

class JoinQueueComponent : public TradeLogicComponent {
 protected:

  factory<StocksState>::pointer      _stocksState;
  factory<ExchangeTracker>::pointer  _exchangeT;
  factory<ImbTracker>::pointer       _imbt;
  factory<TradeConstraints>::pointer _tradeConstraints;
  TAGlobalMarketCapacityModel        _gmcm;  

  // Track the last order we placed for each symbol, to avoid loops of place/pull in various scenarios
  vector<LastPlacedOrder>  _lastPlacedOrders;

  /// Decide whether queue position looks unfavorable - which would trigger a cancel attempt.
  bool queuePositionUnfavorable( const Order *order );

  /// based on GlobalMarketCapacityModel, and also round size for NYSE 
  int calculateOrderSize( int cid, Mkt::Side side, int numShares, ECN::ECN e, double px );

  /// Decide where (which ECN) to route described limit order. Returns ECN::UNKN if no ECN is good
  ECN::ECN chooseECN( int cid, Mkt::Side side, Mkt::Tape t, int size, double px );

 public:  
  JoinQueueComponent();
  virtual ~JoinQueueComponent() {}

  ////////////////////////////////////////
  //  TradeLogicComponent functions:
  ////////////////////////////////////////

  // Suggest a set of order placements for specified stock x side, to be placed on current wakeup.  
  virtual void suggestOrderPlacements( int cid, Mkt::Side side, int tradeLogicID, 
				       int numShares, double priority,
				       vector<OrderPlacementSuggestion> &suggestions );

  /// Should populate <cancelSuggestions>
  virtual void suggestOrderCancels( int cid, unsigned int bidCapacity, unsigned int askCapacity, double priority, 
				    int tradeLogicId, vector<OrderCancelSuggestion> &cancelSuggestions );

  // Decide whether specified Order should be cancelled.
  virtual bool decideToCancel(  int cid, const Order* order, unsigned int capacity, double priority,
				OrderCancelSuggestion::CancelReason& cancelReason );
  /// Expected transaction costs for JQ algo.  
  /// This very simple TLC uses a static (across stocks x situations) estimate based on sim and past live behavior.
  const static double EXPECTED_TC = 0.00010;  // 1.0 bps.

private:
  /// for each tape, this keeps a vector of ECNs sorted by increasing remove-liquidity fees and, as a second key, 
  /// decreasing provide-liquidity-fees
  static vector< vector<ECN::ECN> > _ecnsSortedByFees; // NUM_TAPES * N_ECNS_to_consider
  static bool _ecnsSortedByFeesInitialized;

  void initializeVectorOfSortEcns();
};

#endif   // __JOINQUEUECOMPONENT_H__
