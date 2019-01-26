
#ifndef __IMPLSHORTFALLCOMPONENT_H__
#define __IMPLSHORTFALLCOMPONENT_H__

#include "PriorityComponent.h"
#include "MarketImpactEstimate.h"
#include "MarketImpactModel.h"
#include "TradingImpactModel.h"
#include "ParticipationRateModel.h"
#include "RealizedMarketImpactTracker.h"
#include "IntervalVolumeTracker.h"

#include <vector>
using std::vector;

/*
  1st attempt at a component that attempts to intelligently trade-off short-term alpha, signal profile,
     expected participation rate, and market impact.
  This version represents a heuristic that attempts to cut off negative tails of current production
     scheduler (PriorityComponent).  In 1st version, it does not attempt to find an "optimal"
     trade schedule, but instead simply tries to be careful about market impact when deciding whether
     to take-liquidity or not.
  Basic approach:
  - Basic algo is as follows:
    - Assume base of of liquidity providing strategy. Estimate:
      - Expected trade completion time T w/ liqudity providing strategy.
      - Long-term alpha profile of trade (clean of our market impact) over T.
      - Forward-return from liquidity providing strategy (vs underlying alpha trajectory).
    - Potentially move trade forward in time from liquidity providing at end of trajectory 
      to liquidity-taking at current (wall or sim) time.
      - Estimate savings on shares S moved forward in time from being done at current price,
        not along later trajectory.
      - Balanace that savings against increased market-impact from taking liquidity now,
        spread along remainder of trade-request.
    - Use relative cost & savings to estimate an maximum WTP for crossing at current (wall or sim) time.
    - Trim max WTP to sane values.
    - Use max WTP as priority for CrossComponent. 
  - This should hopefully reduce some cases of extreme positive transaction costs that seem as if
    they may be due to market impact.
*/



/*
  1st implementation of a simple Implementation shortfall scheduler.
  Initial goal is to try to be relatively smart about setting priority on 
    liquidity-taking orders so as to not have excess market impact.
*/
class PriorityCutoffComponent : public PriorityComponent, public TradeRequestsHandler::listener {
 protected:
  /*
    Components accessed via factory system.
  factory<DataManager>::pointer _dm;
  factory<debug_stream>::pointer _ddebug;
  ==> In TradeLogicComponent
  */

  // Additional models needed.  These should not include state that is specific to
  //   a single top-level component/trading algo, and thus can be shared via factory
  //   system.
  // Used for scenario analysis of market impact of specific liquidity-taking orders.
  factory<TakeLiquidityMarketImpactModel>::pointer        _tlMIM;
  // Used for general info about avg market impact of liquidity-taking orders.
  factory<AverageTradingImpactModel>::pointer             _avgTIM;

  // Used for estimating market impact of liquidity provision,
  factory<ImbTradingImpactModel>::pointer                 _plTIM;
 
  // Used to guesstimate participation rates for liquidity-providing schedules.
  factory<DefaultProvideLiquidityPRM>::pointer            _plPRM;

  // Used to keep track of realized market impact - per stock * trade request.
  // Probably not explicitly called inside of PriorityCutoffComponent code, 
  //   but should be created to log realized market-impact guesstimates.
  factory<RealizedMarketImpactTracker>::pointer           _rmiTracker;

  // Used to keep track of recent spreads - over trailing 1 minute, sampled
  //   every second.
  factory<SpdTracker>::pointer                            _spdT;

  // USed to keep track of distribution of trailing tradig volume, per stock.
  factory<IBVolumeTracker>::pointer                       _volumeT;

  /*
    Non-public member functions.
  */
  // Calculate order priority that should be passed to specified
  //   component.
  virtual double componentPriority(int cid, Mkt::Side side, int tradeLogicId, 
			   int numShares, double tlPriority, OrderPlacementSuggestion::PlacementReason pr);

  // Estimate completion time for an order to transact <numShares> shares of stock <cid>, 
  //   assuming participation rate <pRate>.  Ignroes rounding effects e.g. due to queue waiting times.
  virtual bool estimateCompletionTime(int cid, int numShares, double pRate,
				      const TimeVal &curtv, TimeVal &fv);

  // Estimate amount of *total* alpha from beginning of trade request to specified
  //   ending time.
  virtual bool estimateSignalProfile(int cid, Mkt::Trade dir, double priority, 
				     const TimeVal &startTV, const TimeVal &endTV, 
				     double &fv);

  // Estimate the amount of *remaining* alpha in remaining life of tade request
  //   that started at earlier time.
  virtual bool estimateSignalProfile(int cid, Mkt::Trade dir, double priority, 
				     const TimeVal &startTV, const TimeVal &curTV, 
				     const TimeVal &endTV, double &fv);

  // Estimate volume participation rate for specified trading.
  virtual bool estimatePRate(int cid, Mkt::Trade dir, double priority, 
			     const TimeVal &startTV, const TimeVal &endTV,
			     double &fv);

  // Estimate the market impact of trading <numShares> shares of stock <cid>,
  //   in specified direction, at specified participation rate.
  // MarketImpact should be *absoluetly* signed quantity, not signed relative to trade
  //   direction.
  // MarketImpact estimate should include effects of any shares of same stock
  //   previously traded in the same stock, e.g. in the case where the market
  //   impact estimate is non-linear in size.
  virtual bool estimateMarketImpactPL(int cid, int numShares, Mkt::Trade dir, double pRate,
				      MarketImpactEstimate &fv);

  // Vectorized version of estimateMarketImpactPL.  Breaks numShares up into chunks
  //  and estimates cumulative market impact estimates for each chunk (in order), rather than just 
  //  for the aggregate.
  // Included as _plTIM->marketImpactTrading is a slow operation to do inside a market-data handling
  //  loop, and thus reducing # of calls to it seems like a good idea.
  virtual bool estimateMarketImpactPL(int cid, int numShares, Mkt::Trade dir, double pRate,
				      vector<MarketImpactEstimate> &fv);

  virtual bool estimateTransactionCosts(int cid, int additionalSize, Mkt::Trade dir,
					double priority, 
					const TimeVal &startTV, const TimeVal &curtv, 
					double &fv);
 public:
  PriorityCutoffComponent(AlphaSignal *stSignal);
  virtual ~PriorityCutoffComponent();

  /*
    TradeLogicComponent functions.
    - Currently uses PriorityComponent versions of suggestOrderPlacements & suggestOrderCancels.
  */


  /*
    UpdateListener functions.
  */
  virtual void update (const TradeRequest &tr);

  // Default priority if unable to calculate real priority - 0.1 bps.
  const static double DEFAULT_PRIORITY;

  //Guesstimated adverse fill of slow liquidity-providing strategy.
  const static double ADVERSE_FILL_PL;
};


#endif   // __IMPLSHORTFALLCOMPONENT_H__
