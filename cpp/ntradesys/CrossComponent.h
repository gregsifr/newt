/**
 * Code for simple crossing order placement component.  
 * 1st version of this component:
 * - Only crosses inside spread, aka will only take out 1st level of CBBO.
 * - In order to determine whether to "fire" or not, compares TR priority
 *   with 0.5*spread + fees.  Crosses only If priority  >= spread + fees
 * - Currently doesn't use unconditional alpha signal to make more/less likely to cross.
 * - Waits some small amount of time between firing off crossing orders (per stock),
 *   e.g. 150 milliseconds.  This is a *very very simple and probably insufficient*
 *   attempt to avoid/reduce effect of pushing stock away from you while crossing.
 *
 *   Priority component should divide priority by 2.0 instead.
 */

#ifndef __CROSSCOMPONENT_H__
#define __CROSSCOMPONENT_H__

#include <cl-util/factory.h>
using namespace clite::util;

#include "TradeLogicComponent.h"
#include "ExchangeTracker.h"
#include "DataManager.h"
#include "TradeConstraints.h"
#include "StocksState.h"
#include "FeeCalc.h"

class AlphaSignal;

class CrossComponent : public TradeLogicComponent {
 protected:
  factory<StocksState>::pointer      _stocksState;
  factory<TradeConstraints>::pointer _tradeConstraints;
  factory<ExchangeTracker>::pointer  _exchangeT;
  factory<FeeCalc>::pointer          _feeCalc;

  int             _waitMilliSec;                       // Number of milliseconds to wait between placing crossing orders (per stock).
  double _priorityScale;
  vector<TimeVal> _lastOPTime;                         // Last time an order was placed by this component, per stock.

  AlphaSignal *_fvSignal;                              // Unconditional return/alpha signal.  Null for no-signal.

  ECN::ECN chooseECN( int cid, int size, Mkt::Tape tape, const vector<int>& ecnSizes );
  bool _enabled;
 public:
  CrossComponent( int waitMilliSec, AlphaSignal *fvSignal);

  // Suggest a set of order placements for specified stock x side, to be placed on
  //   current wakeup.  Intended to be called after populateFromDataQueue / other 
  //   populate on wakeup functions.
  // Initial & target position is probably not a generic enough interface/semantics.
  // What we want is to move the order placement & pull logic into a signal class,
  //   in a way that is generic across both trade & execution logics.
  // Initial and target Position seems to work with with the execution logic framework,
  //   but not the trade l  // initial and targetPosition should be signed quantities, akak positive #s for long,
  //   negative numbers for short.
  void setWait( int waitMilliSec ) { _waitMilliSec = waitMilliSec;}
  void setScale(double scale){ if (scale>0) _priorityScale = scale; }

  virtual void suggestOrderPlacements( int cid, Mkt::Side side, int tradeLogicID, 
				       int numShares, double priority,
				       vector<OrderPlacementSuggestion> &suggestions );

  
  virtual bool decideToCancel( int cid, const Order* order, unsigned int capacity, double priority,
			       OrderCancelSuggestion::CancelReason& cancelReason ) { return false; }

  void setEnable( bool e ) { _enabled = e; }

private:
  /// for each tape, this keeps a vector of ECNs sorted by increasing remove-liquidity fees
  static vector< vector<ECN::ECN> > _ecnsSortedByFees; // NUM_TAPES * N_ECNS_PRIORITISED
  static bool _ecnsSortedByFeesInitialized;

  void initializeVectorOfSortEcns();
};


#endif   //  __CROSSCOMPONENT_H__
