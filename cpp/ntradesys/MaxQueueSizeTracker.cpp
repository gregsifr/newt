#include <algorithm> // included for std::max

#include "MaxQueueSizeTracker.h"

#ifndef ABS
#define ABS(x) ((x)>0?(x):(-x))
#endif

const double MILLION = 1000000.0;
const double MAX_DIFF_BETWEEN_CFRM_TIME_AND_BOOK_ELEM_TIME = 0.001; // in seconds

MaxQueueSizeTracker::MaxQueueSizeTracker( OrderPlacementSuggestion::PlacementReason reasonToTrack )
  : _reasonToTrack( reasonToTrack )
{
  _dm = factory<DataManager>::find(only::one);
  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in MaxQueueSizeTracker::MaxQueueSizeTracker)" );
  _logPrinter = factory<debug_stream>::get( std::string("trader") );

  _dm->add_listener_front( this );
}

void MaxQueueSizeTracker::update( const OrderPlacementSuggestion &newOrderRecord ) {
  if( newOrderRecord._reason != _reasonToTrack )
    return;
  if ( newOrderRecord._ecn == ECN::NYSE ) // NYSE not supported
    return; 
  int orderId = newOrderRecord.getOrderId();
  const Order* order = _dm->getOrder( orderId );
  if( order==NULL ) {
    TAEL_PRINTF(_logPrinter.get(), TAEL_ERROR, "ERROR in MaxQueueSizeTracker::update( const OrderPlacementSuggestion& ). "
			 "Id of reported Order (=%d) not found in DM", orderId );
    return;
  }

  int currentQSz = getMarketOrders( _dm->subBook(order->ecn()), order->cid(), order->side(), order->price() );
  std::pair<int,int> p( orderId, currentQSz );
  if( !_orderIdToMaxQSize.insert(p).second ) //map.insert returns <iterator,bool>, where the bool is true if insertion was successful
    TAEL_PRINTF(_logPrinter.get(), TAEL_WARN, "%-5s WARNING: MaxQueueSizeTracker::update(OrderPlacementSuggestion) couldn't insert new "
			 "order-Id (%d) to the map", _dm->symbol(order->cid()), orderId );
  return;
}

void MaxQueueSizeTracker::update( const WakeUpdate& w ) {
  __gnu_cxx::hash_map<int,int>::iterator it;
  for( it=_orderIdToMaxQSize.begin(); it!=_orderIdToMaxQSize.end(); ) {
    int orderId = it -> first;
    const Order* order = _dm->getOrder( orderId );
    if( order == NULL ) {
      // this order no longer exists ==> remove it from the map
      _orderIdToMaxQSize.erase( it++ );
      continue;
    }
    int currQueueSize = getMarketOrders( _dm->subBook(order->ecn()), order->cid(), order->side(), order->price() );
    it -> second = std::max( it->second, currQueueSize );
    it++;
  }
}

int MaxQueueSizeTracker::getMaxQueueSize( int orderId ) {
  __gnu_cxx::hash_map<int,int>::iterator it = _orderIdToMaxQSize.find( orderId );
  if( it == _orderIdToMaxQSize.end() ) {
    TAEL_PRINTF(_logPrinter.get(), TAEL_ERROR, "ERROR: MaxQueueSizeTracker::getMaxQueueSize was called to with an order-Id (%d) that doesn't "
			 "show in its internal map", orderId );
    return -1;
  }
  return it->second;
}

// guess the queue position of a certain order of ours in the general queue (of a particular ECN)
// returns false if failed
bool MaxQueueSizeTracker::guessPositionInQueue( DataManager* dm, const Order* order, int& positionInQueue )
{
  // We need the confirmation time of our order in order to match it up with an order in the book
  if( !order->confirmed() || order->confirmed().action()!=Mkt::CONFIRMED ) 
    return false;
  double ourOrderConfirmTimeInSecs = order->confirmed().tv().sec() + order->confirmed().tv().usec() / MILLION;

  // our order cannot be invisible
  if( order->invisible() )
    return false;

  using namespace lib3;
  // Get the list of orders in the relevant queue
  std::pair<BookLevel<BookOrder>::OrderListIter, BookLevel<BookOrder>::OrderListIter> levelList = 
    getOrdersList( dm->subBook(order->ecn()), order->cid(), order->side(), order->price() );
  int position = 0;
  for( BookLevel<BookOrder>::OrderListIter it = levelList.first; it!=levelList.second; it++, position++ ) {
    if( (*it)->entry_size != order->sharesOpen() ) continue;
    
    float orderTimeInSec = (*it)->add_time.sec() + (*it)->add_time.usec() / MILLION;
    if( ABS(orderTimeInSec-ourOrderConfirmTimeInSecs) > MAX_DIFF_BETWEEN_CFRM_TIME_AND_BOOK_ELEM_TIME ) continue;
    
    // ok, so now we assume there's a match
    positionInQueue = position;
    return true;
  }
  return false;
}
  

  
