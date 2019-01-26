/*
  MarketImpactModel.h

  Models for estimating market impact of various trading activities. 

  Simple market-impact model in which impact of trading is assumed to
    be dependent on # of shares and trading rate, but not on how orders
    are specifically placed in market.
  This approach seems fairly standard in execution scheduling literature.
*/

#ifndef __TRADINGIMPACTMODEL_H__
#define __TRADINGIMPACTMODEL_H__


#include <cl-util/factory.h>
using namespace clite::util;

#include "DataManager.h"
#include "AlphaSignal.h"
#include "ExchangeTracker.h"
#include "SpdTracker.h"
#include "FeeCalc.h"

#include "MarketImpactEstimate.h"

#include "TimeBucketSeries.h"

// For ImbTradingImpactModel
#include "ImbTracker.h"

#include <vector>
using std::vector;

typedef CircBuffer<double> CBD;

/*
  Base-class / interface definition for a widget that is used to keep track of 
    trading market impact data, and which can be queried to produce a guesstimate
    of the market impact of a potential set of executions.
*/
class TradingImpactTracker {
 public:
  TradingImpactTracker();
  virtual ~TradingImpactTracker();
   
  // Explicitly add a sample (called from outside).
  virtual void addSample(MarketImpactEstimate &est) = 0;

  // Guesstimate the impact of the trading specified number of shares at specified
  //   participation rate.
  virtual bool estimateImpact(int cid, int numShares, Mkt::Trade dir, 
				   double pRate, MarketImpactEstimate &fv) = 0;
};

/*
  Specific version of TradingImpactTracker.
  - Ingores participation rate.
  - Assumes that permanent market impact is linear in # of shares traded,
    and that temporary market impact is invariant of # of shares traded.
  - Uses trailing average of market impact per share for market-occurring 
    crossing trades to guesstimate impact of proposed trade.
  ==> These assumptions are probably semi-plausible for:
      - Permanent market impact.
      - Of trading at reasonable participation rate, aka rate at which
        a liquidity breakdown does not occur.
  Notes:
  - Breaks multi-lot trades up into round-lots, with total permanent impact per share
    spread evenly across all volume.
  - Keeps track of temporary market impact per trade (not per share).
  - Uses EW average of all trailing sample points over specified time-interval
    (e.g. 15 minutes) to guesstimate market impact.
    - Market impact estimates seem to have big outliers that contain real information,
      so outliers shoould be included in calculation.
    - EW average tends to oscillate less than e.g. exp decay avg in presence of such
      outliers.
    - A fixed time interval lookback, as opposed to a fixed # of trades, is used to 
      make the model more consistent across stocks that have very different trading 
      volume profiles.
*/
class AverageTradingImpactTracker : public TradingImpactTracker {
 protected:
  factory<DataManager>::pointer _dm;         // Exists externally.  Accessed via factory system.
  TimeBucketSeriesTracker _tiTracker;        // Permanent impact.
  TimeBucketSeriesTracker _piTracker;        // Temporary impact.
  vector<int> _numSamples;                   // Total number of sample points received, per stock.

 public:
  AverageTradingImpactTracker();
  virtual ~AverageTradingImpactTracker();

  /*
    TradingImpactTracker functions.
  */
  void addSample(MarketImpactEstimate &est);
  virtual bool estimateImpact(int cid, int numShares, Mkt::Trade dir, 
				   double pRate, MarketImpactEstimate &fv);
  virtual bool estimateTemporaryImpact(int cid, int numShares, Mkt::Trade dir, 
				       double pRate, double &fv);
  virtual bool estimatePermanentImpact(int cid, int numShares, Mkt::Trade dir, 
				       double pRate, double &fv);  
};


/*
  Base-class / interface-definition for simple market impact model that *does not*
    consider how orders are placed in the market.
*/
class TradingImpactModel {

 public:
  TradingImpactModel();
  virtual ~TradingImpactModel();

  // Estimate the market impact that trading specified # of shares would have.
  // numShares should be # of shares to trade in specified direction (always >= 0).
  // previousShares should be # of shares already traded in same direction, during 
  //   current trade request (almost always >= 0, theoretically could be < 0).
  virtual bool marketImpactTrading(int cid, int previousShares, int numShares, 
				   Mkt::Trade dir, double pRate, MarketImpactEstimate &fv) = 0;  
};

/*
  A version of a default trading impact model. 
  Designed to be Used in absence of information/beliefs about impact of
    specific trading strategy used. 
  Asumes that:
  - temporray impact is invariant of trade size.
  - permanent impact is linear in trade size with an intercept of zero.

  Notes:
  - TimeHandler functions, _marketOpen, _lastPrintTV data members for debugging only.
  - Estimates permanent impact per share using trailing average of permanent impact
    per share values.
*/
class AverageTradingImpactModel : public TradingImpactModel, public TimeHandler::listener {
  factory<DataManager>::pointer _dm;         // Exists externally.  Accessed via factory system.
  factory<debug_stream>::pointer _ddebug;    // Exists externally.  Accessed via factory system.
  factory<AverageTradingImpactTracker>::pointer _atiTracker;  // Exists externally.  Accessed via factory system.
  bool _marketOpen;  
  TimeVal _lastPrintTV;    
  
 public:
  AverageTradingImpactModel();
  virtual ~AverageTradingImpactModel();

  // Estimate the market impact that trading specified # of shares would have.
  // Parameters:
  // - numShares : number of shares to trade, in specified direction (always >= 0).
  // - previousShares:  number of shares previously traded in same stock x direction,
  //   over course of current trade-request.
  //   - Can be negative number to represent shares traded in the *opposite* direction.
  // In this simple formulation:
  // - pRate is ignored.
  // - temporary impact is populated with 0.
  // - permanent impact should be correctly populated.
  virtual bool marketImpactTrading(int cid, int previousShares, int numShares, 
				   Mkt::Trade dir, double pRate, MarketImpactEstimate &fv); 

  /*
    TimeHandler functions....
  */
  virtual void update(const TimeUpdate &au);
  
  virtual void printState();
};


/*
  Another attempt at a default trading impact model liquidity providing strategies.  
  Conceptual idea:
  - Assume that any shares that we displace generally continue to be marketed passively 
    at the top-level of the order book.
  - Estimate the accumulated effect of such shares in the order book over time.
  Implementation:
  - Periodically sample the market impact of one additional lot posted on the top
    level of the order book (each side or 1 side?), using scenario analysis features of
    Imb signal.
  - Also sample the top-level queue size (without additional shares) at which that market
    impact is estimated.
  - Query Imb signal for k value.  Use k value to estimate relative decay for each additional
    lot placed at top of order book.  For plausible k-values, this should be a concave (downward)
    function.
  - Market impact of 1st lot, plus queue size and k-value can be used to trace out total
    market impact function.
  Assumptions.  Assume that:
  - Our presence in order book does not encourage/discourage crossing orders from other 
    side of book.  Aka just as many crossing orders/share occur with our orders as without.
  - Our passive executions displace other passive executions, and 100% of those displaced
    shares continue to be passively marketed at the top-level queue of the specified stock.
  ==> These assumptions seem semi-plausible for a relatively stealthy/careful passive MM
      strategy that attempts to get good quality fills and participate at a relatively small
      fraction of volume.
*/
class ImbTradingImpactModel : public TradingImpactModel, public TimeHandler::listener {
 protected:
  factory<DataManager>::pointer _dm;         // Exists externally.  Accessed via factory system.
  factory<debug_stream>::pointer _ddebug;    // Exists externally.  Accessed via factory system.
  factory<ImbTracker>::pointer _imbSignal;   // Exists externally.  Accessed via factory system.
  int _sampleMilliSeconds;           // How freqently to market-impact of round-lot, and top-level queue-size.
  int _numSamplePoints;              // Max number of sample points to keep (per stock).
  int _minSamplePoints;              // Min number of sample points to use for volatility estimation (per stock).
  int _sampleNumber;                 // Current sampling point (starts at 0).
  TimeVal _startTV;                  // Time at 1st sampling point.
  bool _marketOpen;                  // is market currently open for regular session trading.
  vector<CBD*> _lotImpactV;          // Impact estimate series, of 1 round-lot marketed passively 
                                     //   at top-level queue, per stock.
  vector<CBD*> _queueSizeV;          // queue-size series, per stock.
  
  virtual void sampleAll();
  virtual void populateImpacts();
  virtual void flushImpactSamples();
  virtual void onMarketOpen(const TimeUpdate &au);
  virtual void onMarketClose(const TimeUpdate &au);
  virtual void onTimerUpdate(const TimeUpdate &au);
 public:
  ImbTradingImpactModel();
  ImbTradingImpactModel(ImbTracker *imbSignal);
  virtual ~ImbTradingImpactModel();

  // Estimate the market impact that trading specified # of shares would have.
  virtual bool marketImpactTrading(int cid, int previousShares, int numShares, 
				   Mkt::Trade dir, double pRate, MarketImpactEstimate &fv); 

  
  // As marketImpactTrading, except that it:
  // - breaks numShares up into round-lots + potentially 1 odd lot.
  // - populates an array with market impact estimates for each such lot.
  // Included as call to scalar version of marketImpactTrading takes
  //   O(numShares/100), and as some potential use cases involve calling
  //   scalar version once per potential traded lot - yielding an O(N^2)
  //   time, whereas the vector version can be computed once in O(1)
  //   time. 
  virtual bool marketImpactTrading(int cid, int previousShares, int numShares, 
				   Mkt::Trade dir, double pRate, vector<MarketImpactEstimate> &fv);

  virtual void update(const TimeUpdate &au);
};


#endif   // __TRADINGIMPACTMODEL_H__
