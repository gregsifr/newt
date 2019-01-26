/*
  VolatilityTracker.h

  Simple widget that tracks recent short-term (per stock) return volatility.
  Notes:
  - Base class which provides a default implementation for volatility estimation
    from HF data.
  - In default implementation - Px/return volatility is computed using mid -> mid returns over trading intervals.
    For short enough intervals, micro-structure effects (e.g. discreteness of prices)
    can severely impact this process.  
  - Current version of VolatilityTracker tracker code makes no effort to adjust for 
    micro-structure effects, and thus should not be used for estimating
    short-horizon volatility unless a rough estimate is sufficient.
  - Current version of VolatilityTracker code equal-weights return observations.
    May want to switch to weighted estimation using e.g. exponential decay (a la Barra).
  - Current version of VolatiltyTracker only starts sampling vol (for each stock)
    when the stock has been opened for trading.  It also stops sampling on market close.
  - Current version of VolatilityTracker only tries to sample prices on timer 
    updates (technically on the 1st wakeup after a timer update).  This implies that
    if the timer-freq is set to a value >> _sampleMilliSeconds, then the sampling
    will not actuall occur every _sampleMilliSeconds, and the per-minute volum estimate
    returned will be incorrect....
*/

#ifndef __VOLATILITY_TRACKER_H__
#define __VOLATILITY_TRACKER_H__

#include <vector>
using std::vector;

#include <cl-util/factory.h>
using namespace clite::util;

#include "DataManager.h"
#include "CircBuffer.h"
#include "ExchangeTracker.h"

#include "c_util/Time.h"
using trc::compat::util::TimeVal;

class OpenTracker;
class DataManager;
class AlphaSignal;

typedef CircBuffer<double> CBD;

class VolatilityTracker : public TimeHandler::listener {
 protected:
  factory<DataManager>::pointer _dm;                  // 
  factory<OpenTracker>::pointer _openT;               // Keeps track of whether each stock has been "opened" for trading, and also
                                                      //  whether the MARKET_CLOSE message has been received.
  factory<ExchangeTracker>::pointer _exchangeT;       // Keeps track of primary listing ecn for each stock.
  factory<debug_stream>::pointer _ddebug;             // For debug/error logging.

  int _sampleMilliSeconds;           // How freqently to sample mid-prices, in milli-seconds.
  int _numSamplePoints;              // Max number of sample points to keep (per stock).
  int _minSamplePoints;              // Min number of sample points to use for volatility estimation (per stock).
  int _sampleNumber;                 // Current sampling point (starts at 0).
  vector<CBD *> _retBufV;            // Vector of circbuffers, each holding per period returns per stock.
  vector<double> _lastPriceV;        // Holds stock prices (one per stock) as of last sampling point.
  TimeVal _sampleStartTime;          // Time at which mid-price sampling started.
  bool _marketOpen; 
  AlphaSignal * _fvSignal;           // Signal used to adjust stated mid --> fv when doing returtn calculations.
                                     // Null --> No adjustment.

  // Add a single time point sample (cross sectionally, aka for all stocks).
  virtual void addSample();
  // Populate _midPriceV with current prices.
  virtual void populateCurrentPrices();
  // Add retunrs to _retBufV.
  virtual void addReturnSamples(vector<double> &retV);
  virtual void flushReturnSamples();
  // Populae retV with returns from lastMidV --> curMidV.
  static void populateReturns(vector<double> &lastMidV, vector<double> &curMidV, vector<double> &retV);

  virtual void onMarketOpen(const TimeUpdate &au);
  virtual void onMarketClose(const TimeUpdate &au);
  virtual void onTimerUpdate(const TimeUpdate &au);
  
 public:
  VolatilityTracker(int sampleMilliSeconds, int numSamplePoints, int minSamplePoints, AlphaSignal *fvSignal);
  virtual ~VolatilityTracker();

  // Query for SD of per-minute returns, for specified stock.
  virtual bool getVolatility(int cid, double &fv);

  // Allows access to underlying buffer holding return series.
  // Advanced swim only. 
  virtual const CBD *retBuffer(int cid) {return _retBufV[cid];}

  /*
    Update listener functions.
  */
  virtual void update(const TimeUpdate &au);
};

#endif   // __VOLATILITY_TRACKER_H__
