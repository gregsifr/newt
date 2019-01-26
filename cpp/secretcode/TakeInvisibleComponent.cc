/********************************************************
  Component that tries to find invisible orders inside the CBBO to cross against.
  - Uses optional alpha signal to estimate true unconditional mid.
  - 
********************************************************/

#include "TakeInvisibleComponent.h"
#include "HFUtils.h"
#include "FeeCalc.h"
#include "BookTools.h"

#include <cl-util/float_cmp.h>
using namespace clite::util;

#include <cmath>
#include <stdlib.h>

using trc::compat::util::DateTime;

const static int IOC_TIMEOUT = 0;
const static double MPV = 0.01;     // Minimum price variation.  
                                    // Assumed to be 1 penny for all stocks in popultion set.

TakeInvisibleComponent::TakeInvisibleComponent(int waitMilliSeconds, AlphaSignal *fvSignal) 
  : TradeLogicComponent(),
    _exchangeT( factory<ExchangeTracker>::get(only::one) ),
    _tradeConstraints( factory<TradeConstraints>::get(only::one) ),
    _feeCalc( factory<FeeCalc>::get(only::one) ),
    _waitMilliSec(waitMilliSeconds),
    _lastOPTime(0),
    _lastOPNum(0),
    _priorityScale(1.0),
    _fvSignal(fvSignal),
    _printTrades(false),
    _enabled(true)
{
  _dm = factory<DataManager>::find(only::one);
  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in TakeInvisibleComponent::TakeInvisibleComponent)" ); 
  
  // Simple Market Capacity Model.  Target 10% of trailing avg (exp decay) queue size, with
  //   min size of 100 shares, and max size of 10000 shares.
  // Any need to also register _gmcm with e.g, DataManager to receive updates?
  // GVNOTE: Why do we use only 2% of the size? Seems too low. The initialization in the 
  // JoinQueueComponent or FollowLeaderComponent makes more sense - ( 0.05, 100, 1000 )
  _gmcm = new TAGlobalMarketCapacityModel( 0.02, 100, 10000 );
  if ( !_gmcm ) {
    throw std::runtime_error( "Failed to allocate TAGlobalMarketCapacityModel (in TakeInvisibleComponent::TakeInvisibleComponent)" ); 
  }

  _lastOPTime.resize( _dm->cidsize(), 0 );
  _lastOPNum.resize( _dm->cidsize(), 0 );

}

TakeInvisibleComponent::~TakeInvisibleComponent() {
  delete _gmcm;
}

/*
  Basic algo:
  - Apply a minimum time between order placements per stock.  This should be on the order
    of magnitude of the maximum observed round-trip time for order placements.  This is mainly
    intended to avoid having to guess whether previous in-flight orders have been filled or
    not.
  - Look for execution against invisible orders inside the CBBO.  These suggest that there *might* be additional
    invisible orders thatw e can try to hit.
    - If we see any such orders, that we would like to try to hit, issue the corresponding
      (fill-or-kill) orders.
  - Probe for invisible orders inside the CBBO that have not yet been revealed but that we'd
    be willing to hit.
    -  If we don't see any executions against invisible orders that we'd like to try to emulate,
       AND if its been sufficiently long since we last tried pinging for invisible orders:
    - Pick an ECN randomly from among the list of current valid trading destinations.
    - Calculate the most aggressive price to which wed be willing to cross (including take-liquidity fees).
    - If that price is inside the CBBO, submit a fill-or-kill order at that price.
    - Hope we discover a hidden limit order inside the CBBO at that price or better.
*/
void TakeInvisibleComponent::suggestOrderPlacements( int cid, Mkt::Side side, int tradeLogicId, 
					     int numShares, double priority,
					     vector<OrderPlacementSuggestion> &suggestions ) {
  // Base case - no new orders.
  suggestions.clear();
  if (!_enabled)
    return;
  // No capacity allocated.
  if( numShares <= 0 )
    return;

  // Dont know how market looks.  Dont place orders.
  SingleStockState *ss = _stocksState->getState(cid);
  if( !ss->haveNormalMarket() ) 
    return;

  // Have issued crossing order for stock too recently. Don't try to trade it in current wakeup context.
  if( HFUtils::milliSecondsBetween(_lastOPTime[cid], _dm->curtv()) < _maxFlightMilliSeconds ) 
    return;  

  // Check for execution of hidden orders, and see if there are any that we want to follow.
  int usedShares = suggestFollowOrderPlacements(cid, side, tradeLogicId, numShares, 
						priority, suggestions);

  // Trying to follow an invisible execution.  Dont also try to probe for invisible orders
  //   inside the CBBO.
  if (usedShares > 0) {
    return;
  }
  
  // No follow-invisible order placements.  Try probing for as-of-yet unrevealed 
  //   invisible orders.
  usedShares = suggestProbeOrderPlacements(cid, side, tradeLogicId, numShares, 
			      priority, suggestions);

  // Done.
}


/*
  Attempt to follow executions against invisible orders that occur
    inside of the stated market on the chosen ECN.
  In more detail:
  - When we see an execution against an invisible order, it suggests that there
    may be additional invisible liquidity at the same price level.
  - In theory, when we see such an execution, we should be able to tell
    whether it occurred at a price that wed be willing to pay to cross, and
    use that to determine whether to place at the same price.
  - In practice, it seems that only ISLD provides the market side on which
    the trade occurred (needed to determine whether wed be willing to cross
    to the same price).  As such, we use a more generic algorithm:
    - Whenever we see an invisible order execution for the specified stock:
    - Calculate most aggressive price that wed be willing to pay.
      - Including all fees.
      - Truncated inside the stated bid/ask on the specified ECN.
    - Issue a fill or kill order for specified stock x ecn x side x price.
      - For all allocated capacity.
*/
int TakeInvisibleComponent::suggestFollowOrderPlacements(int cid, Mkt::Side side, int tradeLogicId, 
							 int numShares, double priority,
							 vector<OrderPlacementSuggestion> &suggestions) {
  // No Shares to do - done.
  if (numShares <= 0) {
    return 0;
  }

  vector <const DataUpdate*> invisTrades = _iot.getInvisibleTradesSinceLastWakeup( cid );
  if (invisTrades.size() == 0) {
    return 0;
  }
  // Try on ISLD, BATS, ARCA, in that order.
  int ret;
  if( _tradeConstraints->canPlace(cid, ECN::ISLD) ) {

    ret = suggestFollowOrderPlacementsECN(cid, side, tradeLogicId, numShares, priority, suggestions, ECN::ISLD, invisTrades);
    if (ret > 0) {
      return ret;
    }
  }
  //
  // Turned off on BATS.
  //
  //ret = suggestFollowOrderPlacementsECN(cid, side, tradeLogicId, numShares, priority, suggestions, ECN::BATS, invisTrades);
  //if (ret > 0) {
  //  return ret;
  //}  

  if( _tradeConstraints->canPlace(cid, ECN::ARCA) ) {
    ret = suggestFollowOrderPlacementsECN(cid, side, tradeLogicId, numShares, priority, suggestions, ECN::ARCA, invisTrades);
    if (ret > 0) {
      return ret;
    }
  }
  return 0;
}

/*
  Version of above that operates on specified ECN only.
  Takes invisTrades as parameter to reduce number of queries against _iot
    (performance optimization).
*/
int TakeInvisibleComponent::suggestFollowOrderPlacementsECN(int cid, Mkt::Side side, int tradeLogicId, 
							 int numShares, double priority,
							 vector<OrderPlacementSuggestion> &suggestions,
							 ECN::ECN destECN, vector <const DataUpdate*> &invisTrades ) {
  // No shares to do - done.
  if (numShares <= 0) {
    return 0;
  }

  // No trades.  Done.
  if (invisTrades.size() == 0) {
    return 0;
  } 

  SingleStockState *ss = _stocksState->getState(cid);
  if( !ss->haveNormalMarket() ) 
    return 0;

  //Mkt::Side crossSide =( side == Mkt::BID ? Mkt::ASK : Mkt::BID );
  // Estimate the best quoted bid/ask on the specified ECN/book.
  double bpx, apx;
  int bsz, asz;
  if (!getECNBBO(cid, Mkt::BID, destECN, bpx, bsz) ||
      !getECNBBO(cid, Mkt::ASK, destECN, apx, asz)) {
    return 0;
  }   

  // Walk through trades and see if any are inside the top-level CBBO for the specified book only.
  vector <const DataUpdate*>::const_iterator it;
  bool place = false;
  for (it = invisTrades.begin(); it != invisTrades.end(); ++it) {
    const DataUpdate *tmp = (*it);
    if (cmp<4>::GT(tmp->price, bpx) && cmp<4>::LT(tmp->price, apx)) {
      place = true;
      break;
    }
  }

  // No invisible executions found inside whats believed to be the current BBO
  //   for specified stock x ecn.  Dont place any follow orders.
  if (place == false) {
    return 0;
  }

  //
  // Found an order, try to follow it:
  // - Estimate the most aggressive px willing to pay gives priority, signal, fees.
  // - Truncate px to be strictly inside stated BBO for specified ECN.
  // - Place fill-or-kill order for all allocated capacity at that level.
  //
 
  // Calculate most-aggressive price.
  double maPrice;
  if (!computeMostAggressivePrice(cid, destECN, side, numShares, priority, maPrice)) {
    return 0;
  }
  // Calculate order size.
  // In original version of code, we tried to do the entire numShares at once,
  // However, this leads to a bad selection bias - on the orders where we successfully 
  //   take liquidity against a large number of shares, that signals that there is a large
  //   hidden order going the other way, which is a signal that the stock-fair-value is
  //   worse than where the fv-signal used predicts (which ignores presence/absence of hidden 
  //   orders).  With this setup, we appear to tend to have no/small adverse selection 
  //   when we hit take a small number of invisibale shares, but bigger adverse eelection
  //   when we hit a large number of invisible shares.
  // The  theoretically correct solution to this is probably to try to build a signal that
  //   tries to better estimate fv in the presence of invisible liquidity, but for now
  //   we try to a) apply a maximum order size to TakeInvis orders, and b) apply some
  //   guesstimated fv-signal penalty that seems empirically roughly correct given that
  //   max order size.
  int osize = calculateOrderSize(cid, side, numShares, destECN, maPrice); 
  if (osize <= 0) {
    return 0;
  }

  // Make order.
  OrderPlacementSuggestion suggestion(cid, destECN, side, osize, maPrice, IOC_TIMEOUT,
				      tradeLogicId, _componentId, allocateSeqNum(), OrderPlacementSuggestion::TAKE_INVISIBLE,
				      _dm->curtv(), ss->bestPrice(Mkt::BID), ss->bestPrice(Mkt::ASK),
				      priority);
  suggestions.push_back(suggestion);
  markOP(cid);
  if (_printTrades) {
    printSuggestion(suggestion, "suggestFollowOrderPlacements");
  }
  return numShares;  
}


/*
  Calculate order size of TakeInvis submission.
  - Uses a heuristic of (at most) 10% of trailing avg queue size.
  - Max order size should probably be related to to the size of the
    _takeInvisSignalPenalty, and so 1 should probably not be adjusted
    without considering the impact on the other.
*/
int TakeInvisibleComponent::calculateOrderSize( int cid, Mkt::Side side, 
					       int numShares, ECN::ECN ecn,
					       double px ) {
  // See how much capacity we think the market can bear.
  // Use that as a max on the # of shares to place.
  int upperLimit;
  if( !_gmcm->globalCapacity(cid, side, px, upperLimit) ) {
    upperLimit = 100;
  }

  numShares = std::min( numShares, upperLimit );
  numShares = std::max( numShares, 0 );

  // Current logic assumes that all potentialt target destinations accept
  //   odd-lot liquidity-taking orders.  May want to add logic to convert
  //   order size to round-lot for some ECNs if that changes.
  return numShares;
}


/*
  Compute the most-aggressive price that component would be willing to pay for
    specified stock on specified ECN.
  - Including priority, alpha, and all estimated crossing fees.
  - side should indicate direction trying to go, not side against which the cross, e.g.
    if trying to BUY, side should be Mkt::BID.
*/
bool TakeInvisibleComponent::computeMostAggressivePrice(int cid, ECN::ECN destECN, Mkt::Side side, 
						    int numShares, double priority, double &maPrice) {
  // No Shares to do - done....
  if (numShares <= 0) {
    return false;
  }
  //
  // Extract stated mid + (alpha-adjusted) fair value (point estimate).
  SingleStockState *ss = _stocksState->getState(cid);
  if( !ss->haveNormalMarket() ) {
    return false; 
  }
  
  double mid = ss->mid();
  // Adjust mid for signal, if present.
  double alpha = 0.0;
  if (_fvSignal == NULL || !_fvSignal->getAlpha(cid, alpha)) {
    alpha = 0.0;
  }

  // Problem computing signal.  Return.
  if (isnan(alpha)) {
    return false;
  }

  //
  // Compute signal-derived fair value.
  //
  // TakeInvis orders only execute if they find hidden liquidity.  Empirically, the presence of 
  //   hidden liquidity seems to itself signal something about returns - and that returns after
  //   take-invis orders will be different (worse) than expeected by a fair-value signal that 
  //   does not include the presence/absence of those invisible orders.
  // Here we apply an empirically guesstimated signal penalty that is flat/constant across
  //   all TakeInvis orders.  This flat penalty guesstimate should probably be changed to a
  //   more rigorously derived model.
  alpha = HFUtils::makeLessAggressive(side, alpha, _takeInvisSignalPenalty);
  double fv = mid * (1.0 + alpha);

  // Per share fees:
  // - brokerage/DMA fees.
  // - ECN take liquidity fee.
  // - SEC & NASD fees.
  Mkt::Tape tape      = _exchangeT -> getTape( cid );
  double brokerageFeePerSh = _feeCalc->brokerageFee( 1 ); // this is calculation per-share
  double removeLiqFee = _feeCalc->takeLiqFee( destECN, tape );
  // SEC and NASD fees are for sell/short only, but in order to avoid buy-bias, we take them as half for each side
  double secFeePerSh = 0.5 * _feeCalc->getSECFee( numShares*mid ) / (double)numShares; 
  double nasdFeePerSh = 0.5 * _feeCalc->getNASDFee( numShares ) / (double)numShares; // this is calculation per-share 
  double totalFeePerSh = brokerageFeePerSh + removeLiqFee + secFeePerSh + nasdFeePerSh;

  // 
  // Calculate most aggressive price.
  if (side == Mkt::BID) {
    // Trying to BUY.
    maPrice = fv * (1.0 + priority/_priorityScale) - totalFeePerSh;
  } else {
    // Trying to SELL.
    maPrice = fv * (1.0 - priority/_priorityScale) + totalFeePerSh;
  }
  maPrice = HFUtils::roundLEAggressive(side, maPrice, MPV);

  // Truncate maPrice to be strictly inside the CBBO most aggressive price.
  // Note 1:  TakeInvisibleComponent and CrossComponent are designed to be complimentary,
  //   especially to have a precise division of labor.  CrossComponent tries to cross
  //   against visible limit orders on the book.  TakeInvisibleComponent tries to
  //   cross against invisible limit orders.  In theory, this suggests that they should
  //   not attempt to take the same shares, which simplifies capacity allocation between 
  //   them.  If you change this constraint, you should also re-visit the code that does
  //   capacity allocation between these two components.
  // Note 2: Original version of this code pushed price to be strictly inside 
  //   market on specified ECN only.
  // - New logic:
  //   - For BUYING: Price must be >= national best bid (avoid reg-NMS violations).
  //                 Price must be > local bid (avoid placing IOC orders that would just go into queue).
  //                 Price must be < local ask (avoid placing crossing orders).
  //   - For SELLING: Price must be <= national best ask (avoid reg-NAM violations).
  //                  Price must be < local ask (avoid placing IOC orders that would just go into queue).
  //                  Price must be > local bid (avoid placing crssing orders).
  double bpx, apx, cbpx, capx, minpx, maxpx;
  int bsz, asz;
  if (!getECNBBO(cid, Mkt::BID, destECN, bpx, bsz) ||
      !getECNBBO(cid, Mkt::ASK, destECN, apx, asz)) {
    return false;
  } 
  if (!HFUtils::bestPrice(_dm.get(), cid, Mkt::BID, cbpx) ||
      !HFUtils::bestPrice(_dm.get(), cid, Mkt::ASK, capx)) {
    return false;
  } 
  if (side == Mkt::BID) {
    // BUYING.
    // Price must be >= national best bid (avoid reg-NMS violations).
    // Price must be > local bid (avoid placing IOC orders that would just go into queue).
    // Price must be < local ask.
    // Price must not be more aggressive than WTP.
    minpx = std::max(cbpx, bpx + MPV);
    maxpx = apx - MPV;
    if (cmp<4>::LT(maxpx, minpx)) {
      return false;
    }
    if (cmp<4>::LT(maPrice, minpx)) {
      return false;
    }
    maPrice = HFUtils::pushToLEAggressive(side, maPrice, maxpx, MPV);
  } else{
    // SELLING
    // Price must be <= national best ask (avoid reg-NAM violations).
    // Price must be < local ask (avoid placing IOC orders that would just go into queue).
    // Price must be > local bid (avoid placing crssing orders).
    // Price must not be more aggressive than WTP.
    maxpx = std::min(capx, apx - MPV);
    minpx = bpx + MPV;
    if (cmp<4>::LT(maxpx, minpx)) {
      return false;
    }    
    if (cmp<4>::GT(maPrice, maxpx)) {
      return false;
    }
    maPrice = HFUtils::pushToLEAggressive(side, maPrice, minpx, MPV);
  }

  return true;
}

/*
  Utility function.  Fill in top-level stated size & px for specified stock x ecn x side.
*/
bool TakeInvisibleComponent::getECNBBO(int cid, Mkt::Side side, ECN::ECN destECN, double &px, int &sz) {
  double tpx;
  size_t tsz;
  lib3::MarketBook *mb = _dm->subBook(destECN);
  if (!getMarket(mb, cid, side, 0, &tpx, &tsz)) {
    return false;
  }  
  px = tpx;
  sz = tsz;
  return true;
}

/*
  Probe the market for hidden invisible orders.
  In more detail:
  - Cycle through target destimations.
    - Currently ISLD, ARCA, BATS.
  - If trading is not currently enabled on the chosen destination, skip
    probing for the stock x wakeup.
  - Calculate the most aggressive price willing to cross to and still be 
    within the priority budget (including tae-liquidity fees).
  - If that price is not inside the best visible market for that ECN.
    - Done, don't place.
  - If that price is inside the best visible market for that ECN.
    - Place a fill or kill order for specified stock x ecn x side x price,
      for all of the allocated capacity.

  Returns the total size of orders added (in shares).
*/
int TakeInvisibleComponent::suggestProbeOrderPlacements(int cid, Mkt::Side side, int tradeLogicId, 
				 int numShares, double priority,
				 vector<OrderPlacementSuggestion> &suggestions) {

  // No Shares to do - done.
  if (numShares <= 0) {
    return 0;
  }

  // Have issued crossing order for stock too recently. Don't try to trade it in current wakeup context.
  if( HFUtils::milliSecondsBetween(_lastOPTime[cid], _dm->curtv()) < _waitMilliSec ) 
    return 0;

  // Issue probe crossing order with some probability < 1, e.g. 5%.  
  // This is intended to make probe TAKE_INVIS orders less bursty by
  //   spreading orders from multiple stocks across multiple wakeups, rather
  //   than potentially having many stocks issue probe orders during
  //   the same wakeup.
  int rnd = rand();
  if (rnd > (_probeOrderProb * RAND_MAX)) {
    return 0;
  }

  // Cycle through ISLD, ARCA, BATS.
  // Theoretically might want to include EDGA, EDGX, BSX - but for now we exclude those
  //   as probably too low volume.
  // Turned off BATS.
  ECN::ECN destECN;
  switch(_lastOPNum[cid] % 2) {
  case 0:
    destECN = ECN::ISLD;
    break;
  case 1:
    destECN = ECN::ARCA;
    break;
  }

  // Check whether trading is enabled on the chosen destination.  If not:
  // - increment _lastOPNum[cid] so as to try a different ECN on the next attempt.
  // - dot place any orders.
  if( !_tradeConstraints->canPlace(cid, destECN) ) {
    _lastOPNum[cid]++;
    return 0;
  }

  //
  // Calculate the most aggressive price against which wed be willing to cross,
  //   on the specified ECN, including take-liquidity fees.
  //
  double maPrice;
  if (!computeMostAggressivePrice(cid, destECN, side, numShares, priority, maPrice)) {
    _lastOPNum[cid]++;  // Probably occurs because ECN is not enabled.  Try different ECN next time.
    return 0;
  }

  SingleStockState *ss = _stocksState->getState(cid); 
  // Mkt::Side crossSide =( side == Mkt::BID ? Mkt::ASK : Mkt::BID );
  // Calculate order size.
  // In original version of code, we tried to do the entire numShares at once,
  // However, this leads to a bad selection bias - on the orders where we successfully 
  //   take liquidity against a large number of shares, that signals that there is a large
  //   hidden order going the other way, which is a signal that the stock-fair-value is
  //   worse than where the fv-signal used predicts (which ignores presence/absence of hidden 
  //   orders).  With this setup, we appear to tend to have no/small adverse selection 
  //   when we hit take a small number of invisibale shares, but bigger adverse eelection
  //   when we hit a large number of invisible shares.
  // The  theoretically correct solution to this is probably to try to build a signal that
  //   tries to better estimate fv in the presence of invisible liquidity, but for now
  //   we try to a) apply a maximum order size to TakeInvis orders, and b) apply some
  //   guesstimated fv-signal penalty that seems empirically roughly correct given that
  //   max order size.
  int osize = calculateOrderSize(cid, side, numShares, destECN, maPrice); 
  if (osize <= 0) {
    return 0;
  }

  //
  // Make Order.
  //
  OrderPlacementSuggestion suggestion(cid, destECN, side, osize, maPrice, IOC_TIMEOUT,
				      tradeLogicId, _componentId, allocateSeqNum(), OrderPlacementSuggestion::TAKE_INVISIBLE,
				      _dm->curtv(), ss->bestPrice(Mkt::BID), ss->bestPrice(Mkt::ASK),
				      priority);
  suggestions.push_back(suggestion);
  markOP(cid);
  if (_printTrades) {
    printSuggestion(suggestion, "suggestProbeOrderPlacements");
  }
  return numShares;
}

void TakeInvisibleComponent::printSuggestion(OrderPlacementSuggestion &op, const string &reason) {
  char buf[256];
  DateTime dt(_dm->curtv());
  SingleStockState *ss = _stocksState->getState(op._cid);
  double bidpx = ss->bestPrice(Mkt::BID);
  double askpx = ss->bestPrice(Mkt::ASK);
  snprintf(buf, 256, "TakeInvisibleComponent::%s suggested trade - cid %i destECN %s side %s numShares %i maPrice %f  Mkt %f x %f  Time HH%iMM%iSS%iUS%i\n",
	   reason.c_str(), op._cid, ECN::desc(op._ecn), Mkt::SideDesc[op._side], op._size, op._price, 
	   bidpx, askpx, dt.hh(), dt.mm(), dt.ss(), dt.usec());
  std::cout << buf << std::endl;
}


void TakeInvisibleComponent::markOP(int cid) {
  _lastOPTime[cid] = _dm->curtv();
  _lastOPNum[cid]++;
}
