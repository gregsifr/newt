/*
  FollowLeaderJoinQueueBackupComponent.h
  TradeLogicComponent that uses FollowLeaderComponent as primary means of placing orders, 
  but also attempts to use JoinQueueComponent as a backup.

  In more detail:
  - Empirically FollowLeaderComponent seems to have low transaction costs (relative to the base case 
  of JoinQueueComponent), but also has a long tail of execution times for some stocks.
  - This component attempts to preserves te generally low transaction costs of 
    FollowLeader, while truncating the long-tail of execution times.
  - It tries to do that with a simple heuristic:
    - Use FollowLeader as primary means of order placement.
    - If the elapsed time since the last order-placement is too long, 
      switch to using JoinQueue, until a new opportunity for
      FollowLeader is found.

  Interaction with priority "knob" and TC estimation:
  - For ease of use for end customers, this component should treat JoinQueue as if it had the same TC
    estimates as FTL.  Aka, it should not happen that user specifies a priority that causes this component
    to place JQ orders but not FTL orders.
*/

#ifndef __FOLLOWLEADERJOINQUEUEBACKUP_H__
#define __FOLLOWLEADERJOINQUEUEBACKUP_H__

#include <vector>
using std::vector;

#include "FollowLeaderComponent.h"
#include "FollowLeaderSOB.h"
#include "JoinQueueComponent.h"
#include "ExecutionEngine.h" // for the TradeRequestsHandler queue
#include "HFUtils.h"


class FollowLeaderJoinQueueBackup : public TradeLogicComponent, public TradeRequestsHandler::listener {
 protected:

  FollowLeaderSOB      _ftl;
  JoinQueueComponent            _jq;

  vector<TimeVal> _lastFtlEventTime;   /// Per stock: Last time FTL suggested-placement/cxl/ we-got-new-trade-request etc.
                                       /// (idealy should include FTL-fills as well)
  bool _ignoreTickDown;
  
  const static int JQ_WAIT_TIME_MS = 150 * 1000;  // 2.5 minutes

  void markFtlEvent( int cid ) { _lastFtlEventTime[cid] = _dm->curtv(); }
  double milliSecondsSinceLastFtlEvent(int cid) const { return HFUtils::milliSecondsBetween(_lastFtlEventTime[cid], _dm->curtv()); }

  bool tickDown ( int cid, Mkt::Side s); // Did the price tick down from the last wakeup?
  virtual void update( const TradeRequest& tr );

 public:
  FollowLeaderJoinQueueBackup();

  /*
    TradeLogicComponent functions.
  */
  // Suggest a set of order placements for specified stock x side, to be placed on current wakeup.  
  virtual void suggestOrderPlacements( int cid, Mkt::Side side, int tradeLogicId, 
				       int numShares, double priority,
				       vector<OrderPlacementSuggestion> &suggestions);

  // Should populate <cancels> with bits specifying whether each order is to be cancelled.
  // Should also populate reasons with vector specifying reason for each cancel.
  virtual void suggestOrderCancels( int cid, unsigned int bidCapacity, unsigned int askCapacity, double priority, 
				    int tradeLogicId, vector<OrderCancelSuggestion> &cancelSuggestions );

  void setIgnoreTickDown ( bool val ) { _ignoreTickDown = val; }
};

#endif  //  __FOLLOWLEADERJOINQUEUEBACKUP_H__
