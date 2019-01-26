#include "FollowLeaderComponent.h"
#include "HFUtils.h"
#include "MarketCapacityModel.h"
#include "Chunk.h"
#include <BookTools.h>
#include <cl-util/float_cmp.h>

using namespace clite::util;
const OrderPlacementSuggestion::PlacementReason _defaultReason = OrderPlacementSuggestion::FOLLOW_LEADER;

const int DEFAULT_TIMEOUT = 3600*8; // 8 hours
const int MIN_SIZE_TO_CONSTITUTE_A_FOLLOWABLE_LEVEL = 100;

const ECN::ECN ECNS_we_can_follow[] = { ECN::ISLD,
					ECN::ARCA,
					ECN::BATS,
					ECN::EDGA,
                                        ECN::NYSE,
                                        ECN::BSX};

const int N_ECNS_we_can_follow = sizeof(ECNS_we_can_follow) / sizeof(ECN::ECN);

/*********************************************************************************
  FollowLeaderComponent code.
*********************************************************************************/
FollowLeaderComponent::FollowLeaderComponent() 

  : TradeLogicComponent(),
    _stocksState( factory<StocksState>::get(only::one) ),
    _tradeConstraints( factory<TradeConstraints>::get(only::one) )
    //_defaultReason(OrderPlacementSuggestion::FOLLOW_LEADER)

{
  // Simple Market Capacity Model.  Target 10% of trailing avg (exp decay) queue size, with
  //   min size of 200 shares, and max size of 2000 shares.
  // Any need to also register _gmcm with e.g, DataManager to receive updates?
  _gmcm = new TAGlobalMarketCapacityModel( 0.05, 100, 1000 );
}

FollowLeaderComponent::~FollowLeaderComponent() {
  delete _gmcm;
}

void FollowLeaderComponent::suggestOrderPlacements( int cid, Mkt::Side side, int tradeLogicId, 
						    int numShares, double priority,
						    vector<OrderPlacementSuggestion> &suggestions ) {
  // Empty out suggestions vector in case it's being re-used.
  suggestions.clear();

  // No capacity allocated - no orders to place.
  if (numShares <= 0)
    return;

  // Check that specified priority/aggressiveness is high enough that we guesstimate that this trading algo will work.
  if( cmp<6>::LT(priority,expectedTC()) ) 
    return;

  // Don't follow on invalid market
  SingleStockState *ss = _stocksState->getState(cid); 
  if( !ss->haveNormalOrLockedMarket() )
    return;

  // Check if we got any followable leader orders for specified stock x side.
  bitset<ECN::ECN_size> ecns;
  double loPrice;
  bool afl = anyFollowableLeaders( cid, side, ecns, &loPrice );

  if( afl ) {
    ECN::ECN e = chooseECN( cid, side, tradeLogicId, numShares, priority, loPrice, ecns );
    if( e != ECN::UNKN ) {
      // Calculate order size.
      int osize = calculateOrderSize(cid, side, numShares, e, loPrice); // truncate big-odd-lots on NYSE to round-lots
      // Allocate logical order sequence number.  Used to group physical orders that represent smaller
      //   pieces of large logical order.
      int csqn = allocateSeqNum();
      // Break order up into (mostly) round-lot sized chunks to reduce information leakage.
      vector<int> chunkSizes;
      HFUtils::chunkOrderShares(osize, _chunk->getChunkSize(), chunkSizes);
      for (unsigned int i =0 ; i < chunkSizes.size(); i++) {
	if (chunkSizes[i] <= 0) continue;
	OrderPlacementSuggestion ops( cid, e, side, chunkSizes[i], loPrice, DEFAULT_TIMEOUT,
				      tradeLogicId, _componentId, csqn, OrderPlacementSuggestion::FOLLOW_LEADER,
				      _dm->curtv(), ss->bestPrice(Mkt::BID), ss->bestPrice(Mkt::ASK),
				      priority);
	suggestions.push_back(ops);
      }
    }
  }
  return;
}

/*
  Do we see any "leader" orders that we feel comfortable following?  In this context,
   a "leader" order is defined as:
  - An order on an exchange on which we feel comfortable trading this heuristic
    (probably set of exchanges on which we can identify pulls from front of queue). 
  - That is:
    - >= some minimum order size (e.g. at least a round lot).
    - More aggressive than the CBBO as of the last wakeup.
    - Not an order placed by another order placement component working
      for the same trade-logic/order-component.
    - That occurs after the US market open (for NASDAQ listed stocks), or
      after the stock-specific specialist open (for NYSE and AMEX listed stocks).
  - Should also populate ecns with set of boolean values indicating whether
    each ecn (in set ECN::ECN) got a more aggressive limit order or not....
  - If returns true, should also populate *fillPx with the price at which the most aggressive such
    leader order detected occurred (if any).
*/
bool FollowLeaderComponent::anyFollowableLeaders( int cid, Mkt::Side side, bitset<ECN::ECN_size> &ecns, double *followablePx ) {
  ecns.reset();

  // Expanded version of logic, done for ISLD, ARCA, BATS, EDGA.
  // CBBO as of last wakeup.
  SingleStockState *ss = _stocksState -> getState( cid );
  
  if ( !ss->haveNormalMarket() )
    return false;

  double lastBestPx; // initialized as the last normal best price

  if ( !ss->previousWakeupNormalPrice(side, &lastBestPx) ) 
    return false;
  // The current best-px
  double bestPx;
  size_t bestSz = 0;
  getTradableMarket( _dm->masterBook(), cid, side, 0, MIN_SIZE_TO_CONSTITUTE_A_FOLLOWABLE_LEVEL, &bestPx, &bestSz );
  // Is it more aggressive? If not - return
  if( !HFUtils::moreAggressive(side, bestPx, lastBestPx) )
    return false;
  // Make sure it's not us 
  if( getMarketSize(_dm->orderBook(), cid, side, bestPx) > 0 )
    return false;

  TAEL_PRINTF(_logPrinter.get(), TAEL_INFO, "%-5s FTL: Sizes of best level: Total: %d ISLD: %d, BATS: %d, ARCA: %d, EDGA: %d, NYSE: %d BSX: %d ",
		       _dm->symbol(cid),
		       getMarketSize(_dm->masterBook(), cid, side, bestPx),
		       getMarketSize(_dm->subBook(ECN::ISLD), cid, side, bestPx),
		       getMarketSize(_dm->subBook(ECN::BATS), cid, side, bestPx),
		       getMarketSize(_dm->subBook(ECN::ARCA), cid, side, bestPx),
		       getMarketSize(_dm->subBook(ECN::EDGA), cid, side, bestPx),
		       getMarketSize(_dm->subBook(ECN::NYSE), cid, side, bestPx),
		       getMarketSize(_dm->subBook(ECN::BSX), cid, side, bestPx) );
  // Look for new leaders on the following ECNs:  ISLD, ARCA, BATS, EDGA. 
  // Right now we don't follow on NYSE because we cannot tell our queue position there.
  bool ret = false;
  for ( int i=0; i<N_ECNS_we_can_follow; i++ ) {
    ECN::ECN ecn = ECNS_we_can_follow[i];    
    // Only check ECNs that are currently being included and have valid state.
    if( _tradeConstraints->canPlace(cid, ecn, bestPx) &&
	getMarketSize(_dm->subBook(ecn), cid, side, bestPx) >= MIN_SIZE_TO_CONSTITUTE_A_FOLLOWABLE_LEVEL ) {
      ecns[ecn] = true;
      ret = true;
    }
  }
  if( ret ) *followablePx = bestPx;
  return ret;
}

/**
 * Choose the (currently single) ECN on which to place FTL limit order.
 * "ecns" should be populated with the set of ecns in which there are followable leaders.
 * - we try to follow on an ECN with a leader, in this priority: ISLD, ARCA, BATS, EDGA
 * - if we can't (mainly due to odd-lot issue) we step up on ISLD or BATS
 */
ECN::ECN FollowLeaderComponent::chooseECN( int cid, Mkt::Side side,  int tradeLogicId, 
					   int numShares, double priority,
					   double loPrice, 
					   bitset<ECN::ECN_size> &ecns ) {
  // For odd lots, don't trade on NYSE
  if( numShares < 100 ) {
    ecns.set( ECN::NYSE, false );
    ecns.set( ECN::ARCA, false );
  }

  // Simple case - we see a new leader on an ECN where were willing to directly follow.
  for( int i=0; i<N_ECNS_we_can_follow; i++ ) {
    if( ecns[ECNS_we_can_follow[i]] )
      return ECNS_we_can_follow[i];
  }
  
  // More complex case: Got a leader, but not on an ECN where we're willing to directly follow.
  //  - Try to step-up on ISLD 1st, BATS 2nd
  // - At this point mkt is known to be in a normal state (not locked/crossed/invalid), so we can comfortably step up
  if( _tradeConstraints->canPlace(cid,ECN::ISLD,loPrice) ) return ECN::ISLD;
  if( _tradeConstraints->canPlace(cid,ECN::BATS,loPrice) ) return ECN::BATS;
  return ECN::UNKN;
}         

/*
  Initial implementation, assumes that this TLC tries to have at most 1 order
    outstanding at a time, and uses a default static max order size of 200 shares.
*/
int FollowLeaderComponent::calculateOrderSize( int cid, Mkt::Side side, 
					       int numShares, ECN::ECN ecn,
					       double px ) {
  // See how much capacity we think the market can bear.
  // Use that as a max on the # of shares to place.
  int upperLimit;
  if( !_gmcm->globalCapacity(cid, side, px, upperLimit) ) 
    return 0;

  numShares = std::min( numShares, upperLimit );
  numShares = std::max( numShares, 0 );

  // For NYSE stocks, try to break orders that are > 100 shares but not round lots 
  // And over 200 shares place only completely-round-lots
  if( ecn == ECN::NYSE || numShares > 200 )  
    numShares = numShares / 100 * 100;
  return numShares;
}

/*
  As TradeLogicComponent::suggestOrderCancels, except that the FTL component
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
void FollowLeaderComponent::suggestOrderCancels( int cid, unsigned int bidCapacity, unsigned int askCapacity, double priority, 
						 int tradeLogicId, vector<OrderCancelSuggestion> &cancelSuggestions ) {

  // Check which orders should be cancelled for order-specific reasons.
  TradeLogicComponent::suggestOrderCancels( cid, bidCapacity, askCapacity, priority, tradeLogicId, cancelSuggestions);

  // Catch the case where a logical order has been broken down into multiple physical orders, and
  //   where the total logic order size violates capacity limits.
  // How:
  // - FTL currently tries to have at most 1 logical order outstanding (and not in the process of
  //   being cancelled) at a time.
  // - Thus, the total outstanding order size of orders placed by this FTL component x tradeLogicID
  //   (not including orders in the process of cancelling) should be <= capacity.
  // - If that is not the case, cancel orders until we are at or under the allocated capacity.
  // - Given that the FTL component tries to have at most 1 logical order at a time, 
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

/*********************************************************************************
  FollowLeaderComponentKH code.
*********************************************************************************/
FollowLeaderComponentKH::FollowLeaderComponentKH() 
  : _fvSignal(factory<UCS1>::get(only::one)),
    _maxQSizeT( factory<MaxQueueSizeTracker>::get(OrderPlacementSuggestion::FOLLOW_LEADER) ),
    _logPrinter( factory<debug_stream>::get(std::string("trader")) )
    //_defaultReason(OrderPlacementSuggestion::FOLLOW_LEADER)
{}

bool FollowLeaderComponentKH::decideToCancel(  int cid, const Order* order, unsigned int capacity, double priority,
					       OrderCancelSuggestion::CancelReason& cancelReason ) {

  SingleStockState *ss = _stocksState->getState( cid );

  // Unfortunately, locked/crossed books seem to happen frequently when listening to multiple ECNs.
  // Pulling on every locked/crossed book leads to too-frequent order cancels.  
  // OTOH, we dont want to leave order hanging foreever if we lose connectivity.  
  // So, as comproise, we try to following heuristic:
  // - If the book stays invalid for more than 1 second, cancel.

  // Check for cancel-reasons that are not market-dependant
  if( cmp<6>::LT(priority, expectedTC()) ) {
    cancelReason = OrderCancelSuggestion::PRIORITY_TOO_LOW;
    return true;  
  } else if( order->sharesOpen() > (int) capacity ) {
    cancelReason = OrderCancelSuggestion::NO_CAPACITY;
    return true;    
  }
  else if( ss->haveNormalOrLockedMarket() ) { 
    // was (ss->haveNormalMarket()).  We change to checking for CROSS
    //   markets rather than locked or crossed markets as checking for
    //   locked or crossed markets seems to lead to a high ratio 
    //   of pulls vs FTL opportunities.
    if( ss->lessAggressiveThanCBBO(order->side(), order->price())) {
      cancelReason = OrderCancelSuggestion::LESS_AGGRESSIVE_THAN_CBBO;           ;
      return true;  
    } else if( queuePositionUnfavorable(order) ) {
      cancelReason = OrderCancelSuggestion::Q_POSITION_UNFAVORABLE;
      return true;
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
  Check if queue has shrunk enough that we're worried about being picked off.
  If the partial book/queue tracker functionality doesn't know current || max queue
    size, then assume that the queue did not shrink too much 
  ==> Is this the correct assumption, or is it better to be conservative?.
  NOTE:
  Change FROM:
  current queue size orders < 0.5 * max queue size orders ==> PULL
  TO
  position of our order (in orders, relative to back of queue) / total # of orders (in orders)
    < 0.5 ==> Pull.
  E.g.
  200 shs      100 shs        1 shs (us)            600 shs
  - our position relative to front of queue is 3.
  - our position relative to back of queue is 2.
*/
bool FollowLeaderComponentKH::queuePositionUnfavorable( const Order* order )
{

  if (order->ecn()==ECN::NYSE){
    SingleStockState *ss = _stocksState->getState(order->cid());
    double mid = ss->mid();
    // Adjust mid for signal, if present.
    double alpha = 0.0;
    if (_fvSignal == NULL || !_fvSignal->getAlpha(order->cid(), alpha)) 
      return false;
    if (isnan(alpha)) 
      return false;
    double fv = mid * (1.0 + alpha);
    if (HFUtils::moreAggressive(order->side(),order->price(),fv)){
      return true;
    }
    return false;
  }
  
  int currQSize = getMarketOrders( _dm->subBook(order->ecn()), order->cid(), order->side(), order->price() );
  if( currQSize == 0 )
    // this should never happen because we assume we have an outstanding order at this px
    // Returning false. We should probably cancel for a different reason
    return false;

  int maxQSize = _maxQSizeT -> getMaxQueueSize( order->id() );
  if( maxQSize <= 0 ) {
    // This should not happen
    TAEL_PRINTF(_logPrinter.get(), TAEL_ERROR, "%-5s ERROR: in FollowLeaderComponentKH::queuePositionUnfavorable, max-Q-size for order Id %d "
			 "is 0", _dm->symbol(order->cid()), order->id() );
    return false;
  }

  // Try to guess the position of our order in the queue
  int positionInQ;
  if( !MaxQueueSizeTracker::guessPositionInQueue(_dm.get(), order, positionInQ) )
    // couldn't guess position
    return false;

  int ourPositionFromEnd = currQSize - positionInQ;
  if( (currQSize<maxQSize) && (((double)ourPositionFromEnd)/currQSize < K_ALLOWED_QUEUE_POSITION) ) {
    return true;
  }
  return false;
}
