#include "QSzTracker.h"
#include "HFUtils.h"
#include <vector>

using namespace std;

const int LEVEL_0 = 0;
const int MIN_SIZE_TO_CONSTITUTE_A_LEVEL = 100;

QSzTracker::QSzTracker() 
  :  _dm( factory<DataManager>::get(only::one) ),
     _msec(1000),
     _lastUpdateTV(TimeVal(0)),
     _mktOpen(false),
     _warmed(false),
     _ct(0)
{
  _qSz.resize( _dm->cidsize(), 0.0 );
  for( int i=0; i<ECN::ECN_size; i++ ) {
    _ecnQSz[i].resize( _dm->cidsize(), 0.0 );
    // initialize _turnedOnEcns
    ECN::ECN ecn = (ECN::ECN) i;
    if( _dm->isDataOn(ecn) )
      _turnedOnEcns.push_back( ecn );
  }

  _dm->add_listener(this); // This should be add_listener_back eventually
  _lambda = 2.0/(1000.0*10*60.0/_msec + 1);
  //printf("QSZ Tracker: Init n=%d msec=%d length=%d lambda=%f\n",dm->cidsize(),msec,length,_lambda);
}


QSzTracker::QSzTracker( int msec, int length ) 
  :  _msec(msec),
     _lastUpdateTV(TimeVal(0)),
     _mktOpen(false),
     _warmed(false),
     _ct(0)
{
  _dm = factory<DataManager>::find(only::one);
  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in QSzTracker::QSzTracker)" );

  _qSz.resize( _dm->cidsize(), 0.0 );
  _dm->add_listener(this);
  //  _dm->add_listener_back(this);
  _lambda = 2.0/(1000.0*length*60.0/msec + 1);
  //printf("QSZ Tracker: Init n=%d msec=%d length=%d lambda=%f\n",dm->cidsize(),msec,length,_lambda);
}

void QSzTracker::update( const TimeUpdate &t ) {
  if (t.timer() == _dm->marketOpen())  _mktOpen = true;
  if (t.timer() == _dm->marketClose()) _mktOpen = false;
}


void QSzTracker::update( const WakeUpdate& wu ) {
  
  double msb = HFUtils::milliSecondsBetween( _lastUpdateTV, wu.tv );
  if( msb >= _msec && _mktOpen ) {
    sampleAllStocks();
    _lastUpdateTV = wu.tv;
  }
}

void QSzTracker::sampleAllStocks(){
  double bpx,apx;
  size_t bsz,asz;

  if( _warmed ) {
    for( int cid=0; cid<_dm->cidsize(); cid++ ) {
      if( getTradableMarket(_dm->masterBook(), cid, Mkt::ASK, LEVEL_0, MIN_SIZE_TO_CONSTITUTE_A_LEVEL, &apx, &asz) && 
	  getTradableMarket(_dm->masterBook(), cid, Mkt::BID, LEVEL_0, MIN_SIZE_TO_CONSTITUTE_A_LEVEL, &bpx, &bsz) ) {
	_qSz[cid] = _qSz[cid] * (1.0 - _lambda) + (_lambda) * ( bsz + asz )/2.0;
	for(unsigned int i=0; i<_turnedOnEcns.size(); i++ ) {
	  ECN::ECN ecn = (ECN::ECN) _turnedOnEcns[i];
	  asz = getMarketSize( _dm->subBook(ecn), cid, Mkt::ASK, apx );
	  bsz = getMarketSize( _dm->subBook(ecn), cid, Mkt::BID, bpx );
	  _ecnQSz[ecn][cid] = _ecnQSz[ecn][cid] * (1.0 - _lambda) + (_lambda) * ( bsz + asz )/2.0;
	}
      }
    }
  }

  // If NOT "warmed"
  else { 
    for( int cid=0; cid<_dm->cidsize(); cid++ ) {
      if( getTradableMarket(_dm->masterBook(), cid, Mkt::ASK, LEVEL_0, MIN_SIZE_TO_CONSTITUTE_A_LEVEL, &apx, &asz) && 
	  getTradableMarket(_dm->masterBook(), cid, Mkt::BID, LEVEL_0, MIN_SIZE_TO_CONSTITUTE_A_LEVEL, &bpx, &bsz) ) {
	_qSz[cid] = _qSz[cid] + ( bsz + asz )/2.0; // keep the sum of sampled q-sizes until "warmed"
	for(unsigned int i=0; i<_turnedOnEcns.size(); i++ ) {
	  ECN::ECN ecn = (ECN::ECN) _turnedOnEcns[i];
	  asz = getMarketSize( _dm->subBook(ecn), cid, Mkt::ASK, apx );
	  bsz = getMarketSize( _dm->subBook(ecn), cid, Mkt::BID, bpx );
	  _ecnQSz[ecn][cid] = _ecnQSz[ecn][cid] + ( bsz + asz )/2.0;
	}
      }
    }
    // We are warmed after 30 samples
    _ct++;
    if( _ct>30 ){
      for( int cid=0; cid<_dm->cidsize(); cid++ ) {
	_qSz[cid] /= _ct;
	for(unsigned int i=0; i<_turnedOnEcns.size(); i++ ) {
	  ECN::ECN ecn = (ECN::ECN) _turnedOnEcns[i];
	  _ecnQSz[ecn][cid] /= _ct;
	}	
      }
      _warmed = true;
    }
  }
}

bool QSzTracker::qSz( int cid, double &qSz ) const {
  qSz = _warmed ? _qSz[cid] : -1;
  return( _warmed );
}

bool QSzTracker::ecnQSz( int cid, ECN::ECN ecn, double &qSz ) const {
  qSz = _warmed ? _ecnQSz[ecn][cid] : -1;
  return( _warmed );
}

