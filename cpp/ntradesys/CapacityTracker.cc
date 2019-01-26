#include "CapacityTracker.h"

#include "DataManager.h"
#include "DataUpdates.h"
#include <vector>
#include <cl-util/factory.h>
#include <vector>

#define INIT_CAPACITY 74 // Initial capacity to use

using namespace std;
using namespace clite::util;

int CapacityTracker::printCount = 0;
const int CapacityTracker::_periods[] = {1,5,30,60};
const double CapacityTracker::_lambda[] = {0.017,0.017,0.1,0.1}; // Values precomupted to keep 2m history for 1sec, 
                                                                 // 5m for 5s, 10m for 30s, 5m for 1m averages
const double CapacityTracker::_fraction[] = {0.05,0.05,0.1,0.1}; // Percentage of volume we should be doing per period

const string RELEVANT_LOG_FILE = "misc";

CapacityTracker::CapacityTracker():
  _dm(factory<DataManager>::find(only::one)),
  _logPrinter( factory<debug_stream>::get(RELEVANT_LOG_FILE) ),
  _hvt (factory<HistVolumeTracker>::get(only::one)),
  _accVol(_dm->cidsize(),0),
  _ourAccVol(_dm->cidsize(),0),
  _mktOpen(false),
  _minCap(100),
  _lastTick(0),
  _participationRate(INIT_CAPACITY)
{  
  _lastV.resize( MAXPERIODS );
  _estimate.resize( MAXPERIODS );
  _lastOurv.resize( MAXPERIODS );
  _avgOurV.resize( MAXPERIODS );
  _laststate.resize( _dm->cidsize(), 0 );
  for (int i=0;i<MAXPERIODS;i++){
    _lastV[i].resize( _dm->cidsize(), 0 );
    _estimate[i].resize( _dm->cidsize(), 0.0 );
    _lastOurv[i].resize( _dm->cidsize(), 0 );
    _avgOurV[i].resize( _dm->cidsize(), 0.0 );
  }
  
  _dm->add_listener(this);

  TimeVal t = _dm->curtv();
  for (int cid=0;cid<_dm->cidsize();cid++){
    double v = _hvt->getAvgDailyVol(cid)/(60*60*6.5); // the avergae volume per second during trading hours (=cross section), historically
    for (int i=0;i<MAXPERIODS;i++){
      _estimate[i][cid] = v*_periods[i];
      TAEL_TPRINTF(_logPrinter.get(), &t.tv, TAEL_ERROR, "INIT %s estimate[%d]=%f",_dm->symbol(cid),_periods[i],_estimate[i][cid]);
    }
  }
}

void CapacityTracker::update(const TimeUpdate &t) {
  if (t.timer() == _dm->marketOpen())  _mktOpen = true;
  if (t.timer() == _dm->marketClose()) _mktOpen = false;
}

void CapacityTracker::update ( const DataUpdate &du ) {
  if (du.isTrade()) 
    _accVol[du.cid] += du.size;
}

void CapacityTracker::update( const OrderUpdate& ou ) { 
   if (ou.action() != Mkt::FILLED)
     return;
   _ourAccVol[ou.cid()] += ou.thisShares();
}

void CapacityTracker::update(const WakeUpdate &wu) {
  
  if (!_mktOpen)
    return;
  int t = _dm->curtv().sec();
  if (t==_lastTick)
    return;
  TimeVal tmv = _dm->curtv();

  for (int i=0;i<MAXPERIODS;i++){
    if (t%_periods[i]==0){
      for (int cid=0;cid<_dm->cidsize();cid++){
	_estimate[i][cid] = _estimate[i][cid]*(1-_lambda[i]) + (_accVol[cid]-_lastV[i][cid])*_lambda[i];
	_avgOurV[i][cid] = _avgOurV[i][cid]*(1-_lambda[i]) + (_ourAccVol[cid]-_lastOurv[i][cid])*_lambda[i];
	_lastV[i][cid] = _accVol[cid];
	_lastOurv[i][cid]=_ourAccVol[cid];
	//TAEL_PRINTF(_logPrinter, TAEL_ERROR, &tmv,"%d %s estimate[%d]=%f",t,_dm->symbol(cid),_periods[i],_estimate[i][cid]);
      }
    }
  }
  _lastTick=t;
}

bool CapacityTracker::capacity(int cid){
  double cap = 1e6;
  int limperiod=-1;
  double limrate = 0;
  int limvol = -1;
  bool ret = true;
  // HACK/Feature to disable rate limiting
  if  (_participationRate>75)
    return true;

  if ( (_participationRate<75) && (_avgOurV[MAXPERIODS-1][cid]>_participationRate*_estimate[MAXPERIODS-1][cid]/100.0)){
    // We've exceeded our participation rate ! Stop
    TAEL_PRINTF(_logPrinter.get(), TAEL_INFO, "%5s: CapTracker: Disallowing as OurVol=%f "
    		"avgVol=%f participation rate=%f",_dm->symbol(cid),_avgOurV[MAXPERIODS-1][cid],
    		_estimate[MAXPERIODS-1][cid]/100.0,_participationRate);
    ret = false;
  }
  else{
	if (printCount < 10)  {
		TAEL_PRINTF(_logPrinter.get(), TAEL_INFO, "Came in CapacityTracker. Checking capacity constraints now.");
		printCount++;
	}
    for (int i=0;i<MAXPERIODS;i++){
      double rate = std::max(_fraction[i]*_estimate[i][cid]+100,_periods[i]*100.0); // HEURISTIC: Assume a min rate of 100/sec (?)
      double periodcap = std::max(rate-(_ourAccVol[cid]-_lastOurv[i][cid]),0.0);
      if (periodcap<_minCap){
	ret = false;
	cap = periodcap;
	limperiod = _periods[i];
	limrate = _fraction[i]*_estimate[i][cid] ;
	limvol = _ourAccVol[cid]-_lastOurv[i][cid];
      }
    }
  }
  if (!ret){
    if (_laststate[cid]>0){
      TAEL_PRINTF(_logPrinter.get(), TAEL_ERROR, "%5s: CapTracker: Disallowing as OurVol=%f avgVol=%f cap=%f rate[%d]=%f < %d = vol[%d]",_dm->symbol(cid),_avgOurV[MAXPERIODS-1][cid],_estimate[MAXPERIODS-1][cid]/100.0,cap,limperiod,limrate,limvol,limperiod);
    }
  }
  else
    if (_laststate[cid]==0)
      TAEL_PRINTF(_logPrinter.get(), TAEL_ERROR, "%5s: CapTracker: Allowing as OurVol=%f avgVol=%f cap=%f rate[%d]=%f < %d = vol[%d]",_dm->symbol(cid),_avgOurV[MAXPERIODS-1][cid],_estimate[MAXPERIODS-1][cid]/100.0,cap,limperiod,limrate,limvol,limperiod);
  
  _laststate[cid]=ret?1:0;
  return ret;
}
