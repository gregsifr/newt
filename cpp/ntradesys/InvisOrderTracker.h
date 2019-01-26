#ifndef __INVISORDERTRACKER_H__
#define __INVISORDERTRACKER_H__

#include <map>
using std::multimap;

#include <vector>
using std::vector;

#include <cl-util/factory.h>
using namespace clite::util;

#include "DataManager.h"

/**
 * Simple widget that keeps track of invisible order executions (for current wakeup()).
 * Designed to be flushed, then re-loaded on each wakeup().
 * As an optimization, can be instructed to only populate some subset of stocks, and
 *  discard updates for other stocks.
 * Also designed to answer simple queries about the invisible orders received/stored.
 * Internally, structured as a multi-map from cid -> DataUpdate, holding INVTRD DataUpdates only.
 */
class InvisOrderTracker : public MarketHandler::listener, public WakeupHandler::listener {

 public:
  InvisOrderTracker();
  virtual ~InvisOrderTracker() {}

  virtual void update( const DataUpdate& du );
  virtual void update( const WakeUpdate& wu );

  vector<const DataUpdate*> getInvisibleTradesSinceLastWakeup( unsigned int cid ) const;

protected:
  // This map is cleared on the first DataUpdate after a wakeup 
  // (This is a clumsy solution to make sure the map is cleared only after it's used by, e.g. FollowableInvisOrderComponent)
  multimap<int, DataUpdate> _invisibleTradesSincePreviousWakeup;
  bool                      _clearMapNextUpdate;

  void clear() { _invisibleTradesSincePreviousWakeup.clear(); }
};

#endif  // __INVISORDERTRACKER_H__
