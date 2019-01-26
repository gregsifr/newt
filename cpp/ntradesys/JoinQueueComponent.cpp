#include "JoinQueueComponent.h"
#include "HFUtils.h"
#include "FeeCalc.h"
#include "Chunk.h"

#include <cl-util/float_cmp.h>

const int DEFAULT_TIMEOUT = 3600*8; // 8 hours
const int MIN_SIZE_TO_CONSTITUTE_A_LEVEL = 100; // no odd-lots please
const int MIN_REPLACED_INTERVAL = 100; // This #millisecs should pass before we place an order of the same symbol and price

const ECN::ECN ECNS_to_consider[] = { ECN::ISLD,
				      ECN::ARCA,
				      ECN::BATS,
				      ECN::NYSE,
                                      ECN::BSX};
const int N_ECNS_to_consider = sizeof(ECNS_to_consider) / sizeof(ECN::ECN);


const int BUF_SIZE = 1024;
char JQbuffer[BUF_SIZE];

vector< vector<ECN::ECN> > JoinQueueComponent::_ecnsSortedByFees;
bool JoinQueueComponent::_ecnsSortedByFeesInitialized = false;

JoinQueueComponent::JoinQueueComponent() 
  : TradeLogicComponent(),
    _stocksState( factory<StocksState>::get(only::one) ),
    _exchangeT( factory<ExchangeTracker>::get(only::one) ),
    _imbt( factory<ImbTracker>::get(only::one)),
    _gmcm( 0.05, 100, 1000 ),
    _lastPlacedOrders(0)
{  
  // I leave it here for now to make sure that the copy generated in the main function is still "alive" when this object is created
  _tradeConstraints = factory<TradeConstraints>::find(only::one);
  if( !_tradeConstraints )
    throw std::runtime_error( "Failed to get TradeConstraints from factory (in FollowLeaderComponent::FollowLeaderComponent)" );

  _lastPlacedOrders.resize( _dm->cidsize() );

  initializeVectorOfSortEcns();
}

/**
 * Try to have exactly one order outstanding for stock at any point in time, placed at the CBBO (ignoring odd lots).
 * Except:
 * - If market is invalid - don't place anything
 * - We may cancel and re-place (say, in a different px) in the same wakeup, resulting in 2 open orders for a short while
 */
void JoinQueueComponent::suggestOrderPlacements( int cid, Mkt::Side side, int tradeLogicId, 
						 int numShares, double priority,
						 vector<OrderPlacementSuggestion> &suggestions ) {
  suggestions.clear();

  SingleStockState *ss = _stocksState->getState(cid);

  // At target position, or not allocated any capacity, or don't have valid picture of market ==> Don't place.
  if( (numShares <= 0) || !ss->haveNormalMarket() ) 
    return;

  // Aggressiveness/priority specified for order too low vs guesstimated transaction
  //   costs for this model.  Don't place any orders.
  if( cmp<6>::LT(priority, EXPECTED_TC) )
    return;

  // Order price:  At (multi-exchange) CBBO, on desired side.
  double cbboPrice = ss->bestPrice( side );

  // Already have an order outstanding at cbbo price (on any exchange)? ==> Don't place another one.
  // This prevents JQT from following orders placed by other components.
  // It also prevents it from adding multiple orders at the same price on same exchange, or across exchanges.
  if( getMarketSize(_dm->orderBook(), cid, side, cbboPrice) > 0 )
    return;

  // To avoid place/pull loops
  /*
  if( _lastPlacedOrders[cid].price == cbboPrice && 
      HFUtils::milliSecondsBetween(_lastPlacedOrders[cid].time,_dm->curtv()) < MIN_REPLACED_INTERVAL )
    return;
  */
  // Do not place at more than 10 orders per second, if the price ticks up, then FLW should pick it up
  
  if( HFUtils::milliSecondsBetween(_lastPlacedOrders[cid].time,_dm->curtv()) < MIN_REPLACED_INTERVAL )
    return;
  
  // Try to place at CBBO unless that would result in too many shares outstanding that are reasonably likely to
  // get filled before the cancels of the outstanding orders go through.
  // By this we refer to orders which are at/more-aggressive than current cbbo
  // (Note that we may have orders that are considered 'open' in the OrderBook but already gone from the "masterBook")
  int outstandingLikelyFilled = _centralOrderRepo -> totalOutstandingSizeMoreEqAggresiveThan( cid, side, cbboPrice, _componentId );
  numShares -= outstandingLikelyFilled;
  if( numShares <= 0 )
    return;
  
  // Anti-Gaming Logic
  double imb,truemid;
  if (!_imbt->getImb(cid,imb,truemid))
    truemid=-1;
  if ((truemid>0) && (HFUtils::moreAggressive(side,cbboPrice,truemid)))
    return;
  
  // GVNOTE: Quick hack to ensure we don't send out order for JQ if the outstanding
  // orders at this level are for less than 200 shares. Added to avoid being gamed.
  int totalQSz = getMarketSize(_dm->masterBook(), cid, side, cbboPrice);
  if (totalQSz < 200) {
	  return;
  }

  // The JQ TLC is intended for use in the main daily open market trading session only.  
  Mkt::Tape loTape = _exchangeT -> getTape( cid );

  // Order route:  Somewhat detailed calculation, see details in code.
  ECN::ECN loRoute = chooseECN( cid, side, loTape, numShares, cbboPrice );
  if( loRoute == ECN::UNKN )
    return;

  // Order size:  Minimum of desired delta shares, max order size specified, 
  //   and roSize specified for stock.
  int loSize = calculateOrderSize( cid, side, numShares, loRoute, cbboPrice );
  if (loSize <= 0)
    return;

  // FOR DEBUGGING: Try track down the reasons for unsolicited cancels
  int nChars = snprintf(JQbuffer, BUF_SIZE, "%-5s JQ: Sizes of best level: Total: %d\t",
			 _dm->symbol(cid), totalQSz);
  for( int j=0; j<N_ECNS_to_consider; j++ )
    nChars += snprintf( JQbuffer+nChars, BUF_SIZE-nChars, ", %s: %d",
			ECN::desc(ECNS_to_consider[j]),
			getMarketSize(_dm->subBook(ECNS_to_consider[j]), cid, side, cbboPrice) );
  TAEL_PRINTF(_logPrinter.get(), TAEL_INFO, "%s", JQbuffer );

  // Place order(s).  To reduce information leakage, try to split large orders up into
  //   smaller chunks, which can then be placed/monitored/cancelled together.
  //
  int csqn = allocateSeqNum();
  vector<int> chunkSizes;
  HFUtils::chunkOrderShares(loSize, _chunk->getChunkSize() , chunkSizes);
  for (unsigned int i=0 ; i< chunkSizes.size(); i++) {
    if (chunkSizes[i] <= 0) continue;
    OrderPlacementSuggestion ops( cid, loRoute, side, chunkSizes[i], cbboPrice, DEFAULT_TIMEOUT, 
				  tradeLogicId, _componentId, csqn, OrderPlacementSuggestion::JOIN_QUEUE,
				  _dm->curtv(), ss->bestPrice(Mkt::BID), ss->bestPrice(Mkt::ASK),
				  priority);
    char buf[1024];
    ops.snprint(buf, 1024);
    TAEL_PRINTF(_logPrinter.get(), TAEL_WARN, "%s", buf);  // switch to DBG_INFO for production use.

    suggestions.push_back(ops);
  }
  if (chunkSizes.size() > 0) {
    _lastPlacedOrders[cid].update( cbboPrice, _dm->curtv() );
  }
  return;
}

/**
 * - Cancel iff:
 *  - Dont have valid picture of current market.
 *  - Order is no longer at CBBO.
 *  - We are all the outstanding volume at the CBBO (globally)
 *  - change in priority / capacity
 */
bool JoinQueueComponent::decideToCancel( int cid, const Order* order, unsigned int capacity, double priority,
					 OrderCancelSuggestion::CancelReason& cancelReason ) {

  SingleStockState *ss = _stocksState->getState(cid);

  // Check for cancel-reasons that are not market-dependant
  if( cmp<6>::LT(priority, EXPECTED_TC) ) {
    cancelReason = OrderCancelSuggestion::PRIORITY_TOO_LOW;
    return true;  
  } else if( order->sharesOpen() > (int) capacity ) {
    cancelReason = OrderCancelSuggestion::NO_CAPACITY;
    return true;    
  }

  // check for market-dependant cancel-reason
  else if( ss->haveNormalOrLockedMarket() && ss->haveNormalOrLockedExclusiveMarket()) {
    // was: ss->haveNormalMarket() && ss->haveNormalExclusiveMarket()
    // We change to checking for CROSS markets rather than locked or crossed markets as checking for
    //   locked or crossed markets seems to lead to a high ratio of pulls to JQ opportunities.
    // if we step up (by another TLC), we don't consider it as a new top level if it's only us there ==> we need to check the exclusive-mkt
    if( HFUtils::lessAggressive(order->side(), order->price(), ss->getExclusiveBestPrice(order->side())) ) { 
      cancelReason = OrderCancelSuggestion::LESS_AGGRESSIVE_THAN_CBBO;
      return true;  
    } else if( ss->strictlyInsideSpread(order->price()) ) { // May happen only if our order is still/already not in the market
      cancelReason = OrderCancelSuggestion::INSIDE_SPREAD;
      return true;
    } else if( queuePositionUnfavorable(order) ) { // When our order is in the top level
      cancelReason = OrderCancelSuggestion::Q_POSITION_UNFAVORABLE;
      return true;
    } else {
       double imb,truemid;
       if (!_imbt->getImb(cid,imb,truemid))
	 truemid=-1;
       if ((truemid>0) && (HFUtils::moreAggressive(order->side(),order->price(),truemid))){
	 cancelReason = OrderCancelSuggestion::MORE_AGGRESSIVE_THAN_TMID;
	 return true;
       }
    }
  } else { 
    // Market is invalid. But pulling on every locked/crossed book leads to too-frequent order cancels.  
    // So, as comproise, we cancel only if the book stays invalid for more than, say, 1 second
    TimeVal lastUpdateTV = ss->getLastChangeInMktStatus();
    if (HFUtils::milliSecondsBetween(lastUpdateTV, _dm->curtv()) > HFUtils::INVALD_MKT_CANCEL_MS) {
      cancelReason = OrderCancelSuggestion::NO_VALID_MARKET;
      return true;
    }
  }

  return false;
}


/*
  As TradeLogicComponent::suggestOrderCancels, except that the JoinQueueComponent
    can break up laarge single logical orders into multiple physical orders
    (e.g. to avoid information leakage).
  This implies that checking order size against capacity inside decideToCancel is not
    sufficient to catch the case where:
  - We have capacity X, and place JQ orders for X shares.
  - Our capacity decreases to Y < X, but we still have JQ orders out for X shares.
  This can occur when:
  - We get a new trade request, or
  - When another component fills some shares that JQ was also working on.
  In this case, logic should attempt to cancel orders down to the point where we have
    <= X orders outstanding.
*/
void JoinQueueComponent::suggestOrderCancels( int cid, unsigned int bidCapacity, unsigned int askCapacity, double priority, 
					       int tradeLogicId, vector<OrderCancelSuggestion> &cancelSuggestions ) {

  // Check which orders should be cancelled for order-specific reasons.
  TradeLogicComponent::suggestOrderCancels( cid, bidCapacity, askCapacity, priority, tradeLogicId, cancelSuggestions);

  // Catch the case where a logical order has been broken down into multiple physical orders, and
  //   where the total logic order size violates capacity limits.
  // How:
  // - JQ currently tries to have at most 1 logical order outstanding (and not in the process of
  //   being cancelled) at a time.
  // - Thus, the total outstanding order size of orders placed by this JQ component x tradeLogicID
  //   (not including orders in the process of cancelling) should be <= capacity.
  // - If that is not the case, cancel orders until we are at or under the allocated capacity.
  // - Given that the JQ component tries to have at most 1 logical order at a time, 
  //   with all component physical orders placed at the same ECN x side x price, the order
  //   in which component physical orders are cancelled should generally not matter.
  for (int i=0 ; i<=1 ; i++ )  {
    Mkt::Side side = (i == 0 ? Mkt::BID : Mkt::ASK);
    // How many shares outstanding.
    int totalOutstandingSize = _centralOrderRepo->totalOutstandingSizeNotCanceling( cid, side, _componentId, tradeLogicId );
    // How many shares already marked for cancellation by TradeLogicComponent::suggestOrderCancels.
    int totalAlreadyCancelled = numberSharesToCancel(cid, side, cancelSuggestions);
    totalOutstandingSize -= totalAlreadyCancelled;
    // How much capacity allocated.
    int capacity = (side == Mkt::BID ? bidCapacity : askCapacity);
    // No violation of total capacity.
    if (totalOutstandingSize <= capacity) {
      continue;
    }
    // Violation of total capacity - cancel orders until total capacity is no longer violated.
    // Current implementation does not attempt to be smart about order in which to cancel
    //    physical orders.
    vector<const OrderRecord*> ordRecs = _centralOrderRepo->getOrderRecords( cid, side, tradeLogicId, _componentId );
    vector<const OrderRecord*>::const_iterator it;
    for (it = ordRecs.begin(); it != ordRecs.end() && totalOutstandingSize > capacity; it++) {
      const OrderRecord& ordRec = *(*it);
      int orderId = ordRec.orderId();
      const Order* order = _dm -> getOrder( orderId );
      if (!order)
	continue;
      // make sure the order has not yet been canceled by us
      if( order->isCanceling() )
	continue;
      SingleStockState *ss =_stocksState -> getState( cid );
      OrderCancelSuggestion cancelSugg( orderId, _componentId, OrderCancelSuggestion::NO_CAPACITY, 
					ss->bestPrice(Mkt::BID), ss->bestPrice(Mkt::ASK),
					_dm->curtv() );
      cancelSuggestions.push_back( cancelSugg );
      totalOutstandingSize -= order->sharesOpen();
    }
  }
} 

/// Is our order the only one in this level?
bool JoinQueueComponent::queuePositionUnfavorable( const Order *order ) {

  //  If the order is not confirmed, I assume it doesn't show in the book yet (although this is not 
  //  necessarily true: we may get a DataUpdate before we get the mathin CONFIRM-order-update)
  if( !order->confirmed() || order->confirmed().action() != Mkt::CONFIRMED )
    return false;

  int totalVisibleVolm = _stocksState -> getState(order->cid()) -> bestSize( order->side() );
  int ourVisibleVolm = order->invisible() ? 0 : order->sharesOpen();
  if( ourVisibleVolm >= totalVisibleVolm ) 
    return true;
  return false;
}

/// return ECN::UNKN if there's no good ECN to place in
ECN::ECN JoinQueueComponent::chooseECN( int cid, Mkt::Side side, Mkt::Tape t, int size, double px ) {

  Mkt::Side otherSide = ( side == Mkt::BID ? Mkt::ASK : Mkt::BID );

  // Is each ECN still being considered as a valid destination?
  vector<char> possibleEcns( ECN::ECN_size, (char)false );

  // Get best price/size of both sides for every ECN in ECNS_INDICES_to_consider
  vector<double> thisSidePrices(  N_ECNS_to_consider, 0.0 );
  vector<double> otherSidePrices( N_ECNS_to_consider, 0.0 );
  vector<size_t> thisSideSizes(   N_ECNS_to_consider, 0 );
  vector<size_t> otherSideSizes(  N_ECNS_to_consider, 0 );

  // fills thisSidePrices, otherSidePrices, thisSideSizes, otherSideSizes for ECNs specified by ECNS_to_consider
  for( int i=0; i<N_ECNS_to_consider; i++ ) {
    ECN::ECN ecn = ECNS_to_consider[i];
    possibleEcns[ecn] = _tradeConstraints -> canPlace( cid, ecn, px );
    possibleEcns[ecn] = possibleEcns[ecn] && getTradableMarket( _dm->subBook(ecn), cid, side, 0, MIN_SIZE_TO_CONSTITUTE_A_LEVEL, 
								&thisSidePrices[i], &thisSideSizes[i] );
    possibleEcns[ecn] = possibleEcns[ecn] && getTradableMarket( _dm->subBook(ecn), cid, otherSide, 0, MIN_SIZE_TO_CONSTITUTE_A_LEVEL, 
								&otherSidePrices[i], &otherSideSizes[i] );
  }

  // Don't trade odd lots (size<100) on ARCA/NYSE.
  // On NYSE, we'll also have to truncate in the sequel if size > 100 but (size mod 100)!=0
  if( size < 100 ) {
    possibleEcns[ ECN::NYSE ] = (char)false;
    possibleEcns[ ECN::ARCA ] = (char)false;
  }

  // Limit destinations to those with other side less aggressive than px:
  int numPossible = 0;
  for( int i=0; i<N_ECNS_to_consider; i++ ) {
    ECN::ECN ecn = ECNS_to_consider[i];
    possibleEcns[ecn] = possibleEcns[ecn] && HFUtils::lessAggressive( otherSide, otherSidePrices[i], px );
    numPossible += possibleEcns[ecn];
  }
  if( numPossible == 0 ) return ECN::UNKN;
  if( numPossible == 1 ) 
    for( int i=0; i<N_ECNS_to_consider; i++ ) 
      if( possibleEcns[ECNS_to_consider[i]] ) return ECNS_to_consider[i];

  // Is Isld valid at this point? We may need this info after the next filter
  bool isValidIsld = possibleEcns[ ECN::ISLD ];

  // Limit destinations to those with top-level in this side GE aggressive than px (we don't like stepping up)
  numPossible = 0;
  for( int i=0; i<N_ECNS_to_consider; i++ ) {
    ECN::ECN ecn = ECNS_to_consider[i];
    possibleEcns[ecn] = possibleEcns[ecn] && HFUtils::geAggressive( side, thisSidePrices[i], px );
    numPossible += possibleEcns[ecn];
  }
  if( numPossible == 0 )
    // in case of odd-lot, route to ISLD even if it means you step up on it
    if( isValidIsld && size<100 ) return ECN::ISLD;
    else return ECN::UNKN;
  if( numPossible == 1 )
    for( int i=0; i<N_ECNS_to_consider; i++ ) 
      if( possibleEcns[ECNS_to_consider[i]] ) return ECNS_to_consider[i];

  // From the remaining destinations: choose the one with the highest liquidity providing rebates (good rebates are positive numbers),
  // and as a second sorting-key the one with the lowest liquidity taking fee (bad fees are also positive numbers)
  for( unsigned int i=0; i<_ecnsSortedByFees[t].size(); i++ ) {
    ECN::ECN ecn = _ecnsSortedByFees[t][i];
    if( possibleEcns[ecn] ) return ecn;
  }

  // should never get here
  return ECN::UNKN;
}

/// Calculate total size of the order(s) that should be placed.
int JoinQueueComponent::calculateOrderSize( int cid, Mkt::Side side, int numShares, 
					    ECN::ECN e, double px ) {
  numShares = std::max( numShares, 0 );
  
  // See how much capacity we think the market can bear.
  // Use that as a max on the # of shares to place.
  int upperLimit;
  if( !_gmcm.globalCapacity(cid, side, px, upperLimit) ) 
    return 0;
  numShares = std::min( numShares, upperLimit );

  // For NYSE stocks: don't trade nyse-odd-lots
  if ( ( e == ECN::NYSE ) || ( numShares>200) )
    numShares = numShares / 100 * 100;
  return numShares;
}

/// for each tape, this keeps a vector of ECNs sorted by increasing remove-liquidity fees and, as a second key, 
/// decreasing provide-liquidity-fees
void JoinQueueComponent::initializeVectorOfSortEcns() {

  factory<FeeCalc>::pointer feeCalc = factory<FeeCalc>::get(only::one);
  if( _ecnsSortedByFeesInitialized )
    return;
  else
    _ecnsSortedByFeesInitialized = true;

  _ecnsSortedByFees.resize( Mkt::NUM_TAPES );
  vector<ECN::ECN> basicVec( N_ECNS_to_consider );
  for( int i=0; i<N_ECNS_to_consider; i++ )
    basicVec[i] = ECNS_to_consider[i];
  vector<double> liquidityFees(ECN::ECN_size), liquidityRebates(ECN::ECN_size);

  for( int i=0; i<Mkt::NUM_TAPES; i++ ) {
    Mkt::Tape tape = (Mkt::Tape)i;
    for( int j=0; j<ECN::ECN_size; j++ ) {
      liquidityFees[j] = feeCalc->takeLiqFee( (ECN::ECN)j, tape );
      liquidityRebates[j] = feeCalc->addLiqRebate( (ECN::ECN)j, tape );
    }
    // A simple bubble-sort (recall this function is called only once, upon initialization)
    _ecnsSortedByFees[i] = basicVec;
    int j=0; // the beginning of the unsorted area in the vector
    while( j < N_ECNS_to_consider-1 ) {
      ECN::ECN ecn1 = _ecnsSortedByFees[i][j];
      ECN::ECN ecn2 = _ecnsSortedByFees[i][j+1];
      // check whether _ecnsSortedByFees[i][j] should come after _ecnsSortedByFees[i][j+1]
      // (i.e. if (provide_fee1 < provide_fee2) , or (provide_fee1 == provide_fee2 and remove_fee1 > remove_fee2)
      if( cmp<6>::LT(liquidityRebates[ecn1], liquidityRebates[ecn2]) ||
	  (cmp<6>::EQ(liquidityRebates[ecn1], liquidityRebates[ecn2]) && cmp<6>::GT(liquidityFees[ecn1], liquidityFees[ecn2])) ) {
	// ==> swap
	_ecnsSortedByFees[i][j]   = ecn2;
	_ecnsSortedByFees[i][j+1] = ecn1;
	j = std::max( 0, j-1 );
      }
      else 
	j++;
    }
    // Print the result
    int nChars = snprintf( JQbuffer, BUF_SIZE, "JoinQueueComponent::initializeVectorOfSortEcns: The sorted ECNs for %s are:", 
			  Mkt::TapeDesc[tape] );
    for( int j=0; j<N_ECNS_to_consider; j++ )
      nChars += snprintf( JQbuffer+nChars, BUF_SIZE-nChars, " %s", ECN::desc(_ecnsSortedByFees[i][j]) );
    TAEL_PRINTF(_logPrinter.get(), TAEL_INFO, "%s", JQbuffer );
  }
}
    
    
    
