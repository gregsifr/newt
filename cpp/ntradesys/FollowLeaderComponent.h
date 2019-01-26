#ifndef __FOLLOWLEADERTRACKER_H__
#define __FOLLOWLEADERTRACKER_H__

#include <vector>
using std::vector;

#include <bitset>
using namespace std;

#include <cl-util/factory.h>
using namespace clite::util;

#include "DataManager.h"
#include "TradeLogicComponent.h"
#include "MarketCapacityModel.h"
#include "MaxQueueSizeTracker.h"
#include "TradeConstraints.h"
#include "StocksState.h"
#include <UCS1.h>

/*
  Simple widget that keeps track of appearence of visible "leader" orders
    (ones that step-up/are more aggressive than previous CBBO).
  FollowLeaderComponent is abstract base class.  It:
  - Provides some default functionality for deciding when to follow leader orders, which subclasses may want to override.
  - Omits functionality for deciding when to pull orders, which subclasses will need to provide.

  Extension to multiple ECNs (July 2009):
  - FTL strategy is applied on ISLD, ARCA, BATS, EDGA (any of those ECNs to which program is listening).
  - Step-up is done on same exchange on which leader order is posted. (there's some fixed priority if more than one)
*/
class FollowLeaderComponent : public TradeLogicComponent {
 protected:
  factory<StocksState>::pointer      _stocksState;
  factory<TradeConstraints>::pointer _tradeConstraints;
  GlobalMarketCapacityModel *_gmcm;  
  //virtual const OrderPlacementSuggestion::PlacementReason _defaultReason;

  /*
    Non-public Member Functions.
  */
  ECN::ECN chooseECN( int cid, Mkt::Side side,  int tradeLogicID, 
		      int numShares, double priority, double loPrice, 
		      bitset<ECN::ECN_size> &ecns );

  int calculateOrderSize( int cid, Mkt::Side side, int numShares, ECN::ECN e, double px );

  virtual double expectedTC() = 0;
 public:
  FollowLeaderComponent();
  virtual ~FollowLeaderComponent();

  ///////////////////////
  // TradeLogicComponent functions.
  ///////////////////////

  // Suggest a set of order placements for specified stock x side, to be placed on current wakeup.  
  virtual void suggestOrderPlacements( int cid, Mkt::Side side, int tradeLogicID, 
				       int numShares, double priority,
				       vector<OrderPlacementSuggestion> &suggestions );

  /// Should populate <cancelSuggestions>
  virtual void suggestOrderCancels( int cid, unsigned int bidCapacity, unsigned int askCapacity, double priority, 
				    int tradeLogicId, vector<OrderCancelSuggestion> &cancelSuggestions );

  // Decide whether to cancel a specific order.
  // Called by suggestOrderCancels(...)
  virtual bool decideToCancel( int cid, const Order* order, unsigned int capacity, double priority,
				OrderCancelSuggestion::CancelReason& cancelReason ) = 0;

  ///////////////////////
  //  Non-generic interface functions.  Specific to FollowLeader type signals.
  ///////////////////////

  // Did we see any followable "leader" orders for stock x side since the last wakeup?
  // If so, fills in *followablePx with price of that order.
  bool anyFollowableLeaders( int cid, Mkt::Side side, bitset<ECN::ECN_size> &ecns, double *followablePx );
};


/*
  Initial concrete implementation of FollowLeaderComponent, which uses
    some simple but rough heurisics to decide when to pull, namely:
  - Pull when the original leader order pulls.
  - Pull if our position in the queue starts looking unfavorable, as
    defined by some simple heuristics.
*/
class FollowLeaderComponentKH : public FollowLeaderComponent {
  
  factory<UCS1>::pointer _fvSignal;
  factory<MaxQueueSizeTracker>::pointer  _maxQSizeT;
  factory<debug_stream>::pointer         _logPrinter;
  //virtual const OrderPlacementSuggestion::PlacementReason _defaultReason;
 public:
  FollowLeaderComponentKH();
  virtual ~FollowLeaderComponentKH() {};  

  virtual bool queuePositionUnfavorable( const Order* order );

  // Decide whether to cancel a specific order.
  // Called by suggestOrderCancels(...)
  virtual bool decideToCancel( int cid, const Order* order, unsigned int capacity, double priority,
				OrderCancelSuggestion::CancelReason& cancelReason );

  virtual double expectedTC() {return EXPECTED_TC;}

  /*
    If current queue size (for stock, on ecn, on side, at top level of book)
    < this K * max queue size (same qualifications), then pull order....
  */
  const static double K_ALLOWED_QUEUE_POSITION = 0.25;
  const static double EXPECTED_TC = 0.0001;  // 1 bps.
};


#endif     // __FOLLOWLEADERTRACKER_H__
