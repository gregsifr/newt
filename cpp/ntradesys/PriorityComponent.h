#ifndef __PRIORITYCOMPONENT_H__
#define __PRIORITYCOMPONENT_H__

#include "DataManager.h"
#include "TradeLogicComponent.h"
#include "JoinQueueComponent.h"
#include "FollowableInvisOrderComponent.h"
#include "FollowLeaderJoinQueueBackup.h"
#include "CrossComponent.h"

#include "TakeInvisibleComponent.h"
#include "AlphaSignal.h"

#include <vector>
using std::vector;

/**
 * Simple order-placement component that combines:
 * - FollowableInvisOrder.
 * - FollowLeader with JoinQueue as backup.
 * - JoinQueue.
 * - Cross.
 */
class PriorityComponent : public TradeLogicComponent {
 protected:
  /*std::string _reportFile;
  boost::shared_ptr<tael::FdLogger> _reportfld;
  tael::Logger _reportlog;*/
  // Unconditional alpha signal.
  AlphaSignal *_signal;

  // Order Placement Components.  Exist internally.
  FollowableInvisOrderComponent _iot;
  JoinQueueComponent            _jqt;
  FollowLeaderJoinQueueBackup   _ftl;
  CrossComponent                _cross;
  TakeInvisibleComponent        _takeInvis;
 
  bool _useCross;
  bool _useTakeInvis;
  vector< vector<int> > _lastCapacityCross;     /// cid * side
  vector< vector<int> > _lastCapacityTakeInvis; 
  vector< vector<int> > _lastCapacityIOT;
  vector< vector<int> > _lastCapacityFTL;
  vector< vector<int> > _lastCapacityJQ;

  const static double TakeLiquidityPriorityScale = 2.0;

  // Total number of shares suggested in vops.
  int ordersSize(vector<OrderPlacementSuggestion> &suggestions);

  // Calculate order priority that should be passed to specified
  //   component.
  virtual double componentPriority(int cid, Mkt::Side side, int tradeLogicId, 
			   int numShares, double tlPriority, OrderPlacementSuggestion::PlacementReason pr);

 public:
  PriorityComponent(AlphaSignal *signal);
  virtual ~PriorityComponent();

  // Suggest a set of order placements for specified stock x side, to be placed on
  //   current wakeup.  Intended to be called after populateFromDataQueue / other 
  //   populate on wakeup functions.
  virtual void suggestOrderPlacements( int cid, Mkt::Side side, int tradeLogicId, 
				       int numShares, double priority,
				       vector<OrderPlacementSuggestion> &suggestions );

  // Should populate <cancels> with bits specifying whether each order is to be cancelled.
  // Should also populate reasons with vector specifying reason for each cancel.
  virtual void suggestOrderCancels( int cid, unsigned int bidCapacity, unsigned int askCapacity, double priority, 
				    int tradeLogicId, vector<OrderCancelSuggestion> &cancelSuggestions );

  void setTakeInvis(bool enabled){ _useTakeInvis = enabled;}
  void setCross(bool enabled){ _useCross = enabled;}
};


#endif    // __PRIORITYCOMPONENT_H__
