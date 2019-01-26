#ifndef __MAXQUEUESIZETRACKER_H___
#define __MAXQUEUESIZETRACKER_H___

#include <ext/hash_map>
#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>

#include "DataManager.h"
#include "DataUpdates.h"
#include "CentralOrderRepo.h" // for the PlacementRequestsHandler

/**
 * This object keeps track of the max-queue position (# orders) of some level in a book (cid,ecn,side,px), over all wakeups
 * since initialized.
 * It is needed only for Follow-The-Leader at the moment, so it keep tracks only of levels in which we placed FTL-orders.
 * Therefore, in needs to listen to a queue of "OrderPlacements", which should be populated in 
 * the same wake-up in which the order was suggested.
 */
class MaxQueueSizeTracker : public PlacementsHandler::listener, public WakeupHandler::listener {
public:
  /// keep track of the max-q-size for orders of us placed with the specified reason only (not too elegantic, I admit...)
  MaxQueueSizeTracker( OrderPlacementSuggestion::PlacementReason reasonToTrack );

  /// listen to order-placements (Partially relies on getting these updates with unchanged market-picture from the 
  /// point the placement-decision was made).
  virtual void update( const OrderPlacementSuggestion& placementMsg );
  /// on wakeup: go over all orders in the map and update their max-queue-size
  virtual void update( const WakeUpdate & w );

  /// returns -1 if this order-id is not in map
  int getMaxQueueSize( int orderId ); 

  /// Some useful static function, for which we should find a more matching habitat (similar functionality appeared in LOB)
  static bool guessPositionInQueue( DataManager* dm, const Order* order, int& positionInQueue );


private:
  factory<DataManager>::pointer             _dm;
  factory<debug_stream>::pointer            _logPrinter;
  OrderPlacementSuggestion::PlacementReason _reasonToTrack; // Only tracks orders placed for this specified "reason"
  __gnu_cxx::hash_map<int,int>              _orderIdToMaxQSize;

};

#endif // __MAXQUEUESIZETRACKER_H___
