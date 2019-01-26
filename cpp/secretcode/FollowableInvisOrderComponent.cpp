#include "InvisOrderTracker.h"
#include "FollowableInvisOrderComponent.h"
#include <cl-util/float_cmp.h>
#include "HFUtils.h"

/// On other ECNS apparently we cannot tell which side of the book the invisible order was on when we see the trade
/// This should be in the order in which we want to prioratize the ECNs
const ECN::ECN ECNS_we_can_use_for_flw_ins[] = { ECN::ISLD};
const int N_ECNS_we_can_use_for_flw_ins = sizeof(ECNS_we_can_use_for_flw_ins) / sizeof(ECN::ECN);

const int DEFAULT_TIMEOUT = 3600*8; // 8 hours

FollowableInvisOrderComponent::FollowableInvisOrderComponent() 
  :  TradeLogicComponent(),
     _stocksState( factory<StocksState>::get(only::one) ),
     _tradeConstraints( factory<TradeConstraints>::get(only::one) ),
     _iot(),
     _lvt() 
{}

bool FollowableInvisOrderComponent::anyFollowableInvisibleTrades( int cid, 
								  Mkt::Side side, 
								  bitset<ECN::ECN_size> &ecns, 
								  double &fp ) {
  ecns.reset();
  bool foundAny = false;
  vector<const DataUpdate*> dataupdates = _iot.getInvisibleTradesSinceLastWakeup( cid );
  for( unsigned int i=0; i<dataupdates.size(); i++ ) {
    const DataUpdate& du = *(dataupdates[i]);
    if( du.side != side ) continue;
    if( !safeToFollow(du) ) continue;
    if (foundAny == false) {
      ecns.set(du.ecn);
      fp = du.price;
      foundAny = true;
    } else if (HFUtils::lessAggressive(side, du.price, fp)) {
      ecns.set(du.ecn);
      fp = du.price;
    }
  }
  return foundAny;  
}

/*
  Check whether we know about any activity that would make it unsafe to follow du.
  Here defined as follows:
  - If we've seen any recent VISTRDs or PULLs at stock x ecn x side x px.
  ==> du may have represented someone clearing out all of the visible orders
      at a level, then hitting some of the invisible orders there also.
      Don't follow.
  - If we've seen any activity in stock x ecn x side x px, after du.
  ==> du.price is probably no longer strictly inside the CBBO, 
    Don't follow.
  - If placing order at px would result in crossing.
    ==> Not safe to follow.
  - If placing order would duplicate one that we already have outstanding
    at stock x ecn x side x px, dont follow.
    - This is useful to avoid cases where we see multiple invisible trades
      over 1...few wakeups, and try to follow each one.
  - If stock has not yet opened yet (global US equity mkt open for NASDAQ stocks,
    stock-specific specialist open for NYSE and AMEX listed stocks), then
    - crossing orders are likely to pay a very wide spread.
    - limit orders running an elevated risk of being picked off.
    ==> Either way, this dont place orders with this trading algo.
*/
bool FollowableInvisOrderComponent::safeToFollow(const DataUpdate &du) {

  // Check if there are any recent visible trades on the same stock x ecn x side.
  // That the invisible trade is not more aggressive than.
  // If so, the invisible trade is likely to be someone taking out 
  //   all of the visible trades on a level, then hitting some extra invisible
  //   orders behind them (which we dont want to follow).
  DataUpdate *lvt = _lvt.lastTrade(du.cid, du.ecn, du.side);
  if (lvt == NULL) {
    // Strange case.  Dont follow just to be sure.
    return false;
  }
  if (lvt->tv > du.tv) {
    // A subsequent visible trade in stock x ecn x side.
    // Pollutes signal & suggests that price is no longer strictly inside 
    //   the CBBO for stock x ecn.  Dont follow.
    return false;
  }
  double msDiff = HFUtils::milliSecondsBetween( lvt->tv, du.tv );
  if ((msDiff <= K_INVTRD_LOOKBACK_MSEC) && !HFUtils::moreAggressive(du.side, du.price, lvt->price)) {
    // A recent visible trade on same stock x ecn x side, against which
    //   the invtrd is not at a more aggressive price.  Suggests invtrd may be someone
    //   cleaning up tail end of level full on invisible trades.
    // Dont follow.
    return false;
  }

  // Check that du.px is still strictly inside CBBO.
  // ==> Otherwise, not safe to follow.
  SingleStockState *ss = _stocksState->getState(du.cid);
  if( !ss->strictlyInsideLastNormalCBBO(du.price) ) 
    return false;

  // Would duplicate an existing order that we placed.  Not safe to follow,
  //   to avoid responding with multiple orders of our own when multiple
  //   invisible trades occur during the interval between our order placement
  //   in response to the 1st such trade, and when that order placement actually
  //   arrives at the exchange.
  // NOTE: we used to look at the open size only at the specific ECN, but that doesn't seem important
  if( getMarketSize(_dm->orderBook(), du.cid, du.side, du.price) > 0 )
    return false;

  return true;
}

void FollowableInvisOrderComponent::suggestOrderPlacements( int cid, Mkt::Side side, int tradeLogicId, 
							    int numShares, double priority, 
							    vector<OrderPlacementSuggestion> &suggestions ) {
  // Empty out suggestions vector in case its being re-used.
  suggestions.clear();

  // At target position, or not allocated any capacity, or dont have valid picture of market.
  // - Dont place.
  SingleStockState *ss = _stocksState->getState(cid);
  if( (numShares <= 0) || !ss->haveNormalMarket() )
    return;

  // Aggressiveness/priority specified for order too low vs guesstimated transaction
  //   costs for this model.  Don't place any orders.
  if( cmp<6>::LT(priority, EXPECTED_TC) ) 
    return;

  // Check if there is room to improve
  if (cmp<3>::LE(ss->spread(),ss->minimalPxVariation()))
    return;
  
  // Check if we got any followable leader orders for specified stock x side.
  bitset<ECN::ECN_size> ecns;
  double tmp;
  bool afl = anyFollowableInvisibleTrades(cid, side, ecns, tmp);

  // No followable leaders - no orders.
  if( afl == false )
    return;

  // Try to step up by 1 cent
  double stepUpPx = HFUtils::makeMoreAggressive( side, ss->bestPrice(side), ss->minimalPxVariation() );

  // Make sure that we dont already have an order out for specified 
  //   stock x side x price.  This helps avoid:
  //   - Edge cases where we've already placed an FIT order, and gotten
  //     filled, but the book change arrives sooner than the trade reply.
  //   - Confusing interactions with other TLCs that try to step-up.
  if( getMarketSize(_dm->orderBook(), cid, side, stepUpPx) > 0 ) 
    return;

  ECN::ECN e = chooseECN( cid, ecns, stepUpPx );
  if (e != ECN::UNKN) {
    int osize = calculateOrderSize( numShares, e );
    OrderPlacementSuggestion ops( cid, e, side, osize, stepUpPx, DEFAULT_TIMEOUT,
				  tradeLogicId, _componentId, allocateSeqNum(), 
				  OrderPlacementSuggestion::FOLLOW_INVTRD,
				  _dm->curtv(), ss->bestPrice(Mkt::BID), ss->bestPrice(Mkt::ASK),
				  priority);
    suggestions.push_back(ops);
  }
}


/**
 * Choose the (currently single) ECN on which to place FIT limit order.
 * ecns should be populated with the set of ecns to choose from.  
 * - Currently uses very simple ECN choice:  Always follow on the
 *   same ECN on which the invisible trade occurred.  
 */
ECN::ECN FollowableInvisOrderComponent::chooseECN( int cid, const bitset<ECN::ECN_size> &ecns, double stepUpPx ) {

  for( int i=0; i<N_ECNS_we_can_use_for_flw_ins; i++ ) {
    ECN::ECN ecn = ECNS_we_can_use_for_flw_ins[i];
    if( ecns[ecn] && _tradeConstraints->canPlace(cid, ecn, stepUpPx) )
      return ecn;
  }
  return ECN::UNKN;  
}

/*
  Look at specific order and decide whether to cancel it.  By convention:
  - Order should be for specified stock (cid) and side.
  - TradeLogicComponent should only decide to cancel orders that come from same componentId
    and have OrderPlacementReason (aka TradeLogicComponent Id) that indicates that 
    they themselves recommended placing the order.
*/
bool FollowableInvisOrderComponent::decideToCancel( int cid, const Order* order, unsigned int capacity, double priority,
						    OrderCancelSuggestion::CancelReason& cancelReason ) {

  SingleStockState *ss = _stocksState->getState(cid);

  /*
    Reasons for cancelling:
    - Don't know where marker is for stock.  Increases chance of getting picked off  Cancel.
    - Order price is less aggressive than cbbo.  Order in wrong place.  Cancel.
      ==> Modified 20090811.  Market now needs to stay locked/crossed for some time
          interval before order is cancelled.
    - Queue position looks unfavorable / micro-structure model says likely to get picked off.
      Cancel.
      This last one should be very rare for following invisible trades (unless that signal\
        starts to get popular), but is probably worth checking anyway.
    - Order has been out for too long (>= 500 msec), AND
        no-one has followed us.
        Note:  Current definition says no-one has followed us across ALL
	  exchanges, not no-one has followed us at the specific exchange
	  on which we posted....
  */

    // Unfortunately, locked/crossed books seem to happen frequently when listening to multiple ECNs.
    // Pulling on every locked/crossed book leads to too-frequent order cancels.  
    // OTOH, we dont want to leave order hanging foreever if we lose connectivity.  
    // So, as comproise, we try to following heuristic:
    // - If the book stays invalid for more than 1 second, cancel.
  if( cmp<6>::LT(priority, EXPECTED_TC) ) {
    cancelReason = OrderCancelSuggestion::PRIORITY_TOO_LOW;
    return true;  
  } else if( order->sharesOpen() > (int) capacity ) {
    cancelReason = OrderCancelSuggestion::NO_CAPACITY;
    return true;    
  }
  else if( ss->haveNormalMarket() ) {
    if( ss->lessAggressiveThanCBBO(order->side(), order->price())) {
      cancelReason = OrderCancelSuggestion::LESS_AGGRESSIVE_THAN_CBBO;           ;
      return true;  
    }
    else if( order->confirmed() && order->confirmed().action()==Mkt::CONFIRMED &&
	     HFUtils::milliSecondsBetween(order->confirmed().tv(),_dm->curtv()) >= K_FOLLOW_INVTRD_TIMEOUT_MSEC &&
	     getMarketOrders(_dm->subBook(order->ecn()), order->cid(), order->side(), order->price()) <= 1 ) { 
      cancelReason = OrderCancelSuggestion::NO_FOLLOWERS;
      return true;
    }
  } else { 
    // Market is invalid. But pulling on every locked/crossed book leads to too-frequent order cancels.  
    // So, as comproise, we cancel only if the book stays invalid for more than, say, 1 second
    TimeVal lastUpdateTV = ss->getLastChangeInMktStatus();
    if (HFUtils::milliSecondsBetween(lastUpdateTV,_dm->curtv()) > HFUtils::INVALD_MKT_CANCEL_MS) {
      cancelReason = OrderCancelSuggestion::NO_VALID_MARKET;
      return true;
    }
  }
  
  return false;
}

/// Initial implementation, assumes that this TLC tries to have at most 1 order
/// outstanding at a time, and uses a default static max order size of 200 shares.
/// Since we don't trade on NYSE / ARCA there's no odd-lot problem here
int FollowableInvisOrderComponent::calculateOrderSize( int numShares, ECN::ECN ecn ) {
  numShares = std::max( numShares, 0 );
  numShares = std::min( numShares, 200 );
  return numShares;
}
