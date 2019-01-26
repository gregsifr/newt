#include "IBVolatilityTracker.h"
#include "HFUtils.h"

IBVolatilityTracker::IBVolatilityTracker(int sampleMilliSeconds, int numSamplePoints, 
					 int minSamplePoints, AlphaSignal *fvSignal) 
  :
  VolatilityTracker(sampleMilliSeconds, numSamplePoints, minSamplePoints, fvSignal)
{
  _bart = new TradeIntervalBarTracker(sampleMilliSeconds);
  // Should be taken care of in base class....
  //_dm->add_listener(this);
}

IBVolatilityTracker::~IBVolatilityTracker() {
  delete _bart;
}

/*
  populate current prices:
  - Initial call (at market open) - populate with mid prices as base class version.
  - Subsequent calls:
    - Walk through all stocks in sample.
    - Extract new IntervalBar data.
    - If IntervalBar is populated (at least 1 trade):
      - Extract VWAP and use that as price.
    - If IntervalBar is not populated (no trades):
      - Leave previous px in place (aka assume price did not change in interval).
*/
void IBVolatilityTracker::populateCurrentPrices() {
  int i;
  IntervalBar bar;
  bool barf, hvwap;
  double vwap;
  //char buf[256];

  // Temporary - for debugging only:
  //std::cout << "IBVolatilityTracker::populatecurrentPrices called" << std::endl;

 /* if (_dm->isMOCmode()) {
	  // In MOC mode we don't have prices
	  return;
  }*/
  if (_sampleNumber == 1) {
    VolatilityTracker::populateCurrentPrices();
    return;
  }

  // Walk through all stocks in population set.
  for (i=0;i<_dm->cidsize();i++) {
    // Try to make a guess at a representative (point in time) price for the interval.
    // We use the following heuristic:
    // - If there werre any trades in the interval, use the vwap of those trades.
    // - If there were no trades but we still believe we have valid market price, use
    //   (_fvSignal) adjusted mid pc as of the end of the sample.
    // - If there were no trades and we dont believe that we have a valid 
    //   quote from the end of the interval, then assume that the price did not
    //   change during the interval.
    barf = _bart->barFinished(i, bar);
    // Check that have new data bar.
    if (!barf) {
      // Should be changed to using asynch logger.
      //sprintf(buf, "IBVolatilityTracker::populateCurrentPrices - _sampleNumber %i, stock %i - unable to get bar data, using signal-adjusted mid px",
      //      _sampleNumber, i);
      //string bufStr(buf);
      //std::cout << bufStr << std::endl;
      if(!_dm->isMOCmode())
		  TAEL_PRINTF(_ddebug.get(), TAEL_ERROR, "IBVolatilityTracker::populateCurrentPrices - _sampleNumber %i, stock %i (%s) - unable to get bar data, using signal-adjusted mid px",
			  _sampleNumber, i, _dm->symbol(i));
    } 

    // If have data barm, ask it for vwap data (can fail if bar has no trades in it).
    hvwap = barf && bar.vwap(vwap);

    // Unable to get vwap (either because no new bar, or because bar had no trades in it).
    // --> Try using fvSignal adjusted mid.
    if (!hvwap) {
      hvwap = HFUtils::bestMid(_dm.get(), i, vwap, _fvSignal);
    }

    // If able to guess some price for stock, use it.
    if (hvwap) {
      _lastPriceV[i] = vwap;
    } else {
      // Otherwise, log warning and assume price is unchanged since last sampling.
      // Should be changed to using asynch logger.
      //sprintf(buf, "IBVolatilityTracker::populateCurrentPrices - _sampleNumber %i, stock %i - unable to get mid px, assuming px unchanged",
      //	      _sampleNumber, i);
      //string bufStr(buf);
      //std::cout << bufStr << std::endl;
      if(!_dm->isMOCmode())
    	  TAEL_PRINTF(_ddebug.get(), TAEL_ERROR, "IBVolatilityTracker::populateCurrentPrices - _sampleNumber %i, stock %i - unable to get mid px, assuming px unchanged",
      	      _sampleNumber, i); 
    }

    // No bar data - aka no trades during interval.
    // Just use previous _lastPriceV, aka dont change
    //   value in _lastPriceV.
  }
}

