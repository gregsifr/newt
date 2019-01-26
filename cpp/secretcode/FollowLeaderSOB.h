#ifndef __FOLLOWLEADERSOB_H__
#define __FOLLOWLEADERSOB_H__


#include "DataManager.h"
#include "FollowLeaderComponent.h"
#include "OrderStateTracker.h"
#include "ModelTracker.h"
#include <UCS1.h>

#include <cl-util/table.h>
#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>


using namespace clite::util;


class FollowLeaderSOB : public FollowLeaderComponentKH {
  
  factory<debug_stream>::pointer         _logPrinter;
  factory<SOBOrderRepo>::pointer         _sobRepo;
  factory<UCS1>::pointer _fvSignal;
  factory<MaxQueueSizeTracker>::pointer  _maxQSizeT;
  factory<ModelTracker>::pointer _modelTracker;
  //virtual const OrderPlacementSuggestion::PlacementReason _defaultReason;
 public:
  FollowLeaderSOB();
  virtual ~FollowLeaderSOB() {};  
  virtual void suggestOrderPlacements( int cid, Mkt::Side side, int tradeLogicID, 
				       int numShares, double priority,
				       vector<OrderPlacementSuggestion> &suggestions );
  virtual bool queuePositionUnfavorable( const Order* order );
  virtual double expectedTC() {return EXPECTED_TC;}

  /*
    If current queue size (for stock, on ecn, on side, at top level of book)
    < this K * max queue size (same qualifications), then pull order....
  */
  const static double EXPECTED_TC = 0.000075;  // 0.75 bps. (places it in between IOT and FL
};

#endif
