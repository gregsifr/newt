/*
  At a high-level, mediation between internal components is as follows:
  - Priority high enough to cross:
    - Cross.
    - And cancel outstanding orders from other internal components.
  - Priority <= 1 bps:
    - Allocate capacity to FollowInvis, and to FTL (with JQ backup).
  - Priority > 1 bps, but not high enough to cross:
    - Allocate capacity to JoinQueue.

  Notes:
  - Based on user-feedback (from Quan - 20091028), we changed the intefaction of the 
    component TLCs slightly.  For the short-term, the code will now attempt to cross
    IFF priority >= 3 bps.

  Implementation Notes:
*/

#include "PriorityComponent.h"
#include <cl-util/float_cmp.h>

/*****************************************************************
  PriorityComponent
  TradeLogicComponent that includes:
  - JoinQueueComponent
  - FollowLeaderComponent w JQ backup.
  - CrossComponent
  and uses priority "knob" to mediate between those sub-components.

  
*****************************************************************/

PriorityComponent::PriorityComponent(AlphaSignal *signal)
  : TradeLogicComponent(),
    _signal(signal),
    _iot(),
    _jqt(),
    _ftl(),
    _cross(100, signal),      // (100, signal)
    _takeInvis(2500, signal),
    _useCross(true),
    _useTakeInvis(false),
    _lastCapacityCross(0),
    _lastCapacityTakeInvis(0),
    _lastCapacityIOT(0),
    _lastCapacityFTL(0),
    _lastCapacityJQ(0)/*,
    _reportlog(*(new tael::LoggerConfiguration((size_t) MAX_BINARY_BUFFER_FILE_SIZE)))*/
{
  _lastCapacityCross.resize( _dm->cidsize(), vector<int>(2,0) ); // cidsize*2 2-dimensional array of zeros
  _lastCapacityTakeInvis.resize( _dm->cidsize(), vector<int>(2,0) ); // cidsize*2 2-dimensional array of zeros
  _lastCapacityIOT.resize(   _dm->cidsize(), vector<int>(2,0) );
  _lastCapacityFTL.resize(   _dm->cidsize(), vector<int>(2,0) );
  _lastCapacityJQ.resize(    _dm->cidsize(), vector<int>(2,0) );
  factory<CrossComponent>::insert(only::one,&_cross);
  factory<TakeInvisibleComponent>::insert(only::one,&_takeInvis);
  factory<FollowLeaderJoinQueueBackup>::insert(only::one,&_ftl);
  /*_reportFile = "/spare/local/guillotine/log/report.log";
  int fd = open(_reportFile.c_str(), O_RDWR | O_CREAT | O_APPEND, 0644);
  if (fd > -1) {
	  _reportfld.reset(new tael::FdLogger(fd));
	  _reportlog.addDestination(_reportfld.get());
  }*/
}

PriorityComponent::~PriorityComponent() {

}

/*
 SuggestOrderPlacements:
 Details of mediation between different components seems keep changing in
 this code.

 1 method is simple:  Rank components in order, and allocate capacity in that
 order, e.g.:
 - Allocate all capacity to crossing.
 - After crossing, allocate remaining capacity to step-up.
 - After stepping up, allocate remaining capacity to FTL (JQ backup).
 - After FTLJQ:
   - If FTLJQ didnt suggest any orders, allocate remaining capacity
     to JQ.
   - If FTLJQ did suggest any orders, dont allocate any capacity to
     JQ.
   ==> This is because FTL is really designed to handle cases where theres 
       a new queue level, not JQ, so in those cases we profer to stick with
       FTL, and not let JQ place any additional orders.
  This method has the advantage of being conceptually simple, and of avoiding
  many overfills.
  It is probably also suboptimal from a transaction cost point of view.  In particular,
    it causes us to lose queue position when we place with components that have low fill
    probability but give good fills (when they do fill).

  We try a different method:
  - Basically, for each possible combination of (2) components, we decide whether
    we want the capacity used by component #1 to reduce the capacity used by component
    #2.  
  - Looking at the current specifific set of components (CROSS, TAKE_INVIS, IOT,
    FTL, AND JQ), we find only 2 cases where we want capacity allcoated to 1 component to
    reduce capacity available to another:
    - CROSS --> TAKE_INVIS.
    - FTL --> JQ.
    Otherwise, capacity used by 1 component should not reduce capacity allocated to
    another.
*/
void PriorityComponent::suggestOrderPlacements( int cid, Mkt::Side side, int tradeLogicId, 
						int numShares, double priority,
						vector<OrderPlacementSuggestion> &suggestions ) {
  suggestions.clear();
  vector<OrderPlacementSuggestion> subComponentSuggestions;
  int sharesUsedCross = 0, sharesUsedTakeInvis = 0, sharesUsedIOT = 0, sharesUsedFTL = 0;//, sharesUsedJQ = 0;
  int capacityCross = 0, capacityTakeInvis = 0, capacityIOT = 0, capacityFTL = 0, capacityJQ = 0;
  double priorityCross = 0.0, priorityTakeInvis = 0.0, priorityIOT = 0.0, priorityFTL = 0.0, priorityJQ = 0.0;

  // Ask CROSS for OrderPlacementSuggestions.
  // CROSS is called first, and thus gets all numShares.
  
  capacityCross = (_useCross)?numShares:0;
  
  _lastCapacityCross[ cid ][ side ] = capacityCross;
  if( capacityCross > 0 ) {
    // first, reduce the current outstanding size placed by this component from "new" capacity
    sharesUsedCross = _centralOrderRepo->totalOutstandingSize( cid, side, _cross.componentId(), tradeLogicId );
    priorityCross = componentPriority(cid, side, tradeLogicId, numShares, priority, OrderPlacementSuggestion::CROSS); 
    _cross.suggestOrderPlacements( cid, side, tradeLogicId, capacityCross - sharesUsedCross, 
				   priorityCross, subComponentSuggestions );
    //TAEL_PRINTF(&_reportlog, TAEL_INFO, "%s cross:%d", _dm->symbol(cid), capacityCross - sharesUsedCross);
    sharesUsedCross += ordersSize( subComponentSuggestions );
    suggestions.insert(suggestions.end(),subComponentSuggestions.begin(), subComponentSuggestions.end());
    subComponentSuggestions.clear();
  }

  // Ask TakeInvisible for OrderPlacementSuggestions.
  // TakeInvis gets numshares - any shares used by CROSS.
  
  capacityTakeInvis = (_useTakeInvis)?(numShares - sharesUsedCross):0;
  _lastCapacityTakeInvis[ cid ][ side ] = capacityTakeInvis;
  if( capacityTakeInvis > 0 ) {
    // first, reduce the current outstanding size placed by this component from "new" capacity
    sharesUsedTakeInvis = _centralOrderRepo->totalOutstandingSize( cid, side, _takeInvis.componentId(), tradeLogicId );
    priorityTakeInvis = componentPriority(cid, side, tradeLogicId, numShares, priority, OrderPlacementSuggestion::TAKE_INVISIBLE);
    _takeInvis.suggestOrderPlacements( cid, side, tradeLogicId, capacityTakeInvis - sharesUsedTakeInvis, 
				       priorityTakeInvis, subComponentSuggestions );
    //TAEL_PRINTF(&_reportlog, TAEL_INFO, "%s takeInvis:%d", _dm->symbol(cid), capacityTakeInvis - sharesUsedTakeInvis);
    sharesUsedTakeInvis += ordersSize( subComponentSuggestions );
    suggestions.insert(suggestions.end(),subComponentSuggestions.begin(), subComponentSuggestions.end());
    subComponentSuggestions.clear();
  }

  // Ask IOT for suggestions.
  // IOT gets all numShares of capacity.
  capacityIOT = numShares;
  _lastCapacityIOT[ cid ][ side ] = capacityIOT;
  if( capacityIOT > 0 ) {
    sharesUsedIOT = _centralOrderRepo->totalOutstandingSize( cid, side, _iot.componentId(), tradeLogicId );
    priorityIOT = componentPriority(cid, side, tradeLogicId, numShares, priority, OrderPlacementSuggestion::FOLLOW_INVTRD);
    _iot.suggestOrderPlacements( cid, side, tradeLogicId, capacityIOT - sharesUsedIOT, 
				 priorityIOT, subComponentSuggestions );
    //TAEL_PRINTF(&_reportlog, TAEL_INFO, "%s iot:%d", _dm->symbol(cid), capacityIOT - sharesUsedIOT);
    sharesUsedIOT += ordersSize( subComponentSuggestions );
    suggestions.insert( suggestions.end(), subComponentSuggestions.begin(), subComponentSuggestions.end() );
    subComponentSuggestions.clear();
  }

  // Ask FTL for suggestions:
  // FTL gets all numShares of capacity.
  //capacityFTL = numShares;
  // GVNOTE: Temporarily disabling the FTL component by setting capacity to 0
  capacityFTL = 0;
  _lastCapacityFTL[ cid ][ side ] = capacityFTL;
  if ( capacityFTL > 0) {
    // We don't want to wait until we cancel the former FTL-order before following a new leader.
    // Thus, we do not subtract here the size of open FTL-orders from the capacity (but only from the capacity of JQ)
    sharesUsedFTL = 0;
    priorityFTL = componentPriority(cid, side, tradeLogicId, numShares, priority, OrderPlacementSuggestion::FOLLOW_LEADER);
    _ftl.suggestOrderPlacements(cid, side, tradeLogicId, capacityFTL - sharesUsedFTL, 
				priorityFTL, subComponentSuggestions);
    //TAEL_PRINTF(&_reportlog, TAEL_INFO, "%s ftl:%d", _dm->symbol(cid), capacityFTL - sharesUsedFTL);
    sharesUsedFTL = ordersSize( subComponentSuggestions );
    suggestions.insert( suggestions.end(), subComponentSuggestions.begin(), subComponentSuggestions.end() );
    subComponentSuggestions.clear();
    sharesUsedFTL += _centralOrderRepo->totalOutstandingSize( cid, side, _ftl.componentId(), tradeLogicId );
  }

  // Ask JQ for suggestions:
  // JQ gets numShares - sharesUsedFTL.
  capacityJQ = numShares - sharesUsedFTL;
  _lastCapacityJQ[ cid ][ side ] = capacityJQ;
  if( (capacityJQ > 0) && (sharesUsedFTL<=0) ) {
    // Note: JQ takes care itself for not risking getting over-fills too frequently. That's partly why 
    // we don't bother subtracting from numShares the current open orders
    priorityJQ = componentPriority(cid, side, tradeLogicId, numShares, priority, OrderPlacementSuggestion::JOIN_QUEUE);
    _jqt.suggestOrderPlacements( cid, side, tradeLogicId, capacityJQ, priorityJQ, subComponentSuggestions );
    //TAEL_PRINTF(&_reportlog, TAEL_INFO, "%s jq:%d", _dm->symbol(cid), capacityJQ);
    suggestions.insert( suggestions.end(), subComponentSuggestions.begin(), subComponentSuggestions.end() );
    subComponentSuggestions.clear();
  }
  return;
}

///  Note:  Current implementation assumes that, on every wakeup, the containing
///     TradeLogic/TLC calls suggestOrderPlacement(..., side, ...) exactly once,
///     and suggestOrderCancels(..., side, ...) exactly once, in that order.
void PriorityComponent::suggestOrderCancels( int cid, unsigned int bidCapacity, unsigned int askCapacity, double priority, 
					     int tradeLogicId, vector<OrderCancelSuggestion> &cancelSuggestions ) {

  cancelSuggestions.clear();
  vector<OrderCancelSuggestion> subComponentSuggestions;

  // Ask CROSS to cancel its own undesired orders.
  _cross.suggestOrderCancels( cid, _lastCapacityCross[cid][Mkt::BID], _lastCapacityCross[cid][Mkt::ASK], 
			      priority / TakeLiquidityPriorityScale, tradeLogicId, subComponentSuggestions );
  cancelSuggestions.insert( cancelSuggestions.end(), subComponentSuggestions.begin(), subComponentSuggestions.end() );
  subComponentSuggestions.clear();

  // Ask TakeInvisible to cancel its own undesired orders.
  _takeInvis.suggestOrderCancels( cid, _lastCapacityTakeInvis[cid][Mkt::BID], _lastCapacityTakeInvis[cid][Mkt::ASK], 
				  priority / TakeLiquidityPriorityScale, tradeLogicId, subComponentSuggestions );
  cancelSuggestions.insert( cancelSuggestions.end(), subComponentSuggestions.begin(), subComponentSuggestions.end() );
  subComponentSuggestions.clear();

  // Ask IOT to cancel its own undesired orders.
  _iot.suggestOrderCancels( cid, _lastCapacityIOT[cid][Mkt::BID], _lastCapacityIOT[cid][Mkt::ASK], priority, tradeLogicId, 
			    subComponentSuggestions );
  cancelSuggestions.insert( cancelSuggestions.end(), subComponentSuggestions.begin(), subComponentSuggestions.end() );
  subComponentSuggestions.clear();

  // Ask FTL to cancel its own undesired orders.
  _ftl.suggestOrderCancels( cid, _lastCapacityFTL[cid][Mkt::BID], _lastCapacityFTL[cid][Mkt::ASK], priority, tradeLogicId, 
			    subComponentSuggestions );
  cancelSuggestions.insert( cancelSuggestions.end(), subComponentSuggestions.begin(), subComponentSuggestions.end() );
  subComponentSuggestions.clear();

  // Ask JQ to cancel its own undesired orders.
  _jqt.suggestOrderCancels( cid, _lastCapacityJQ[cid][Mkt::BID], _lastCapacityJQ[cid][Mkt::ASK], priority, tradeLogicId, 
			    subComponentSuggestions );
  cancelSuggestions.insert( cancelSuggestions.end(), subComponentSuggestions.begin(), subComponentSuggestions.end() );
  subComponentSuggestions.clear();

  return;
}

int PriorityComponent::ordersSize( vector<OrderPlacementSuggestion> &suggestions ) {
  int ret = 0;
  vector<OrderPlacementSuggestion>::const_iterator it;
  for( it=suggestions.begin(); it!=suggestions.end() ;it++ ) 
    ret += it -> _size;
  return ret;
}

double PriorityComponent::componentPriority(int cid, Mkt::Side side, int tradeLogicId, 
					    int numShares, double tlPriority, 
					    OrderPlacementSuggestion::PlacementReason pr) {
  if (pr == OrderPlacementSuggestion::CROSS) {
    return tlPriority / TakeLiquidityPriorityScale;
  }
  if (pr == OrderPlacementSuggestion::TAKE_INVISIBLE) {
    return tlPriority / TakeLiquidityPriorityScale;
  }
  return tlPriority;
}
