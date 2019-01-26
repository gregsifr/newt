#include "VolatilityTracker.h"
#include "OpenTracker.h"
#include "DataManager.h"
#include "HFUtils.h"
#include "AlphaSignal.h"

const string RELEVANT_LOG_FILE = "misc";

VolatilityTracker::VolatilityTracker(int sampleMilliSeconds, int numSamplePoints, 
				     int minSamplePoints, AlphaSignal *fvSignal) 
  :
  _sampleMilliSeconds(sampleMilliSeconds),
  _numSamplePoints(numSamplePoints),
  _minSamplePoints(minSamplePoints),
  _sampleNumber(0),
  _retBufV(0),
  _lastPriceV(0),
  _sampleStartTime(),
  _marketOpen(false),
  _fvSignal(fvSignal)
{
  _dm = factory<DataManager>::find(only::one);
  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in VolatilityTracker::VolatilityTracker)" ); 
  _openT = factory<OpenTracker>::get(only::one);
  if( !_openT )
    throw std::runtime_error( "Failed to get OpenTracker from factory (in VolatilityTracker::VolatilityTracker)" );   
  _exchangeT =  factory<ExchangeTracker>::get(only::one) ;
  if( !_exchangeT )
    throw std::runtime_error( "Failed to get ExchangeTracker from factory (in VolatilityTracker::VolatilityTracker)" ); 
  _ddebug = factory<debug_stream>::get( RELEVANT_LOG_FILE );
  if( !_ddebug )
    throw std::runtime_error( "Failed to get DebugStream from factory (in VolatilityTracker::VolatilityTracker)" ); 

  _retBufV.resize(_dm->cidsize());
  _lastPriceV.assign(_dm->cidsize(), 0.0);
  //_sampleStartTime = _dm->curtv();    Unclear what happens when _dm->curtv() is called before event loop starts.
  // with this commented out, need to ensure that addSample() sets _sampleStartTime on the 1st received sample.

  for (int i=0;i<_dm->cidsize();i++) {
    _retBufV[i] = new CircBuffer<double>(numSamplePoints);
  }  
  _dm->add_listener(this);
}

VolatilityTracker::~VolatilityTracker() {
  for (int i=0;i<_dm->cidsize();i++) {
    delete _retBufV[i];
  }
}

void VolatilityTracker::addSample() {
  // 1st sample:
  // Just get mid-prices.
  if (_sampleNumber++ == 0) {
    populateCurrentPrices();
    //_sampleStartTime = _dm->curtv();  Should be set from market open TimerUpdate.tv(),
    //  not _dm_>curtv(), as these two timers can sometimes have different usec resolution,
    //  and as other timer-based widgets generally use market open TimerUpdate.tv() for
    //  sample start TV.
  } else {
    // Subsequent sample:
    // Copy last sample point mid prices, so that they can be used
    //   to compute returns.
    vector<double> lastMidPriceV = _lastPriceV;
    // Compute current sample point mid prices.
    populateCurrentPrices();
    // Use last sample point mid prices + current mid prices to compute
    //   per period returns.
    vector<double> returnV;
    populateReturns(lastMidPriceV, _lastPriceV, returnV);
    // Append computed returns to each ret buf.
    addReturnSamples(returnV);
  }  
}

void VolatilityTracker::populateCurrentPrices() {
  vector<double> cpy = _lastPriceV;
  HFUtils::bestMids(_dm.get(), cpy, _lastPriceV, _fvSignal);
}

void VolatilityTracker::addReturnSamples(vector<double> &retV) {
  unsigned int rvs = retV.size();
  assert(rvs == (unsigned int)_dm->cidsize());
  for (unsigned int i=0;i<rvs;i++) {
    // Check whether stock has opened for trading.
    ECN::ECN  e    = _exchangeT -> getExchange( i );
    if (!_openT->hasOpened(i, e)) {
      continue;
    }
    double tsample = retV[i];
    CBD *tbuf = _retBufV[i];
    tbuf->add(tsample);

    // Temporary - For Debugging.
    //std::cout << "VolatilityTracker::addReturnSamples - adding sample: stock " << i 
    //	      << " sample " << tsample << std::endl;
  }
}

void VolatilityTracker::flushReturnSamples() {
  unsigned int sz = _retBufV.size();
  for (unsigned int i=0;i<sz;i++) {
    CBD *tbuf = _retBufV[i];
    tbuf ->clear();
  }
}

void VolatilityTracker::populateReturns(vector<double> &lastMidV, 
					vector<double> &curMidV, 
					vector<double> &retV) {
  double tret;
  unsigned int sz = lastMidV.size();
  assert(curMidV.size() == sz);
  retV.resize(sz);
  for (unsigned int i = 0;i < sz; i++) {
    if (lastMidV[i] > 0) {
      tret = ((curMidV[i] - lastMidV[i])/lastMidV[i]);
    } else {
      tret = 0.0;
    }
    retV[i] = tret;
  }
}

bool VolatilityTracker::getVolatility(int cid, double &fv) {
  assert((cid >= 0) && (((unsigned int)cid) < _retBufV.size()));
  CBD *tbuf = _retBufV[cid];
  int nsp = tbuf->size();
  if (nsp < _minSamplePoints) {
    return false;
  }
  double fvt;
  if (!tbuf->getStdev(&fvt)) {
    return false;
  }
  // Convert unscaled volatility to per minute volatility.
  double speriod = ((double)60000.0)/((double)_sampleMilliSeconds);
  double scale = sqrt(speriod);
  fv = fvt * scale;
  if (isnan(fv) || isinf(fv)) {
    return false;
  }
  return true;
} 


/*
  UpdateListener functions
*/
void VolatilityTracker::update(const TimeUpdate &au) {
  if (au.timer() == _dm->marketOpen()) {
    onMarketOpen(au);
  } else if (au.timer() == _dm->marketClose()) {
    onMarketClose(au);
  } 
  onTimerUpdate(au);
}

void VolatilityTracker::onMarketOpen(const TimeUpdate &au) {
  _marketOpen = true;
  _sampleStartTime = au.tv();
  _sampleNumber = 0;
  flushReturnSamples();
}

void VolatilityTracker::onMarketClose(const TimeUpdate &au) {
  _marketOpen = false;
}

void VolatilityTracker::onTimerUpdate(const TimeUpdate &au) {
  if (!_marketOpen) {
    return;
  }
  double msdiff = HFUtils::milliSecondsBetween(_sampleStartTime, au.tv());
  if (msdiff >= (_sampleNumber * _sampleMilliSeconds)) {
    addSample();
  }
}
