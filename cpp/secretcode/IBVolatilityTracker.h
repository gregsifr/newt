/*
  VolatilityTracker.h

  Simple widget that tracks recent short-term per-stock return volatility, estimated
    from HF data.
  Notes:
  - Version of VolatiltyTracker that uses different method of estimating prices,
    and thus of estimating volatility.
  - In this context, IB refers to IntervalBar, and the IBVolatilityTracker uses
    interval-bar data to estimate volatility.
  - Calculates per-period interval bar data (open, high, low, close, vwap) using
    reported trade prices.
  - Uses time-series of VWAP prices to estimate realized volatility.
  - Hopefully should thus be less sensitive to micro-structure effects (especially 
    discreteness of quote & trade prices) than a volatility estimate made from
    a mid price series.
  - Interval time-slices should be set long enough so that there is enough trading volume
    per slice to get a reasonable VWAP reading.  With small numbers of trades per
    interval, vol estimate may be dominated by bid-ask bounce, rather than true
    volatility of the underlying process.
  - In cases where no VWAP data is found for a given stock x time-slice, the last measured
    VWAP value is used again.  This may under-estimate volatility, but is probably more
    accurate than:
    - Using the mid price, which can over-estimate volatility due to bid-ask bounce.
    - Skipping the period, which can over-estimate volatility due to missing a time-period.
*/

#ifndef __IBVOLATILITY_TRACKER_H__
#define __IBVOLATILITY_TRACKER_H__

#include <vector>
using std::vector;

#include <cl-util/factory.h>
using namespace clite::util;

#include "VolatilityTracker.h"
#include "IntervalBar.h"

#include "DataManager.h"
#include "CircBuffer.h"
#include "ExchangeTracker.h"

#include "c_util/Time.h"
using trc::compat::util::TimeVal;

class OpenTracker;
class DataManager;

/*
  Design notes:
  - VolatilityTracker is designed to decide on its own when its time to end a
    sampling interval, and take a sample of prices.
  - IntervalBarTracker is also designed to decide on its own when its time
    to end a sampling interval, and present a new set of IntervalBar data 
    elements for use.
  - This makes composing them potentially slightly tricky.  
  - In the IBVolatilityTracker code, we assume that the underlying DataManager/
    event-queueing mechanism will deliver the same updates to all listeners, in
    the order in which the listeners are registered.  We then force the 
    IntervalBarTracker to have the same sampling frequency as the VolatilityTracker, 
    and register it first.  Thus, on each timer update on which the VolatilityTracker
    wants to sample, the IntervalBarTracker should also have a new sample ready.
*/
class IBVolatilityTracker : public VolatilityTracker {

  IntervalBarTracker *_bart;      // Internal.  Uses for populating interval bar data.

  // Override of base-class version.
  virtual void populateCurrentPrices();
 public:
  IBVolatilityTracker(int sampleMilliSeconds, int numSamplePoints, int minSamplePoints, AlphaSignal *fvSignal);
  virtual ~IBVolatilityTracker();

};


#endif  // #define __IBVOLATILITY_TRACKER_H__
