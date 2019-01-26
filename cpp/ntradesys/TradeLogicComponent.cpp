#include "TradeLogicComponent.h"

/******************************************************************************
  TradeLogicComponent code
******************************************************************************/
TradeLogicComponent::TradeLogicComponent() 
  : _componentId( CentralOrderRepo::allocatePlacerId() ),
    _nextComponentSeqNum(0),
    _centralOrderRepo( factory<CentralOrderRepo>::get(only::one) ),
    _logPrinter( factory<debug_stream>::get(std::string("trader")) ),
    _dm(factory<DataManager>::get(only::one)),
    _stocksState( factory<StocksState>::get(only::one) ),
    _chunk(factory<Chunk>::get(only::one) )
{}

void TradeLogicComponent::suggestOrderCancels( int cid, unsigned int bidCapacity, unsigned int askCapacity, double priority, 
					       int tradeLogicId, vector<OrderCancelSuggestion> &cancelSuggestions ) {
  // First, get the orders that are relevant to this instance of TradelogicComponent
  vector<const OrderRecord*> ordRecs = _centralOrderRepo->getOrderRecords( cid, tradeLogicId, _componentId );

  // Walk through orders, cancelling those no longer appear to be good idea to leave in market.
  // Note:  Logical structure used may not work well when needing to look across multiple orders
  //   when deciding which ones to cancel.  Thus the separation of suggestOrderCancels and
  //   decideToCancel functions, and the ability of subclasses that need to consider relationships
  //   between orders when deciding to cancel to potentially override suggestOrderCancels function
  //   in addition to decideToCancel.
  vector<const OrderRecord*>::const_iterator it;
  for( it = ordRecs.begin(); it != ordRecs.end(); it++ ) {
    const OrderRecord& ordRec = *(*it);
    // get the order
    int orderId = ordRec.orderId();
    const Order* order = _dm -> getOrder( orderId );

    if( !order ) {
      TAEL_PRINTF(_logPrinter.get(), TAEL_ERROR, "%-5s ERROR: in tradelogiccomponent::suggestordercancels asked to suggest cancel for an "
			     "order DM doesn't know of", _dm->symbol(cid) );
      continue;
    }
    
    // make sure the order has not yet been canceled by us
    if( order->isCanceling() )
      continue;

    OrderCancelSuggestion::CancelReason cancelReason = OrderCancelSuggestion::NONE;
    int capacity = order->side()==Mkt::BID ? bidCapacity : askCapacity;
    if( decideToCancel( cid, order, capacity, priority, cancelReason ) ) {
      SingleStockState *ss =_stocksState -> getState( cid );
      OrderCancelSuggestion cancelSugg( orderId, _componentId, cancelReason, 
					ss->bestPrice(Mkt::BID), ss->bestPrice(Mkt::ASK),
					_dm->curtv() );
      cancelSuggestions.push_back( cancelSugg );
    }
  }
} 

/// Count number of shares worth of cancels in cancelSuggestions that 
///   have specified cid x side.
int TradeLogicComponent::numberSharesToCancel(int cid, Mkt::Side side, 
					      vector<OrderCancelSuggestion> &cancelSuggestions) {
  int ret = 0;
  vector<OrderCancelSuggestion>::const_iterator it;
  for (it = cancelSuggestions.begin(); it != cancelSuggestions.end(); ++it) {
    const Order *order = _dm->getOrder( it->_orderId );
    if (!order)
      continue;
    if (order->cid() != cid || order->side() != side) {
      continue;
    }
    ret += order->sharesOpen();
  }
  return ret;
}
