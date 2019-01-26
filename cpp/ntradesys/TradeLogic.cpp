 #include "TradeLogic.h"
 #include "CentralOrderRepo.h"

 const int BUF_SIZE = 1024;
 static char buffer[BUF_SIZE];

 const TimeVal MAX_TIME_WITHOUT_TOUCH(0,100000); /// 0.1 sec. Add an artificial "touch" to a stock after 0.1 seconds without touch

 TradeLogic::TradeLogic( TradeLogicComponent* component ) 
   : _stocksState( factory<StocksState>::get(only::one) ),
     _centralRepo( factory<CentralOrderRepo>::get(only::one) ),
     _tradeConstraints( factory<TradeConstraints>::get(only::one) ),
     _logPrinter( factory<debug_stream>::get(std::string("trader")) ),
     _riskLimits( factory<RiskLimit>::get(only::one) ),
     _mainTLComponent( component ),
     _tlMIM( factory<TakeLiquidityMarketImpactModel>::get(only::one) ),
     _OrderProb(1.0)
 {
   _dm = factory<DataManager>::find(only::one);
   if( !_dm )
     throw std::runtime_error( "Failed to get DataManager from factory (in TradeLogic::TradeLogic)" );

   _associations.resize( _dm->cidsize(), 0 );
   _numTouches.resize( _dm->cidsize(), 0 );
   _lastTouch.resize( _dm->cidsize(), TimeVal(0) );
   _currentlyTrading.resize( _dm->cidsize(), 0 );
   _errCount.resize( _dm->cidsize(), 0);
   _lastOrderResult.resize( _dm->cidsize(), Mkt::NO_REASON);

   _tradeLogicId = CentralOrderRepo::allocatePlacerId();
   _placementsHandler = factory<PlacementsHandler>::get( only::one );
   _cancelsHandler = factory<CancelsHandler>::get( only::one );

   _dm -> add_listener( this );
 }

 // Does specified order attempt to take or provide liquidity -
 //   given current market conditions?
 bool TradeLogic::takesLiquidity(const OrderPlacementSuggestion &ops) {
   if (ops._timeout > 0) {
     return false;
   }
   return true;
 }

 bool TradeLogic::placeOrder( int cid, OrderPlacementSuggestion& placementSugg ) {
   SingleStockState* ss = _stocksState -> getState( cid );

   // DataManager should take care of distinguishing SELL orders from SHORT orders
   Mkt::Side side    = placementSugg._side;
   int       size    = placementSugg._size;
   ECN::ECN  ecn     = placementSugg._ecn;
   double    price   = placementSugg._price;
   int       timeout = placementSugg._timeout;

   int rnd = rand();
   if (rnd > _OrderProb * RAND_MAX)
     return false;
   
  // Check with DM whether this order can be placed (DM checks with RejectTracker, with lib2/lib3 etc.)
  int orderId;
  if (_riskLimits){
    size = std::min(size,_riskLimits->maxShares() );
    if (size*price >= _riskLimits->maxNotional() ){
      int newsize = HFUtils::roundSizeForNYSE( int(floor( _riskLimits->maxNotional() / price ) ));
      if (newsize==0){
	// Corner case for vv expensive stocks, 
	newsize =  int(floor( _riskLimits->maxNotional() / price ) );
	if ((ecn==ECN::NYSE) || (ecn==ECN::ARCA)){
	  // Dont send odd lots to these places
	  TAEL_PRINTF(_logPrinter.get(), TAEL_ERROR, "%-5s TL: Risk: Cant trade size=%d price=%.2f",
			       _dm->symbol(cid),size, price);
	  return false;
	}
	
      }
      TAEL_PRINTF(_logPrinter.get(), TAEL_WARN, "%-5s TL: Risk: resizing order to respect risk limits %d -> %d @ %.2f",
			   _dm->symbol(cid),size, newsize,price);
      size=newsize;
    }
  }

  // Check to make sure that there are no know constraints that would prevent us
  //   from placing specified order.
  if (!_tradeConstraints->canPlace(cid, ecn, price)) {
      TAEL_PRINTF(_logPrinter.get(), TAEL_WARN, "%-5s TL: TradeConstraints: Cant trade size=%d price=%.2f",
			       _dm->symbol(cid),size, price);
      return false;
  }

  // Tell _tradeConstraints that we are about to place an order for specified symbol.
  // See TradeConstraints code for expkanation of why this is done immediately before
  //   placing an order, as opposed to waiting for a PLACED OrderUpdate.
  _tradeConstraints->onPlace(cid);

  Mkt::OrderResult orst = _dm->placeOrder( cid, ecn, size, price, side, timeout, false, &orderId, ss->getOrderID(), ss->getMarking(), placementSugg.PlacementReasonDesc[(int)placementSugg._reason] );
  
  if (orst!=_lastOrderResult[cid]){
    _errCount[cid]=0;
    _lastOrderResult[cid]=orst;
  }
  else
    _errCount[cid]++;

  if( orst != Mkt::GOOD ) {
    if( _errCount[cid]%100==0 ) {
      TAEL_PRINTF(_logPrinter.get(), TAEL_WARN, "%-5s dm->placeOrder returned %s, NOT placing order",
			   _dm->symbol(cid), Mkt::OrderResultDesc[orst] );
      TAEL_PRINTF(_logPrinter.get(), TAEL_WARN, "%-5s OrderDetails: ecn = %s, size = %d, price = %.2f, side = %s, timeout = %d",
			   _dm->symbol(cid), ECN::desc(ecn), size, price, Mkt::SideDesc[side], timeout );
    }
    return false;
  }
  if( orderId < 0 ) {
    TAEL_PRINTF(_logPrinter.get(), TAEL_ERROR, "%-5s BUG in TradeLogic::placeOrder. DM::placeOrder returned Mkt::GOOD but orderId is %d",
			 _dm->symbol( cid ), orderId );
    return false;
  }

  // For liquidity-taking orders *only*.  Estimate expected market impact of
  //   order given current market conditions.
  MarketImpactEstimate marketImpactEst;
  double marketImpact = 0.0;
  if (takesLiquidity(placementSugg)) {
    // Note: OrderPlacementSuggestion uses Mkt::Side where it should use Mkt::Trade. 
    // Therefore, sides are reversed for liquidity-taking orders.
    Mkt::Side crossSide =( side == Mkt::BID ? Mkt::ASK : Mkt::BID );  
    if (!_tlMIM || !_tlMIM->marketImpactFill(cid, ecn, size, price,
					     crossSide, timeout, false, marketImpactEst)) {  // Note - false should be ops._invisible.
      char buf[256];
      placementSugg.snprint(buf, 256);
      TAEL_PRINTF(_logPrinter.get(), TAEL_WARN, "%-5s TL: MarketImpact: unable to estimate MI of order, using 0 : %s",
			   _dm->symbol(cid), buf);  
    } else {
      marketImpact = marketImpactEst.permanentImpact();
    }
  }

  TAEL_PRINTF(_logPrinter.get(), TAEL_INFO, "%-5s TL::placeOrder: %s %4d shs on %s at %7.3f Reason = %s "
		       "compId = %2d cbbo=(%.2f,%.2f) sizes=(%d,%d) id=%d pri=%.6f impact=%.8f", 
		       _dm->symbol(cid), Mkt::SideDesc[side], size, ECN::desc(ecn), price, 
		       OrderPlacementSuggestion::PlacementReasonDesc[placementSugg._reason], 
		       placementSugg._componentId, ss->bestPrice(Mkt::BID), ss->bestPrice(Mkt::ASK),
		       ss->bestSize(Mkt::BID), ss->bestSize(Mkt::ASK), orderId, placementSugg._priority, 
		       marketImpact );

  // Update the order-Id & market-impact-estimate and broadcast a placement-message
  placementSugg.setOrderId( orderId );
  placementSugg.setMarketImpact(marketImpact);
  _placementsHandler->send( placementSugg );
  return true;
}

// Apply the cancel suggestion (send cancel request and cancel update)
void TradeLogic::cancelOrder( OrderCancelSuggestion cancelSuggestion ) {
  int orderId = cancelSuggestion._orderId;

  // For logging
  const Order* order = _dm -> getOrder( orderId );
  if( order==NULL ) {
    TAEL_PRINTF(_logPrinter.get(), TAEL_ERROR, "ERROR: In TradeLogic::cancelOrder encountered an OrderRecord with "
			 "id (%d) DM doesn't know of", orderId );
    return; // Order not found in DM
  }

  // Already trying to cancel order.  Dont re-try, or re-broadcast cancel message.
  // Probably also should not log anything here, to avoid overloading logging facilities
  //   where cancels take a long time to hit the exchange & return.
  if (order->canceling()) {
    return;
  }

  if ((order->realecn() == ECN::NYSE) && (order->state() == Mkt::NEW)) {
	  // SingleStockState *ss = _stocksState->getState( order->cid() );
	  TAEL_PRINTF(_logPrinter.get(), TAEL_ERROR, "Trying to cancel NYSE order without getting a CNF. "
			      "%s (TL::cancelOrder): id=%d (%s,%3d@%.2f,%s) compId=%d %s",
		          _dm->symbol(order->cid()), order->id(), Mkt::SideDesc[order->side()], order->sharesOpen(),
		          order->price(), ECN::desc(order->ecn()), cancelSuggestion._componentId,
		          OrderCancelSuggestion::CancelReasonDesc[cancelSuggestion._reason]);
	  return;
  }

  _dm->cancelOrder( orderId );
  _cancelsHandler->send( cancelSuggestion );

  SingleStockState *ss = _stocksState->getState( order->cid() );
  TAEL_PRINTF(_logPrinter.get(), TAEL_INFO, "%-5s (TL::cancelOrder): id=%d (%s,%3d@%.2f,%s) compId=%d %s cbbo=(%.2f,%.2f) sizes=(%d,%d)",
		       _dm->symbol(order->cid()), order->id(), Mkt::SideDesc[order->side()], order->sharesOpen(), order->price(), 
		       ECN::desc(order->ecn()), cancelSuggestion._componentId, 
		       OrderCancelSuggestion::CancelReasonDesc[cancelSuggestion._reason],
		       ss->bestPrice(Mkt::BID), ss->bestPrice(Mkt::ASK), ss->bestSize(Mkt::BID), ss->bestSize(Mkt::ASK) );
}

// Place set of orders specified in placementSuggs.
// Assumes no need for intermediation/resolution of interactions between order placement suggestions.
// Assumes that populator of placementSuggs has already been careful not the exceed risk/position constrainsts.
int TradeLogic::placeNewOrders( int cid, vector<OrderPlacementSuggestion> &placementSuggs ) {
  int ret = 0;
  vector<OrderPlacementSuggestion>::iterator it;
  for( it = placementSuggs.begin(); it != placementSuggs.end(); ++it )
    if( placeOrder(cid, *it) ) ret++;
  return ret;
}

int TradeLogic::cancelUndesiredOrders( int cid, unsigned int bidCapacity, unsigned int askCapacity, double priority ) {

  vector<OrderCancelSuggestion> cancelSuggestions;
  _mainTLComponent -> suggestOrderCancels( cid, bidCapacity, askCapacity, priority, _tradeLogicId, cancelSuggestions );

  // Cancel the specified orders.
  for( unsigned int i=0; i<cancelSuggestions.size(); i++ ) 
    cancelOrder( cancelSuggestions[i] );

  return( cancelSuggestions.size() );
}

void TradeLogic::update( const WakeUpdate& wu ) {
  int numCancelled, numPlaced, numOutstanding;
  vector<OrderPlacementSuggestion> placementSuggs,subPlacementSuggs;
  SingleStockState *ss;

  // Check if in a mode where should be doing any trading.  if so, do the trading.
  for( int cid=0; cid<_dm->cidsize(); cid++ ) {
	if( _dm->stops[cid]) continue;
    // in target position and no outstanding orders - don't try to trade.
    if( _currentlyTrading[cid] == 0 ) continue;
    // stock not associated with this trade logic - dont try to trade.
    if( _associations[cid] == 0 ) continue;
    // stock not touched since last wakeup - no need to revisit trading decisions (unless some time has passed since the last touch.
    // After all, some of our trading decisions are based on time limits: trade-halting, JQ-backup,.. )
    if( _lastTouch[cid] < _dm->curtv() - MAX_TIME_WITHOUT_TOUCH ) addTouch( cid );
    if( numTouches(cid) < 1 ) continue;
    
    numCancelled = numPlaced = numOutstanding = 0;  
    ss = _stocksState -> getState( cid );
    int currentPosition = _dm -> position( cid ); 
    int targetPosition = ss -> getTargetPosition();
    double priority = ss -> getPriority();
    
    numOutstanding = getMarketOrders( _dm->orderBook(), cid );
    if( numOutstanding == 0 && (currentPosition == targetPosition) ) {
      _currentlyTrading[ cid ] = 0;
      continue;
    }

    // Just allocate capacity to _mainTLComponent
    int distToTarget = targetPosition - currentPosition;
    unsigned int bidCapacity = distToTarget>0 ?  distToTarget : 0;
    unsigned int askCapacity = distToTarget<0 ? -distToTarget : 0;
    
    placementSuggs.clear();
    // if trade not open yet for some reason / halted for this symbol ==> don't place new orders
    if( _tradeConstraints->canPlace( cid ) ) {
      subPlacementSuggs.clear();
      _mainTLComponent -> suggestOrderPlacements( cid, Mkt::BID, _tradeLogicId, bidCapacity, priority, subPlacementSuggs );
      placementSuggs.insert( placementSuggs.end(), subPlacementSuggs.begin(), subPlacementSuggs.end() );
      subPlacementSuggs.clear();
      _mainTLComponent -> suggestOrderPlacements(cid, Mkt::ASK, _tradeLogicId, askCapacity, priority, subPlacementSuggs);
      placementSuggs.insert( placementSuggs.end(), subPlacementSuggs.begin(), subPlacementSuggs.end() );
      if( placementSuggs.size() > 0 )
	numPlaced = placeNewOrders( cid, placementSuggs ); 
    }
    
    numCancelled = cancelUndesiredOrders( cid, bidCapacity, askCapacity, priority );
    
    //  Check current # of orders oustanding for this stock (bid & ask sides).
    //  - Should this be separated out BID vs ASK sides?
    numOutstanding = getMarketOrders( _dm->orderBook(), cid ); 
    
    //  Print some summary info about cancels/placements/outstanding orders.
    //  For efficiency, checking debug-level outside the printf function
    if( _logPrinter->configuration().threshold() >= TAEL_DATA && (numCancelled > 0 || numPlaced > 0) ) {
    	TAEL_PRINTF(_logPrinter.get(), TAEL_DATA, "%-5s In TradeLogic::update(wakeup): #Cancelled = %d, #Placed = %d, #Outstanding = %d, "
			   "mkt = (BID %d) x (%.2f,%.2f) x (%d ASK)", 
			   _dm->symbol(cid), numCancelled, numPlaced, numOutstanding, 
			   ss->bestSize(Mkt::BID), ss->bestPrice(Mkt::BID), 
			   ss->bestPrice(Mkt::ASK), ss->bestSize(Mkt::ASK) ); 
    }
  }
  
  // Reset map of which stocks we've touched....
  clearTouches();
}

void TradeLogic::update( const TradeRequest& tr ) {
  //TAEL_PRINTF(_logPrinter.get(), TAEL_INFO, "Got a TradeRequest for %s initPos: %d prevTarget: %d newTarget: %d",
  //            _dm->symbol(tr._cid), tr._initPos, tr._previousTarget, tr._targetPos);
  if( !_associations[tr._cid] ) return;

  //TAEL_PRINTF(_logPrinter.get(), TAEL_INFO, "Setting currentlyTrading = 1");
  _currentlyTrading[tr._cid] = 1;
  addTouch( tr._cid );

  // in case this is a stop request, and just in case we don't want to wait until the next wakeup to cancel our outstanding orders:
  if( tr._targetPos == _dm->position(tr._cid) && getMarketOrders(_dm->orderBook(), tr._cid)>0 ) {
    TAEL_PRINTF(_logPrinter.get(), TAEL_INFO, "%-5s In TradeLogic::update(TradeRequest) cancelling all orders because request identified as "
			 "a stop request", _dm->symbol(tr._cid) );
    cancelAllOrders( tr._cid , OrderCancelSuggestion::STOP_REQ );
  }
}

// broadcast cancel messages and cancel via DM
void TradeLogic::cancelAllOrders( int cid, OrderCancelSuggestion::CancelReason reason ) {
  _dm->cancelMarket( cid );
  SingleStockState *ss = _stocksState -> getState( cid );

  vector<const OrderRecord*> orders = _centralRepo->getOrderRecords( cid );
  for( unsigned int i=0; i<orders.size(); i++ ) {
    OrderCancelSuggestion cxlSugg( orders[i]->orderId(), orders[i]->componentId(), reason, 
				   ss->bestPrice(Mkt::BID),ss->bestPrice(Mkt::ASK),_dm->curtv() );
    _cancelsHandler->send( cxlSugg );
  }
}

// broadcast cancel messages and cancel via DM
void TradeLogic::cancelAllOrders( int cid, ECN::ECN ecn, OrderCancelSuggestion::CancelReason reason ) {
  bool printedComment = false;
  SingleStockState *ss = _stocksState -> getState( cid );
  vector<const OrderRecord*> ordRecs = _centralRepo->getOrderRecords( cid );
  for( unsigned int i=0; i<ordRecs.size(); i++ ) {
    if( ordRecs[i]->tradeLogicId() != _tradeLogicId ) continue; // Order not from this TL
    int orderId = ordRecs[i] -> orderId();
    const Order* order = _dm -> getOrder( orderId );
    if( order==NULL ) {
      TAEL_PRINTF(_logPrinter.get(), TAEL_ERROR, "%-5s ERROR: In TradeLogic::cancelAllOrders(cid,ecn,reason) encountered an OrderRecord with "
			   "id (%d) DM doesn't know of", _dm->symbol(cid), orderId );
      continue; // Order not found in DM
    }
    if( order->ecn() != ecn ) continue; // Order on a different ECN
    if( order->isCanceling() ) continue; // Don't double-cancel an order
     if( !printedComment ) {
      TAEL_PRINTF(_logPrinter.get(), TAEL_ERROR, "%-5s TL: cancelling all orders on %s (%s)",
			   _dm->symbol(cid), ECN::desc(ecn), 
			   OrderCancelSuggestion::CancelReasonDesc[reason]);
      printedComment = true;
    }
    OrderCancelSuggestion cxlSugg( orderId, ordRecs[i]->componentId(), 
				   reason, 
				   ss->bestPrice(Mkt::BID),ss->bestPrice(Mkt::ASK),
				   _dm->curtv() );
    cancelOrder( cxlSugg );
  }
}

void TradeLogic::disconnect() {
  TAEL_PRINTF(_logPrinter.get(), TAEL_INFO, "TradeLogic::disconnect called" );
  for( int cid=0; cid<_dm->cidsize(); cid++ ) {
    if( _associations[cid] ) {
      cancelAllOrders( cid, OrderCancelSuggestion::DISCONNECT );
      _associations[cid] = 0;
    }
  }
}

// Tell TradeLogic that it has just been associated with a specified stock.
bool TradeLogic::associate( int cid ) {
  _associations[ cid ] = 1;
  TAEL_PRINTF(_logPrinter.get(), TAEL_INFO, "In TradeLogic::associate. Setting currentlyTrading = 1 for %s", _dm->symbol(cid));
  _currentlyTrading[ cid ] = 1;
  //   addTouch( cid );  // removed because addTouch uses _dm->curtv() which is not available on initialization
  return true;
}

bool TradeLogic::disassociate(int cid) {
  TAEL_PRINTF(_logPrinter.get(), TAEL_INFO, "%-5s TradeLogic::disassociate called", _dm->symbol(cid) );
  cancelAllOrders( cid, OrderCancelSuggestion::DISASSOCIATE );
  _associations[cid] = 0; 
  _currentlyTrading[ cid ] = 0;
  return true;
}

void TradeLogic::update( const OrderUpdate& ou ) { 
  addTouch(ou.cid()); 
  ou.snprint( buffer, BUF_SIZE );
  TAEL_PRINTF(_logPrinter.get(), TAEL_INFO, "%-5s TL::update(OU): %s", _dm->symbol(ou.cid()), buffer );
}
