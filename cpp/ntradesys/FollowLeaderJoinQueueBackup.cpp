#include "FollowLeaderJoinQueueBackup.h"

#include <cl-util/float_cmp.h>
using namespace clite::util;
const int MIN_SIZE_TO_CONSTITUTE_A_FOLLOWABLE_LEVEL = 100;

FollowLeaderJoinQueueBackup::FollowLeaderJoinQueueBackup() 
  : _ftl(),
    _jq(),
    _ignoreTickDown ( false )
{
  _lastFtlEventTime.resize( _dm->cidsize(), 0 );
  _dm->add_listener( this );
}

void FollowLeaderJoinQueueBackup::update( const TradeRequest& tr ) {

  // This logic has a corner case in which if we have a JQ_Backup order outstanding, it will be cancelled 
  // This can cause us to lose our position in queue but happens very infrequently
  if( tr._previousTarget != tr._targetPos )
    markFtlEvent( tr._cid );
}

bool FollowLeaderJoinQueueBackup::tickDown( int cid, Mkt::Side side ){


  if (_ignoreTickDown)
    return false;
  
  SingleStockState *ss = _stocksState -> getState( cid );
  double lastBestPx; // initialized as the best-px from the former wakeup
  double bestPx;
  size_t bestSz = 0;
  if( !ss->previousWakeupPrice(side, &lastBestPx) )
    // No previous market against which to compare current px. Don't place orders.
    return false;  
  getTradableMarket( _dm->masterBook(), cid, side, 0, MIN_SIZE_TO_CONSTITUTE_A_FOLLOWABLE_LEVEL, &bestPx, &bestSz );
   if( HFUtils::moreAggressive(side, lastBestPx, bestPx) )
     return true;
   return false;
   
  
}

/// Decide how to allocate capacity between FTL and JQ. Pass along request accordingly.
void FollowLeaderJoinQueueBackup::suggestOrderPlacements(  int cid, Mkt::Side side, int tradeLogicId, 
							   int numShares, double priority,
							   vector<OrderPlacementSuggestion> &suggestions ) {
  suggestions.clear();

  // If expected FTL TC is above max aggressgiveness that user specified, don't suggest anything (not even with the JQ back up)
  if( cmp<6>::GT(_ftl.expectedTC(),priority) )
    return;
  
  _ftl.suggestOrderPlacements( cid, side, tradeLogicId, numShares, priority, suggestions );
  // Wait 2.5 mins from a tick down event before joining queue
  // This way we do not take profits earlier than we should
  // If the price doesnt move in all this time, then it might be a good time to join 
  // queue otherwise there is no harm in waiting
  
  if( ( suggestions.size() > 0 ) || tickDown(cid,side) ) {
    markFtlEvent(cid);
    return;
  }
  
  // No orders from FTL.  If sufficent time has elapsed since last FTL order, ask JQ for order placements.
  // Note:  Assumes JQ:expected_tc >= FTL:expected_tc....
  if( milliSecondsSinceLastFtlEvent(cid) >= JQ_WAIT_TIME_MS )
    _jq.suggestOrderPlacements( cid, side, tradeLogicId, numShares, JoinQueueComponent::EXPECTED_TC, suggestions );
  
  return;
}

/// Suggest order cancels.  Basic algo:
/// - FTL always gets same amt of capacity (numShares) that caller specified.
/// - If sufficient time has elapsed since the last FTL order-placement, then
///   JQ gets same amount of capacity, otherwise JQ gets zero capacity.
void FollowLeaderJoinQueueBackup::suggestOrderCancels(  int cid, unsigned int bidCapacity, unsigned int askCapacity, double priority, 
							int tradeLogicId, vector<OrderCancelSuggestion> &cancelSuggestions ) {

  vector<OrderCancelSuggestion> ftlCancelSuggestions, jqCancelSuggestions;

  // FTL always gets capacity.
  _ftl.suggestOrderCancels( cid, bidCapacity, askCapacity, priority, tradeLogicId, ftlCancelSuggestions );
  if( ftlCancelSuggestions.size() > 0 )
    markFtlEvent( cid );

  // Iff sufficiently long has elapsed since last FTL OP, then JQ gets capacity
  int jqBidCapacity = 0, jqAskCapacity = 0;
  if( milliSecondsSinceLastFtlEvent(cid) > JQ_WAIT_TIME_MS ) {
    jqBidCapacity = bidCapacity;
    jqAskCapacity = askCapacity;
  }

  double jqPriority = priority;
  if( cmp<6>::GE(priority, _ftl.expectedTC()) && cmp<6>::LE(priority, JoinQueueComponent::EXPECTED_TC) )
    jqPriority = JoinQueueComponent::EXPECTED_TC;
  _jq.suggestOrderCancels( cid, jqBidCapacity, jqAskCapacity, jqPriority, tradeLogicId, jqCancelSuggestions );

  // at last, concatenate the two cancel-suggestions-vectors
  cancelSuggestions.clear();
  cancelSuggestions.insert( cancelSuggestions.end(), ftlCancelSuggestions.begin(), ftlCancelSuggestions.end() );
  cancelSuggestions.insert( cancelSuggestions.end(),  jqCancelSuggestions.begin(),  jqCancelSuggestions.end() );
  
  return;
}



  
