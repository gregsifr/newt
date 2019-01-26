#ifndef __MOCCOMPONENT_H__
#define __MOCCOMPONENT_H__

#include "DataManager.h"
#include "TradeLogicComponent.h"
#include <vector>
using std::vector;

#include "FeeCalc.h"
#include <cl-util/table.h>
#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>

using namespace clite::util;

#include "Markets.h"

/* Class to enable sending Market On Close orders. Should essentially bypass the entire
 * execution server logic, and should just check for simple things like order rate, order
 * size, etc.
 **/
class MOCComponent: public TradeLogicComponent {
 std::string exchanges_filename;
 file_table<ParamReader> tic2exch;
 factory<StocksState>::pointer _stocksState;
 clite::util::factory<clite::util::debug_stream>::pointer dbg;
 public:
  MOCComponent();
  ~MOCComponent() {}

  /// Suggest a set of order placements for specified stock x to be placed on current wakeup.  
  /// Intended to be called after populateFromDataQueue / other populate on wakeup functions.
  void suggestOrderPlacements( int cid, Mkt::Side side, int tradeLogicId,
				       int numShares, double priority,
				       vector<OrderPlacementSuggestion> &suggestions );

  bool isPastCutoff(ECN::ECN ecn);

  /// Should populate <cancelSuggestions>
  // Since we don't want to cancel any orders, this function can be null
  virtual void suggestOrderCancels( int cid, unsigned int bidCapacity, unsigned int askCapacity, double priority, 
				    int tradeLogicId, vector<OrderCancelSuggestion> &cancelSuggestions ) {}

};


#endif    // __MOCCOMPONENT_H__
