#include "FollowLeaderSOB.h"
#include "BookTools.h"
#include "OrderStateTracker.h"
#include "ModelTracker.h"

#include <boost/dynamic_bitset.hpp>
using boost::dynamic_bitset;

const int DEFAULT_TIMEOUT = 3600*8;
//const OrderPlacementSuggestion::PlacementReason FollowLeaderSOB::_default_reason = OrderPlacementSuggestion::FOLLOW_LEADER_SOB;
const OrderPlacementSuggestion::PlacementReason _defaultReason = OrderPlacementSuggestion::FOLLOW_LEADER_SOB;

const ECN::ECN ECNS_we_can_follow[] = { ECN::ISLD, ECN::BATS,ECN::ARCA,ECN::NYSE,ECN::BSX}; 

const int N_ECNS_we_can_follow = sizeof(ECNS_we_can_follow) / sizeof(ECN::ECN);
FollowLeaderSOB::FollowLeaderSOB() :
  _logPrinter( factory<debug_stream>::get(std::string("trader")) ),
  _sobRepo( factory<SOBOrderRepo>::get(componentId())),
  _fvSignal(factory<UCS1>::get(only::one)),
  _maxQSizeT( factory<MaxQueueSizeTracker>::get(_defaultReason) ),
  _modelTracker( factory<ModelTracker>::get(only::one))
  //_defaultReason(OrderPlacementSuggestion::FOLLOW_LEADER_SOB)
{
  TAEL_PRINTF(_logPrinter.get(), TAEL_ERROR, "Init FLW_SOB with defRes=%s",OrderPlacementSuggestion::PlacementReasonDesc[_defaultReason]);
}




bool FollowLeaderSOB::queuePositionUnfavorable( const Order* order )
{
  
  double normFV;
  // Switch between model and heuristic 
  if ((order->ecn()==ECN::ISLD) && (_modelTracker->modelApplies(order->cid())))
    return _sobRepo->queuePositionUnfavorable(order,&normFV);
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
  else{
    int currQSize = getMarketOrders( _dm->subBook(order->ecn()), order->cid(), order->side(), order->price() );
    if( currQSize == 0 )
      // this should never happen because we assume we have an outstanding order at this px
      // Returning false. We should probably cancel for a different reason
      return false;
    
    int maxQSize = _maxQSizeT -> getMaxQueueSize( order->id() );
    if( maxQSize <= 0 ) {
      // This should not happen
      TAEL_PRINTF(_logPrinter.get(), TAEL_ERROR,  "%-5s ERROR: in FollowLeaderComponentKH::queuePositionUnfavorable, max-Q-size for order Id %d "
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
  // not needed
  return false;
}

void FollowLeaderSOB::suggestOrderPlacements( int cid, Mkt::Side side, int tradeLogicId, 
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
  SingleStockState *ss;
  ss = _stocksState->getState(cid);
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
      HFUtils::chunkOrderShares(osize, 100, chunkSizes);
      for (unsigned int i =0 ; i < chunkSizes.size(); i++) {
	if (chunkSizes[i] <= 0) continue;
	OrderPlacementSuggestion ops( cid, e, side, chunkSizes[i], loPrice, DEFAULT_TIMEOUT,
				      tradeLogicId, _componentId, csqn, _defaultReason,
				      _dm->curtv(), ss->bestPrice(Mkt::BID), ss->bestPrice(Mkt::ASK),
				      priority);
	suggestions.push_back(ops);
      }
    }
  }
  return;
}
