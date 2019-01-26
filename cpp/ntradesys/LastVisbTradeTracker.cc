#include <cl-util/factory.h>
using namespace clite::util;
#include "DataManager.h"

#include "LastVisbTradeTracker.h"

LastVisbTradeTracker::LastVisbTradeTracker() 
  : _filterFunc(defaultLVTFilter),
    _bidVS(0),
    _askVS(0)
{
  factory<DataManager>::pointer dm = factory<DataManager>::find(only::one);
  if( !dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in LastVisbTradeTracker::LastVisbTradeTracker)" );
  int cidsize = dm->cidsize();
  _bidVS.resize( ECN::ECN_size, DUVector(cidsize) );
  _askVS.resize( ECN::ECN_size, DUVector(cidsize) );

  for (int i=0;i<ECN::ECN_size;i++) {
    initialize(_bidVS[i]);
    initialize(_askVS[i]);
  }

  dm->add_listener( this );
}


LastVisbTradeTracker::LastVisbTradeTracker(int cidsize, LVT_FF filterFunc) : 
  _filterFunc(filterFunc),
  _bidVS(ECN::ECN_size, DUVector(cidsize)), 
  _askVS(ECN::ECN_size, DUVector(cidsize))
{
  for (int i=0;i<ECN::ECN_size;i++) {
    initialize(_bidVS[i]);
    initialize(_askVS[i]);
  }
}

/*
   Called after constructor.
   Set elements of _isldBidV and _isldAskV to default state indicating
     that they are dummy place-holder values....
*/
void LastVisbTradeTracker::initialize(DUVector &duv) {
  vector<DataUpdate>::iterator it;
  for (it = duv.begin(); it != duv.end(); ++it) {
    it->type = Mkt::BOOK;
    it->ecn = ECN::UNKN;
    it->side = Mkt::BID;
    it->cid = -1;
    it->size = 0;
  }
}

void LastVisbTradeTracker::update( const DataUpdate& du ) {
  if( include(du) ) {
    DUVector *vec = getVector( du.ecn, du.side );
    (*vec)[du.cid] = du;
  }
}

DataUpdate *LastVisbTradeTracker::lastTrade(int cid, ECN::ECN ecn, Mkt::Side side) {
  DataUpdate *ret;
  assert(side == Mkt::BID || side == Mkt::ASK);
  DUVector *vec = getVector(ecn, side);
  ret = &((*vec)[cid]); 
  if (isDummyTrade(*ret)) {
    return NULL;
  }
  return ret;
}

bool LastVisbTradeTracker::isDummyTrade(const DataUpdate &du) {
  if (du.cid == -1) return true;
  return false;
}

bool LastVisbTradeTracker::include(const DataUpdate &du) {
  if (_filterFunc == NULL) return true;
  bool ret = (*_filterFunc)(du);
  return ret;  
}

bool defaultLVTFilter(const DataUpdate &du) {
  if (!du.isTrade()) {
    return false;
  }
  if (du.ecn != ECN::ISLD) {
    return false;
  }
  if (!du.isVisibleTrade()) {
    return false;
  }
  // Seem to sometimes be inserted into data update stream in sim
  //   trading.  Strange....
  if (du.size == 0 && du.id == 0) {
    return false;
  }
  return true;
}

DUVector *LastVisbTradeTracker::getVector(ECN::ECN ecn, Mkt::Side side) {
  assert(side == Mkt::BID || side == Mkt::ASK);
  if (side == Mkt::BID) {
    return &(_bidVS[ecn]);
  }
  return &(_askVS[ecn]);
}
