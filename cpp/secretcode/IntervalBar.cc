/*
  IntervalBar.cc
*/

#include <algorithm>
using namespace std;

#include "IntervalBar.h"
#include "HFUtils.h"
#include "KFRTSignal.h"

/******************************************************
  IntervalBar code
******************************************************/
IntervalBar::IntervalBar()  :
  _cid(-1),
  _intervalNumber(-1),
  _open(0.0),
  _close(0.0),
  _high(0.0),
  _low(0.0),
  _numSamples(0),
  _sumVol(0.0),
  _sumVolPx(0.0),
  _sumBidVol(0.0),
  _sumAskVol(0.0)
{

}

IntervalBar::IntervalBar(int cid, int intervalNumber) :
  _cid(cid),
  _intervalNumber(intervalNumber),
  _open(0.0),
  _close(0.0),
  _high(0.0),
  _low(0.0),
  _numSamples(0),
  _sumVol(0.0),
  _sumVolPx(0.0),
  _sumBidVol(0.0),
  _sumAskVol(0.0)
{

}

IntervalBar::IntervalBar(int cid, int intervalNumber, double bid, 
			 double ask) :
  _cid(cid),
  _intervalNumber(intervalNumber),
  _open((bid + ask)/2.0),
  _close((bid + ask)/2.0),
  _high(ask),
  _low(bid),
  _numSamples(1),
  _sumVol(0.0),
  _sumVolPx(0.0),
  _sumBidVol(0.0),
  _sumAskVol(0.0)
{

  
}

IntervalBar::~IntervalBar() {

}

bool IntervalBar::open(double &fv) const {
  if (_numSamples == 0) {
    return false;
  }
  fv = _open;
  return true;
}

bool IntervalBar::close(double &fv) const {
  if (_numSamples == 0) {
    return false;
  }
  fv = _close;
  return true;
}

bool IntervalBar::high(double &fv) const {
  if (_numSamples == 0) {
    return false;
  }
  fv = _high;
  return true;
}

bool IntervalBar::low(double &fv) const {
  if (_numSamples == 0) {
    return false;
  }
  fv = _low;
  return true;
}

bool IntervalBar::vwap(double &fv) const {
  if (_numSamples == 0) {
    return false;
  }
  fv = _sumVolPx / _sumVol;
  return true;
}

void IntervalBar::addSample(Mkt::Side side, double px, double vol) {
  if (vol <= 0) {
    return;
  }
  vol = std::min(vol, (double)BasicKFRTSignal::TRADE_SIZE_CIELING);  // Somewhat arbitrary.  Try to damp down block trades.
  if (_numSamples++ == 0) {
    _open = px;
    _high = px;
    _low = px;
  } else {
    if (px > _high) {
      _high = px;
    }
    if (px < _low) {
      _low = px;
    }
  }

  _close = px;

  _sumVol += vol;
  _sumVolPx += (vol * px);

  if (side == Mkt::BID) {
    _sumBidVol += vol;
  } else if (side == Mkt::ASK) {
    _sumAskVol += vol;
  }

  // Temporary - for debugging.
  //char buf[256];
  //snprint(buf, 256);
  //std::cout << "IntervalBar::addSample - new state = " << buf << std::endl;
}

int IntervalBar::snprint(char *s, int n) const {
  return snprintf(s, n, "IntervalBar  %i  %i  %f %f %f %f  %i  %f %f",
            _cid, _intervalNumber, _open, _close, _high, _low, _numSamples, _sumVol, _sumVolPx);
}

/******************************************************
  IntervalBarTracker code
******************************************************/
IntervalBarTracker::IntervalBarTracker() {

}

IntervalBarTracker::~IntervalBarTracker() {

}

/******************************************************
  TradeIntervalBarTracker code
******************************************************/
TradeIntervalBarTracker::TradeIntervalBarTracker(int intervalMSec) :
  IntervalBarTracker(),
  _dm(),
  _intervalNumber(0),
  _intervalMSec(intervalMSec),
  _startTV(0),
  _marketOpen(false),
  _currentBars(0),
  _mostRecentBars(0),
  _mrbStatus(0)
{
  _dm = factory<DataManager>::find(only::one);
  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in TradeIntervalBarTracker::TradeIntervalBarTracker)" ); 

  _currentBars.resize(_dm->cidsize()),
  _mostRecentBars.resize(_dm->cidsize()),
    _mrbStatus.assign(_dm->cidsize(), 0);

  // Note:  Hack to try to fix potential bug.
  // Original code caused TradeIntervalBarTracker to be added as a dm listener 
  //   *after* VolatilityTracker, and thus to receive updates after it as well.
  // This causes the TradeIntervalBarTracker to appear to be 1 interval
  //   behind the IBVolatilityTracker.  E.g. if we just processed many
  //   trade ticks for interval 8, when we get the IBVolTracker::populateCurrentPrices,
  //   querying _bart->barFinished will get bar data for interval *7*.
  // This effectively delays the introduction of new vol nfo by 1 sampling
  //   interval (probably 1000 milliseconds = 1 1 second).
  //_dm->add_listener(this);
  _dm->add_listener_front(this);
}

TradeIntervalBarTracker::~TradeIntervalBarTracker() {

}

/*
  TimerUpdate:
  - On market open:  set _startTime.
  - On timer: check if time to make new bar data set.
*/
void TradeIntervalBarTracker::update(const TimeUpdate &au) {
  if ( au.timer() == _dm->marketOpen() ) {
    onMarketOpen(au);
  } else if ( au.timer() == _dm->marketClose() ) {
    onMarketClose(au);
  } else { 
    onTimerUpdate(au);
  }
}

void TradeIntervalBarTracker::onMarketOpen(const TimeUpdate &au) {
    _marketOpen = true;
    _startTV = au.tv();
    _intervalNumber = 0;
    makeNewIntervalSet();
}

void TradeIntervalBarTracker::onMarketClose(const TimeUpdate &au) {
  _marketOpen = false;
}

void TradeIntervalBarTracker::onTimerUpdate(const TimeUpdate &au) {
  if (!_marketOpen) {
    return;
  }
  double msDiff = HFUtils::milliSecondsBetween(_startTV, au.tv());
  if (msDiff >= (_intervalMSec * _intervalNumber)) {
    makeNewIntervalSet();
  } 
}

/*
  DataUpdate:
  - Not a trade:  discard.
  - Trade:  Apply trade px & vol to _currentBars[cid].
*/
void TradeIntervalBarTracker::update(const DataUpdate &du) {
  if (!du.isTrade()) {
    return;
  }
  if (!_marketOpen) {
    return;
  }

  // Temporary, for debugging.
  //char buf[256];
  //du.snprint(buf, 128);
  //std::cout << "TradeIntervalBarTracker::update called - du = " << buf << std::endl;
  
  _currentBars[du.cid].addSample(du.side, du.price, du.size);
}

/*
  Start a new interval bar set (for all stocks in specified population set).
  - On the 1st such set:
    - Populate _currentBars with new IntervalBar objects.
    - Set _mrbStatus to all 0.
  - On subsequent sets:
    - Copy _currentBars --> _mostRecentBars.
    - Populate _currentBars with new IntervalBar objects.
    - Set _mrbStatus to all 1.
*/
void TradeIntervalBarTracker::makeNewIntervalSet() {
  int i;

  // Temporary - for debugging only:
  //std::cout << "TradeIntervalBarTracker::makeNewIntervalSet called" << std::endl;

  /*
    Things done on subsequent sets only:
    - Copy _currentBars --> _mostRecentBars.
    - Set _mrbStatus to all 1.    
  */
  if (_intervalNumber != 0) {
    _mostRecentBars = _currentBars;
    for (i=0;i<_dm->cidsize();i++) {
      _mrbStatus[i] = 1;
    }
  }

  /*
    Things done on all sets:
    - Populate _currentBars with new IntervalBar objects.
  */
  for (i=0;i<_dm->cidsize();i++) {
    IntervalBar b(i, _intervalNumber);
    _currentBars[i] = b;
  }

  /*
    Dont forget to increment _intervalNumber.
  */
  _intervalNumber++;
}

bool TradeIntervalBarTracker::barFinished(int cid, IntervalBar &fv) {
  if (_mrbStatus[cid] == 1) {
    fv = _mostRecentBars[cid];
    _mrbStatus[cid] = 2;
    return true;
  }
  return false;
}
