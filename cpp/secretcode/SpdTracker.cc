/*
  SpdTracker.cc
  Contains logic for various types of spread calculation.  Used for stock
    market-making.
  Spread calc methods here replicate some modeo factors fitted in LRCF model
    generating code - and probably need to model coefficients from that code
    to be applied as intended.  Be careful changing spod calculation here unless
    also changing factor definitions (and recalculating betas) in LRCF model
    generation code.
*/

#include "SpdTracker.h"

#include <cl-util/float_cmp.h>

#include "HFUtils.h"

#include <iostream>
#include <ostream>
using namespace std;

using trc::compat::util::DateTime;
const string RELEVANT_LOG_FILE = "misc";

SpdTracker::SpdTracker() :
  _minpts(50),
  _nperiods(60),
  _msec(1000),
  _buf(),
  _lastUpdateTV(),
  _lastPrintTV(),
  _mktOpen(false)
{
  _dm = factory<DataManager>::find(only::one);
  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in SpdTracker::SpdTracker)" );   

  _ddebug = factory<debug_stream>::get( RELEVANT_LOG_FILE );
  if (!_ddebug) {
    throw std::runtime_error( "Failed to get debug_stream (in SpdTracker::SpdTracker)" );   
  }

  TAEL_PRINTF(_ddebug.get(), TAEL_INFO, "SpdTracker::SpdTracker called: dm->cidsize = %i  minpts = %i  nperiods = %i  msec = %i",
		 _dm->cidsize(), _minpts, _nperiods, _msec); 
  //std::cout << "SpdTracker::SpdTracker : dm->cidsize = " << dm->cidsize() <<
  //  " minpts = " << minpts << " nperiods " << nperiods << " msec " << msec << std::endl;
  _buf.resize(_dm->cidsize());
  for (int i=0;i<_dm->cidsize();i++) {
    _buf[i] = new CircBuffer<double>(_nperiods);
  }
  _dm->add_listener(this);
}

SpdTracker::SpdTracker(int minpts, int nperiods, int msec) :
  _minpts(minpts),
  _nperiods(nperiods),
  _msec(msec),
  _buf(),
  _lastUpdateTV(),
  _lastPrintTV(),
  _mktOpen(false)
{
  _dm = factory<DataManager>::find(only::one);
  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in SpdTracker::SpdTracker)" );   

  _ddebug = factory<debug_stream>::get( RELEVANT_LOG_FILE );
  if (!_ddebug) {
    throw std::runtime_error( "Failed to get debug_stream (in SpdTracker::SpdTracker)" );   
  }

  TAEL_PRINTF(_ddebug.get(), TAEL_INFO, "SpdTracker::SpdTracker called: dm->cidsize = %i  minpts = %i  nperiods = %i  msec = %i",
		 _dm->cidsize(), _minpts, _nperiods, _msec); 
  //std::cout << "SpdTracker::SpdTracker : dm->cidsize = " << dm->cidsize() <<
  //  " minpts = " << minpts << " nperiods " << nperiods << " msec " << msec << std::endl;
  _buf.resize(_dm->cidsize());
  for (int i=0;i<_dm->cidsize();i++) {
    _buf[i] = new CircBuffer<double>(_nperiods);
  }
  _dm->add_listener(this);
}

SpdTracker::~SpdTracker() {
  for (int i=0;i<_dm->cidsize();i++) {
    delete _buf[i];
  }
}

int SpdTracker::nPts(int cid) {
  CircBuffer<double> *cb = _buf[cid];
  return cb->size();
}

bool SpdTracker::getASpd(int cid, double &fv) {
  if (nPts(cid) < _minpts) {
    fv = 0.0;
    return false;
  }
  return _buf[cid]->getAvg(&fv);
}

bool SpdTracker::getDSpd(int cid, double &fv) {
  double aspd;
  if (!getASpd(cid, aspd)) {
    fv = 0.0;
    return false;
  }
  double cspd;
  if (!getSpd(cid, cspd)) {
    fv = 0.0;
    return false;
  }
  fv = (cspd - aspd);
  return true;
}

/*
  Query data cache for point estimate of spread for specified stock.
  Note:  Should return spd in bps vs mid, not in notional $.
*/
bool SpdTracker::getSpd(int cid, double &fvSpd) {
  double bid, ask, mid, spd;
  if (!getMkt(cid, bid, ask)) {
    fvSpd = 0.0;
    return false;
  }
  mid = (bid + ask)/2.0;
  spd = (ask - bid);
  fvSpd = spd/mid;
  return true;
}

bool SpdTracker::getMkt(int cid, double &bid, double &ask) {
  if (!HFUtils::bestPrice(_dm.get(), cid, Mkt::BID, bid) ||
      !HFUtils::bestPrice(_dm.get(), cid, Mkt::ASK, ask)) {
    bid = 0.0;
    ask = 0.0;
    return false;
  }  
  return true;
}

/*
  Add a point estimate of spread for specified stock.
*/
void SpdTracker::addSampledSpd(int cid, double fv) {
  //std::cout << "SpdTracker::addSampledSpd : cid = " << cid << " fv =  " << fv << std::endl;
  CircBuffer<double> *cb = _buf[cid];
  cb->add(fv);
}

void SpdTracker::sampleAllStocks() {
  int i, nstocks = _buf.size();
  double spd;
  for (i=0;i<nstocks;i++) {
    if (!getSpd(i, spd)) {
      continue;
    }
    addSampledSpd(i, spd);
  }
}

void SpdTracker::printAllStocks() {
  int i, nstocks = _buf.size();
  double bid, ask;
  DateTime dt(_dm->curtv());
  TAEL_PRINTF(_ddebug.get(), TAEL_INFO, "SpdTracker::printAllStocks called - curTV = (HH)%i, (MM)%i, (SS)%i, (US)%i",
		 dt.hh(), dt.mm(), dt.ss(), dt.usec()); 
  for (i=0;i<nstocks;i++) {
    if (!getMkt(i, bid, ask)) {
      continue;
    }
    TAEL_PRINTF(_ddebug.get(), TAEL_INFO, "%-5s  stock %i : bid = %f, ask = %f", _dm->symbol(i), i, bid, ask);
  }
}

void SpdTracker::clearAllStocks() {
  int i, nstocks = _buf.size();
  for (i=0;i<nstocks;i++) {
    _buf[i]->clear();
  }
}

void SpdTracker::update(const TimeUpdate &au) {
  if ( clite::util::cmp<4>::EQ(_msec, 0.0) ) {
    sampleAllStocks();
  }
  processTimeUpdate(au);   
}

/*
  Process incoming admin update.  Should only be called if _msec > 0.
*/
void SpdTracker::processTimeUpdate(const TimeUpdate &au) {

  double msb = HFUtils::milliSecondsBetween(_lastUpdateTV, au.tv());
  if ((msb >= _msec) && (_mktOpen == true)) {
    sampleAllStocks();
    _lastUpdateTV = au.tv();
  }
  // Temporary for debugging.
  msb = HFUtils::milliSecondsBetween(_lastPrintTV, au.tv());
  if ((msb >= 60000) && (_mktOpen == true)) {
    printAllStocks();
    _lastPrintTV = au.tv();
  }
  
  if (au.timer() == _dm->marketOpen()) {
    _mktOpen = true;
    clearAllStocks();
  }
  if (au.timer() == _dm->marketClose()) {
    _mktOpen = false;
  }
}

