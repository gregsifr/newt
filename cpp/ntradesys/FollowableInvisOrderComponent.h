#ifndef __FOLLOWABLEINVISORDERCOMPONENT_H__
#define __FOLLOWABLEINVISORDERCOMPONENT_H__

#include <vector>
using std::vector;

#include <bitset>
using namespace std;

#include <cl-util/factory.h>
using namespace clite::util;

#include "InvisOrderTracker.h"
#include "LastVisbTradeTracker.h"
#include "TradeLogicComponent.h"
#include "DataManager.h"
#include "StocksState.h"
#include "TradeConstraints.h"

/**
 *  Widget that keeps track of invisible orders:
 * - plus some additional state info that allows it to guess whether we want to try to follow them.
 * - plus some logic for order placement following invisible trades.
 * - plus some logic for when to cancel such orders.
 * The anyFollowableInvisibleTrades functions are defined to check whether we saw any
 *   followable invisible trades ON THIS WAKEUP only.
 */
class FollowableInvisOrderComponent : public TradeLogicComponent {
 protected:
  factory<StocksState>::pointer      _stocksState;
  factory<TradeConstraints>::pointer _tradeConstraints;  

  InvisOrderTracker    _iot;   /// Keeps map of stock (cid) -> invisible orders that occurred on this wakeup. 
  LastVisbTradeTracker _lvt;   /// Keeps track of last visible trade for stock x ecn x side.

  /// Does specified invisile trade look safe to follow.
  /// du shoudl be an invisible order trade.
  bool safeToFollow( const DataUpdate &du );

  ECN::ECN chooseECN( int cid, const bitset<ECN::ECN_size> &ecns, double stepUpPx );  

  int calculateOrderSize( int numShares, ECN::ECN ecn );
  /// Expected transaction costs for FIT algo.  
  /// This very simple TLC uses a static (across stocks x situations) 
  /// estimate based on sim and past live behavior.    
  const static double EXPECTED_TC = 0.00005;  /// One-half bps.

 public:  
  FollowableInvisOrderComponent();

  /// Suggest a set of order placements for specified stock x side, to be placed on current wakeup.  
  virtual void suggestOrderPlacements( int cid, Mkt::Side side, int tradeLogicId, 
				       int numShares, double priority,
				       vector<OrderPlacementSuggestion> &suggestions );

  virtual bool decideToCancel( int cid, const Order* order, unsigned int capacity, double priority,
			       OrderCancelSuggestion::CancelReason& cancelReason );

  /// Did we see any invisible trades in specified stock on specified side.
  /// If so, fills in ecn and fp.
  /// fp should be filled in with the least-aggressive price at which such a trade
  ///   occurred.  ecn should be filled in with any of the ecns on which an
  ///   invisible trade occurred at that least-aggressive price....
  bool anyFollowableInvisibleTrades( int cid, Mkt::Side side, bitset<ECN::ECN_size> &ecns, double &fp );

  /// Length of time (in msec) to wait for someone to follow your order before
  /// timing out, when following an INVTRD.
  const static int K_FOLLOW_INVTRD_TIMEOUT_MSEC = 500;
  
  /**
   * Length of time (in msec) to look back & check visible trades on same stock x ecn x side
   *  to see if they pollute the invtrd signal.
   * Intended to avoid picking up invisible trades that are:
   * - Someone taking out all the visible trades at a single level in the book, plus a bit,
   *   and catching some invisible trades that were at the end of that book level.
   * - Someone being too slow to re-price their invisible LO when the market moves quickly.
   */
  const static int K_INVTRD_LOOKBACK_MSEC = 25;
};

#endif  // __FOLLOWABLEINVISORDERCOMPONENT_H__
