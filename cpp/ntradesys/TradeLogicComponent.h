/*
  TradeLogicComponent.  Interface for something that sits inside a trade logic, execution logic, 
    or study logic, and makes suggestions/recommendations on:
  - Order placements in response to current market conditions.
  - Order cabncellations in response to current market conditions.

  These components are intended to be "plug-and'play", in that they can be easily snapped
    into TradeLogics (or studies), without needing to duplicate
    very similar signal, order-placement, and order cancellation logics across those
    different TradeLogic(s).

  Initial use for these components is making seperate follow-leader and follow-invtrd 
    order placement components that can be easily plugged into:
  - TradeLogicZura, I1, I2.
  - A simple HF strategy using those signals for entry/exit of trades, but not structured
    to do external client execution.
  - Some simple studies examining the alpha in various events flagged by those components.
*/

#ifndef __TRADELOGICCOMPONENT_H__
#define __TRADELOGICCOMPONENT_H__

#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>
using namespace clite::util;

#include "Markets.h"
#include "CentralOrderRepo.h" 
#include "DataManager.h"
#include "Suggestions.h"
#include "StocksState.h"
#include "Chunk.h"

/**
 * Abstract class / interface to specify semi-generic way in which signals used for order-placement
 * can both suggest new order placements, and recommend for/against cancelling old ones.  
 */
class TradeLogicComponent {
 protected:
  const int _componentId;  /// Intended to uniquely identify each instance of each TradeLogicComponent type.
  int       _nextComponentSeqNum; /// A sequence number for suggestions made by this component. This is used to enable
                                  /// the component to keep some data about the placement-suggestion, and later on know about its placement

  /// Count number of shares worth of cancels in cancelSuggestions that 
  ///   have specified cid x side.
  virtual int      numberSharesToCancel(int cid, Mkt::Side side, vector<OrderCancelSuggestion> &cancelSuggestions);
 public:
  factory<CentralOrderRepo>::pointer _centralOrderRepo;
  factory<debug_stream>::pointer     _logPrinter;
  factory<DataManager>::pointer      _dm;
  factory<StocksState>::pointer     _stocksState;
  factory<Chunk>::pointer           _chunk;
  TradeLogicComponent();
  virtual ~TradeLogicComponent() {}

  /// Get copy of componentId.
  int componentId() const {return _componentId;}

  /// Allocate a sequence number.
  /// By convention, the same sequence number should be used for the individual pieces
  ///   of what is logically the same order.  This is useful e.g. when an OPC breaks up
  ///   a large order into pieces to reduce information leakage.
  int allocateSeqNum() {return _nextComponentSeqNum++;}

  /// Suggest a set of order placements for specified stock x to be placed on current wakeup.  
  /// Intended to be called after populateFromDataQueue / other populate on wakeup functions.
  virtual void suggestOrderPlacements( int cid, Mkt::Side side, int tradeLogicId, 
				       int numShares, double priority,
				       vector<OrderPlacementSuggestion> &suggestions ) = 0;

  /// Should populate <cancelSuggestions>
  virtual void suggestOrderCancels( int cid, unsigned int bidCapacity, unsigned int askCapacity, double priority, 
				    int tradeLogicId, vector<OrderCancelSuggestion> &cancelSuggestions );

  /// Decide whether a given single specific order should be cancelled. If so, returns true and fill cancelSuggestions with a reason.
  /// Should probably be override in most subclasses
  /// Note: 'capacity' is a non-negative number that should match the side of the order
  virtual bool decideToCancel( int cid, const Order* order, unsigned int capacity, double priority,
			       OrderCancelSuggestion::CancelReason& cancelReason ) { return false; }
};


#endif    // __TRADELOGICCOMPONENT_H__
