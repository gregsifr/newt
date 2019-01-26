#include "OpenTracker.h"

using namespace clite::util;
using trc::compat::util::DateTime;

OpenTracker::OpenTracker()
  : _exchangeT( factory<ExchangeTracker>::get(only::one) ),
    _nyse(0),
    _amex(0),
    _marketIsOpen(false),
    _nyseOpenTV(0),
    _amexOpenTV(0),
    _marketOpenTV(0)
{
  _dm = factory<DataManager>::find(only::one);
  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in MaxQueueSizeTracker::MaxQueueSizeTracker)" );
  _nyse.assign( _dm->cidsize(), false );  
  _amex.assign( _dm->cidsize(), false );

  _nyseOpenTV.assign(_dm->cidsize(), 0 );
  _amexOpenTV.assign(_dm->cidsize(), 0 );

  _dm -> add_listener( this );
}

void OpenTracker::update( const DataUpdate& du ) {
  if( du.isTrade() ) {
    if (du.ecn == ECN::NYSE && !_nyse[du.cid]) {
      _nyse[du.cid] = true;
      _nyseOpenTV[du.cid] = du.tv;
    }
    if (du.ecn == ECN::AMEX && !_amex[du.cid]) {
      _amex[du.cid] = true;
      _amexOpenTV[du.cid] = du.tv;
    }
  }
}

void OpenTracker::update( const TimeUpdate& t ) {
    if (t.timer() == _dm->marketOpen()) {
    _marketIsOpen = true;
    _marketOpenTV = t.tv();
    _nyse.clear(); _nyse.resize(_dm->cidsize(), false);
    _amex.clear(); _amex.resize(_dm->cidsize(), false);
    } else if (t.timer() == _dm->marketClose()) {
        _marketIsOpen = false;
    }
}

bool OpenTracker::hasOpened( int cid, ECN::ECN ecn ) const {
  DateTime dt( _dm->curtv() );
  if( dt.hh()*60 + dt.mm() >= 575 ) // ASSUME: Market has already opened for all stocks after 9:35am
    return _marketIsOpen; 
  switch (ecn) {
    case ECN::AMEX:
      if( _dm->subBook(ecn) == 0 ) return _marketIsOpen;
      else return _amex[cid] && _marketIsOpen ;
      break; 
    case ECN::NYSE:
      if ( _dm->subBook(ecn) == 0 ) return _marketIsOpen;
      else return _nyse[cid] && _marketIsOpen ;
      break;
    default: 
      return _marketIsOpen;
  }
}

bool OpenTracker::openTV(int cid, ECN::ECN ecn, TimeVal &fv) const {
  bool ret = hasOpened(cid, ecn);
  if (!ret) {
    return false;
  }
  fv = _marketOpenTV;
  switch (ecn) {
    case ECN::AMEX:
      if( _dm->subBook(ecn) != 0 ) {
	fv = _amexOpenTV[cid];
      }
      break; 
    case ECN::NYSE:
      if ( _dm->subBook(ecn) != 0 ) {
	fv = _nyseOpenTV[cid];
      }
      break;
    default: 
      break;
  }
  return true;
}
