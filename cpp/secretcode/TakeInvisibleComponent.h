/**
 * Code for simple order placement component that attempts to find & take hidden
 *  liquidity.
 * 1st version of this component:
 * - Only attempts to cross inside of the CBO (not to the inside CBBO, but
 *   inside of the CBBO).  Aka, it will only cross at a price that is less
 *   aggressive than the stated CBBO.
 * - When it sees a order execute against an invisible limit, computes whether
 *   it would be willing to hit that same invisible limit order.  If so, immediately
 *   tries to cross to that price (with a fill-or-kill order).
 * - Periodically computes the maximum price (per ECN) at which it would be willing 
 *   to hit an invisible limit order (and still trade for <= the specified cost).
 *   Submits fill-or-kill orders at that price level in an attempt to hit any such
 *   limit orders if it can find them in the book.
 * - In order to determine whether to "fire" or not, compares TR priority
 *   with 0.5*spread + fees.  Crosses only If priority  >= spread + fees
 * Modification History:
 *   - Moved divide by 2.0 logic into priorityComponent.
 */

#ifndef __TAKEINVISIBLECOMPONENT_H__
#define __TAKEINVISIBLECOMPONENT_H__

#include <cl-util/factory.h>
using namespace clite::util;

#include "TradeLogicComponent.h"
#include "ExchangeTracker.h"
#include "MarketCapacityModel.h"
#include "DataManager.h"
#include "TradeConstraints.h"
#include "StocksState.h"
#include "AlphaSignal.h"
#include "FeeCalc.h"

#include "InvisOrderTracker.h"


class TakeInvisibleComponent : public TradeLogicComponent {
 protected:
  factory<DataManager>::pointer     _dm;
  factory<ExchangeTracker>::pointer _exchangeT;
  factory<TradeConstraints>::pointer _tradeConstraints;
  factory<FeeCalc>::pointer          _feeCalc;
  GlobalMarketCapacityModel *_gmcm;  

  int             _waitMilliSec;                       // Number of milliseconds to wait between placing crossing orders (per stock).
                                                       // Only used for periodically sweeping book, not for responding to hidden executions.
  vector<TimeVal> _lastOPTime;                         // Last time an order was placed by this component, per stock.  
  vector<int>     _lastOPNum;                          // Increasing id number for last order placed by this component, per stock. 

  double _priorityScale;
  AlphaSignal *_fvSignal;                              // Unconditional return/alpha signal.  Null for no-signal.
  InvisOrderTracker _iot;                              // Keeps copy of invisible orders seen since last wakeup.
                                                       // Current code keeps separate copy inside this component.
                                                       // Might want to move to using factory system....
  bool _printTrades;
  const static int _maxFlightMilliSeconds = 15;        // Guess at maximum round-trip latency to submit an IOC order
                                                       //  and get a response.  Should be strictly >> worst case real number.
  const static double _probeOrderProb = 0.20;          // Probability of issuing a probe order, on any wakeup where
                                                       //  one is eligable.  Applied per stock, to make probe orders
                                                       //  less bursty.

  const static double _takeInvisSignalPenalty = 0.00010;// 1.0 bps.         
                                                       // Take-invis orders only execute against hidden liquidity.  
                                                       // Empirically, the presence of hidden liquidity itself seems to be
                                                       //   a signal, which tends to skew returns of TakeInvis orders that execute
                                                       //   (in the wrong direction).  As a short-term fix, we apply an
                                                       //   empirically guesstimated penalty to the signa;-derived fair value
                                                       //    when computing willingness-to-pay with TakeInvisOrders.  
                                                       // This flat guesstimate should probably be replaced with a real 
                                                       //    model-derived value.... 

  bool _enabled;

  /*
    Non public member functions....
  */
  int suggestFollowOrderPlacements(int cid, Mkt::Side side, int tradeLogicId, 
				   int numShares, double priority,
				   vector<OrderPlacementSuggestion> &suggestions);
  int suggestFollowOrderPlacementsECN(int cid, Mkt::Side side, int tradeLogicId, 
				      int numShares, double priority,
				      vector<OrderPlacementSuggestion> &suggestions,
				      ECN::ECN destECN, vector <const DataUpdate*> &invisTrades );
  bool computeMostAggressivePrice(int cid, ECN::ECN ecn, Mkt::Side side, 
				  int numShares, double priority, double &maPrice);
  int calculateOrderSize( int cid, Mkt::Side side, int numShares, ECN::ECN e, double px );
  bool getECNBBO(int cid, Mkt::Side side, ECN::ECN ecn, double &px, int &sz);
  int suggestProbeOrderPlacements(int cid, Mkt::Side side, int tradeLogicId, 
				  int numShares, double priority,
				  vector<OrderPlacementSuggestion> &suggestions);
  void markOP(int cid);

  // For debugging.
  void printSuggestion(OrderPlacementSuggestion &op, const string &reason);
 
 public:
  TakeInvisibleComponent(int waitMilliSeconds, AlphaSignal *fvSignal);
  void setWait( int waitMilliSeconds ) { _waitMilliSec = waitMilliSeconds;}
  void setScale(double scale){ if (scale>0) _priorityScale = scale; }
  
  virtual ~TakeInvisibleComponent();
  

  // Suggest a set of order placements for specified stock x side, to be placed on
  //   current wakeup.  Intended to be called after populateFromDataQueue / other 
  //   populate on wakeup functions.
  // Initial & target position is probably not a generic enough interface/semantics.
  // What we want is to move the order placement & pull logic into a signal class,
  //   in a way that is generic across both trade & execution logics.
  // Initial and target Position seems to work with with the execution logic framework,
  //   but not the trade l  // initial and targetPosition should be signed quantities, akak positive #s for long,
  //   negative numbers for short.
  virtual void suggestOrderPlacements( int cid, Mkt::Side side, int tradeLogicID, 
				       int numShares, double priority,
				       vector<OrderPlacementSuggestion> &suggestions );

  
  virtual bool decideToCancel( int cid, const Order* order, unsigned int capacity, double priority,
			       OrderCancelSuggestion::CancelReason& cancelReason ) { return false; }

  void setEnable ( bool e ) { _enabled = e; }
};

#endif   //  __TAKEINVISIBLECOMPONENT_H__
