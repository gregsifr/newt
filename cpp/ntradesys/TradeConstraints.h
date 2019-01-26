/*
 *  TradeConstraints: - Can we trade a particular cid? (Open Tracker, Reject Tracker)
 *                    - Can we trade a particular cid on a particular ECN? (set by user, currently)
 *                    - Can we trade a particular cid on a particular ECN in a particular price? (unsolicited cancels tracker)
 *                    - Would trade exceed max order rate (per stock, per second).
 */

#ifndef _TRADE_CONSTRAINTS_H_
#define _TRADE_CONSTRAINTS_H_

#include <bitset>
using namespace std;

#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>
#include <cl-util/float_cmp.h>
using namespace clite::util;
#include "DataManager.h"
#include "Markets.h"
#include "CapacityTracker.h"
#include "OpenTracker.h"
#include "CentralOrderRepo.h"

#include "SlidingWindow.h"

/// Constraints over a single cid
class PerSymbolConstraints {
private:
  const int                          _cid;
  factory<DataManager>::pointer      _dm;
  factory<debug_stream>::pointer     _logPrinter;
  factory<debug_stream>::pointer     _unsolicitedCxlsLog;
  factory<CentralOrderRepo>::pointer _centralOrderRepo;

  /// General constraints for the symbol
  bool                          _canBeTradedForSure; /// if true - can be traded. If false - should check (market opened? Recent REJECTs?)
  TimeVal                       _timeToResumeTrading; // Set following rejections (though should also check marketOpen before resuming)
  factory<OpenTracker>::pointer _openTracker;
  factory<CapacityTracker>::pointer _capTracker;
  /// ECN constraints for the symbol
  bitset<ECN::ECN_size> _ecnsAllowed; /// Keeps track of which ECNs engine is allowed to trade on. 

  /// ECN*price constraints for the symbol
  typedef clite::util::cmp<3>::hash_map<TimeVal>::type price_to_time;
  typedef __gnu_cxx::hash_set<int>                     orders_id_set;

  orders_id_set _cancelingOrdersSet; // A set of all orders of this symbol in a canceling state (fully canceling only)
  price_to_time _specificEcnPxConstraints[ECN::ECN_size]; /// one hash-map of (price ==> time to resume) for every ECN

  /// Rate limiting of per-ticker order placements.
  /// Note: Tower infra applies per ticker x virtual-account rate limits.  The mapping between
  ///   accounts, virtual accounts, and ECNs can be many - many.  So, to be safe, we
  ///   apply per ticker order placement rate limits, rather than per ticker x ecn
  ///   order placement rate limits.
  SlidingWindow<TimeVal> _opWindow;
  int _orderRate;
  TimeVal getHaltTimeFollowingRejection( Mkt::OrderResult error ) const;

  /// Check whether placing order (for stock _cid) would violate risk-mgmt imposed/asked-for
  ///   limits on order placement rate.
  bool checkOPRateLimit();
public:
  PerSymbolConstraints( int cid );
  
  // Note:  3 versions of canPlace cleaned up 20101001.  Same name + incremental parameter
  //   list suggest that different versions should implement progressively more restrictive
  //   checks - e.g. canPlace(ecn) should check everything that canPlace() does, and also
  //   check for any specific constraints that would prevent placing on the specified ecn.
  // Pre 20100101 version of code did not actually work that way:
  //   canPlace: checked capacity, OPR rate limit, stock opened, and no recent rejets.
  //   canPlace(ecn): checked trading on specific ecn allowed *only*.
  //   canPlace(ecn, price) as canPlace(), plus checked for recent unsolicited cancels at
  //     specified {ecn,price}.
  // Post 20100101:
  //   canPlace:  as above.
  //   canPlace(ecn):  as canPlace(), plus checks trading on specific ecn allowed.
  //   canPlace(ecn, price): as canPlace(ecn), plus checks for recent unsolicited cancels at
  //     specified {ecn,price}.
  bool canPlace();                                                /// Checks whether market opened for the symbol, for recent rejections, 
                                                                  ///   and for OPR rate limits.
  bool canPlace( ECN::ECN ecn ) {return canPlace() && _ecnsAllowed[ecn];} /// check for set of ECNs on which we can trade (set by user)
  bool canPlace( ECN::ECN ecn, double price );                    /// check for the set of ECNs as well as for recent unsolicited cancels

  void set( bool tradingAllowed ) { _canBeTradedForSure = tradingAllowed; }
  void set( ECN::ECN ecn, bool allowed ) { _ecnsAllowed.set( ecn, allowed ); }
  void setOrderRate ( int OrderRate ) { _orderRate = OrderRate; }
  // Tell constraint-system that have just placed an order.
  void onPlace();

  void onReject( const OrderUpdate& ou ); // halt trading for a while

  inline void onFullCxling( const OrderUpdate& ou ) { _cancelingOrdersSet.insert(ou.id()); }
  void onCxlRej( const OrderUpdate& ou ); /// remove the order from the canceling-orders set
  void onFullCxled( const OrderUpdate& ou );  /// either remove order from set or go to onUnsolicitedCxl
  void onUnsolicitedCxl( const OrderUpdate& ou ); /// halt trading for a while and report
};
/* Can we trade a particular cid [on a particular ECN [in a particular price]]
 * Based on: 
 * - market opened?
 * - Any recent rejections?
 * - User allows trading on a particular ECN?
 * - Recent unsolicited cancels in a particular ECN and price?
 */
class TradeConstraints : public OrderHandler::listener, public TimeHandler::listener {
public:  
  TradeConstraints();
  virtual ~TradeConstraints();

  inline bool canPlace( int cid ) { return _perSymbolConstraints[cid] -> canPlace(); }
  inline bool canPlace( int cid, ECN::ECN ecn ) const { return _perSymbolConstraints[cid] -> canPlace(ecn); } 
  inline bool canPlace( int cid, ECN::ECN ecn, double price ) { return _perSymbolConstraints[cid] -> canPlace(ecn, price); }

  void set( ECN::ECN ecn, bool allowedToTrade );
  void setOrderRate ( int OrderRate );
  // Tell constraint-system that have just placed an order.
  // Note:  given current execution engine design, would expect this to be trigged
  //  from an actual OrderPlacement (and be called fron inside
  //  void update( const OrderUpdate& ou ).
  // However, because 1 market change can generate multiple OrderPlacementSuggestions,
  //  and thus multiple discrete orders, if the logic is structured:
  //  - respond to mkt change.
  //  - get N order placement suggestions.
  //  - query on each.
  //  - place on each.
  // it would then be possible for the system to violate order placement rate limits.
  // As suchm we instead use an explicit notification that an order is about to be placed.
  void onPlace(int cid);
protected:
  vector<PerSymbolConstraints*>  _perSymbolConstraints; /// one for each symbol

  factory<DataManager>::pointer  _dm;
  factory<debug_stream>::pointer _logPrinter;

  virtual void update( const OrderUpdate& ou ); /// look for unsolicited cancels
  virtual void update( const TimeUpdate& t ); /// look for market-close message

};

# endif // _TRADE_CONSTRAINTS_H_
