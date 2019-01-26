#include "c_util/Time.h"
using trc::compat::util::TimeVal;

#include "StocksState.h"

#include <cl-util/float_cmp.h>

const char *const SingleStockState::MktStatusDesc[] =  
  { "Normal ", "Locked ", "Crossed", "NoData ", "Unknown" };

const int LEVEL_0 = 0; /// Purely for readability
const int MAX_LEVEL_FOR_EXCLUSIVE_TOP = 5;  /// Not looking for exclusive top-level beyond the first inclusive N top levels

const int BUF_SIZE = 1024;
char buffer[BUF_SIZE];

/***********************************************************
  SingleStockState code
***********************************************************/

SingleStockState::SingleStockState( int cid ) 
  : _logPrinter( factory<debug_stream>::get(std::string("trader")) ),
    _cid( cid ),
    _bookTop(),
    _mktStatus(NODATA),
    _lastChangeInMktStatus(0),
    _exclusiveBookTop(),
    _exclusiveMktStatus(NODATA),
    _lastExclusiveMktUpdate(0),
    _lastWakeupBookTop(),
    _lastWakeupNormalTop(),
    _lastWakeupMktStatus(NODATA),
    _lastNormalBookTop(),
    _seenAnyNormalBookTop(false),
    _seenOldNormalBookTop(false),
    _minimalPxVariation(0.01),
    _priority(0.0),
    _marking(Mkt::UNKWN),
    _shsBought(0), _shsSold(0), _nBuys(0), _nSells(0), _lastFillTime(0),
    _totalFees(0.0), _totalFeesBuy(0.0), _totalFeesSell(0.0)
{
  _dm = factory<DataManager>::find(only::one);
  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in SingleStockState::SingleStockState)" );

  _initPos = _dm->position( _cid );
  _targetPos = _initPos;
}

void SingleStockState::onTradeRequest( const TradeRequest& tr ) {
  setTargetPosition( tr._targetPos );
  setPriority( tr._priority );
  setMarking ( tr._marking );
  setOrderID ( tr._orderID );
}
  
void SingleStockState::addFill( const OrderUpdate &ou ) {
  if( ou.dir() == Mkt::BUY ) {
    _nBuys++;
    _shsBought += ou.thisShares();
    _totalFeesBuy += ou.thisFee();
  } else {
    _nSells++;
    _shsSold += ou.thisShares();
    _totalFeesSell += ou.thisFee();
  }
  _totalFees += ou.thisFee();
  _lastFillTime = ou.tv();
}

void SingleStockState::onWakeup( bool haveSeenQuotesInBook ) {
  _lastWakeupBookTop = _bookTop;
  _lastWakeupNormalTop = _lastNormalBookTop;
  _lastWakeupMktStatus = _mktStatus;
  if (_seenAnyNormalBookTop)
    _seenOldNormalBookTop = true;
  if( haveSeenQuotesInBook )
    refreshBookTop();
}

void SingleStockState::refreshBookTop() {

  bool bidAskAvailable = getMarket( _dm->masterBook(), _cid, Mkt::BID, LEVEL_0, &_bookTop._bidPx, &_bookTop._bidSz ) &&
                         getMarket( _dm->masterBook(), _cid, Mkt::ASK, LEVEL_0, &_bookTop._askPx, &_bookTop._askSz );

  if( !bidAskAvailable ) { // no data
    updateMktStatus( NODATA );
  } else if( cmp<4>::LT(_bookTop._askPx,_bookTop._bidPx) ) { // spread is negative
    updateMktStatus( CROSSED );
  } else if( cmp<4>::EQ(_bookTop._askPx,_bookTop._bidPx) ) { // spread is zero
    updateMktStatus( LOCKED );
  }  else { // Market is "normal" (data is available and spread is positive)
    updateMktStatus( NORMAL );
  }

  if( haveNormalMarket() ) {
    _lastNormalBookTop = _bookTop;
    _seenAnyNormalBookTop = true;
  }
}

void SingleStockState::updateMktStatus( MktStatus currMktStatus ) {
  if( _mktStatus == currMktStatus ) return;

  // report if we currently trade this name
  if( _dm->position(_cid) != _targetPos || getMarketOrders( _dm->orderBook(), _cid ) > 0 )
    TAEL_PRINTF(_logPrinter.get(), TAEL_INFO, "%-5s Change of Mkt-Status: %s ==> %s   recorded cbbo=(%.2f,%.2f)",
			 _dm->symbol(_cid), MktStatusDesc[_mktStatus], MktStatusDesc[currMktStatus], 
			 bestPrice(Mkt::BID), bestPrice(Mkt::ASK) );

  _mktStatus = currMktStatus;
  _lastChangeInMktStatus = _dm->curtv();
  return;
}

double SingleStockState::getExclusiveBestPrice( Mkt::Side side ) {
  updateExclusiveMkt();
  return (side == Mkt::BID ? _exclusiveBookTop._bidPx : _exclusiveBookTop._askPx );
}

int SingleStockState::getExclusiveBestSize( Mkt::Side side ) {   
  updateExclusiveMkt();
  return (side == Mkt::BID ? _exclusiveBookTop._bidSz : _exclusiveBookTop._askSz );
}

double SingleStockState::getExclusiveSpread() {
  updateExclusiveMkt();
  return _exclusiveBookTop._askPx - _exclusiveBookTop._bidPx;
}

double SingleStockState::getExclusiveMid() {
  updateExclusiveMkt();
  return (_exclusiveBookTop._askPx + _exclusiveBookTop._bidPx)/2;
}

bool SingleStockState::getExclusiveBestPrice( Mkt::Side side, double* px ) {
  updateExclusiveMkt();
  if( _exclusiveMktStatus != NORMAL ) return false;
  *px = (side == Mkt::BID ? _exclusiveBookTop._bidPx : _exclusiveBookTop._askPx );
  return true;
}
  
bool SingleStockState::getExclusiveBestSize( Mkt::Side side, int* size ) {
  updateExclusiveMkt();
  if( _exclusiveMktStatus != NORMAL ) return false;
  *size = (side == Mkt::BID ? _exclusiveBookTop._bidSz : _exclusiveBookTop._askSz );
  return true;
}  

SingleStockState::MktStatus SingleStockState::getExclusiveMktStatus() { 
  updateExclusiveMkt();
  return _exclusiveMktStatus; 
}

bool SingleStockState::haveNormalExclusiveMarket() {
  updateExclusiveMkt();
  return _exclusiveMktStatus==NORMAL;
}

bool SingleStockState::haveNormalOrLockedExclusiveMarket() {
  updateExclusiveMkt();
  return (_exclusiveMktStatus==NORMAL || _exclusiveMktStatus==LOCKED);
}

bool SingleStockState::getExclusiveSpread( double* spread ) { 
  updateExclusiveMkt();
  if( _exclusiveMktStatus != NORMAL ) return false;
  *spread = _exclusiveBookTop._askPx - _exclusiveBookTop._bidPx; 
  return true;
}

bool SingleStockState::getExclusiveMid( double* mid ) { 
  updateExclusiveMkt();  
  if( _exclusiveMktStatus != NORMAL ) return false;
  *mid = (_exclusiveBookTop._askPx + _exclusiveBookTop._bidPx)/2; 
  return true;
}

bool SingleStockState::lastNormalMid( double *px ) const {
  if( !_seenAnyNormalBookTop ) return false;
  *px = (_lastNormalBookTop._bidPx + _lastNormalBookTop._askPx) / 2;
  return true;
}

bool SingleStockState::lastNormalSpread( double *spread ) const {
  if( !_seenAnyNormalBookTop ) return false;
  *spread = _lastNormalBookTop._askPx - _lastNormalBookTop._bidPx;
  return true;
}

bool SingleStockState::lastNormalPrice( Mkt::Side side, double *px ) const {
  if( !_seenAnyNormalBookTop ) return false;
  *px = ( side==Mkt::BID ? _lastNormalBookTop._bidPx : _lastNormalBookTop._askPx );
  return true;
}

bool SingleStockState::previousWakeupPrice(Mkt::Side side, double *px) const {
  if( !haveNormalOrLockedMarket() ) return false;
  *px = ( side==Mkt::BID ? _lastWakeupBookTop._bidPx : _lastWakeupBookTop._askPx );
  return true;
}

bool  SingleStockState::previousWakeupNormalPrice(Mkt::Side side, double *px) const {
  if ( !_seenOldNormalBookTop ) return false;
  *px = ( side==Mkt::BID ? _lastWakeupNormalTop._bidPx : _lastWakeupNormalTop._askPx );
  return true;
}
bool SingleStockState::atCBBO( Mkt::Side side, double px ) const {
  if( !haveNormalMarket() ) return false;
  return( cmp<4>::EQ(px,bestPrice(side)) );
}

bool SingleStockState::lessAggressiveThanCBBO( Mkt::Side side, double px ) const {
  if( !haveNormalMarket() ) return false;
  if( side==Mkt::BID ) return cmp<4>::LT( px, _bookTop._bidPx );
  else                 return cmp<4>::GT( px, _bookTop._askPx );
}

bool SingleStockState::strictlyInsideLastNormalCBBO( double px ) const {
  if( !_seenAnyNormalBookTop ) return false;
  return cmp<4>::LT(_lastNormalBookTop._bidPx, px) && cmp<4>::LT(px, _lastNormalBookTop._askPx);
}

// Update the "exclusive" top-market
void SingleStockState::updateExclusiveMkt() {
  if( _dm->curtv() == _lastExclusiveMktUpdate ) return; // THIS ASSUMES THAT _dm->curtv DOES NOT UPDATE WITHOUT NEW EXTERNAL UPDATES
  _lastExclusiveMktUpdate = _dm->curtv();

  // Bid
  int    level = 0;
  size_t inclusiveSz = _bookTop._bidSz;
  _exclusiveBookTop._bidPx = _bookTop._bidPx;
  while( (_exclusiveBookTop._bidSz = inclusiveSz - getMarketSize( _dm->orderBook(), _cid, Mkt::BID, _exclusiveBookTop._bidPx)) <= 0 ) {
    if( ++level > MAX_LEVEL_FOR_EXCLUSIVE_TOP ) break;
    if( !getMarket(_dm->masterBook(), _cid, Mkt::BID, level, &_exclusiveBookTop._bidPx, &inclusiveSz) ) break;
  }
  // Ask
  level = 0;
  inclusiveSz = _bookTop._askSz;
  _exclusiveBookTop._askPx = _bookTop._askPx;
  while( (_exclusiveBookTop._askSz = inclusiveSz - getMarketSize( _dm->orderBook(), _cid, Mkt::ASK, _exclusiveBookTop._askPx)) <= 0 ) {
    if( ++level > MAX_LEVEL_FOR_EXCLUSIVE_TOP ) break;
    if( !getMarket(_dm->masterBook(), _cid, Mkt::ASK, level, &_exclusiveBookTop._askPx, &inclusiveSz) ) break;
  }
  // Status
  if( _exclusiveBookTop._bidSz <= 0 || _exclusiveBookTop._askSz <=0 )      _exclusiveMktStatus = NODATA;
  else if( cmp<4>::LT(_exclusiveBookTop._askPx,_exclusiveBookTop._bidPx) ) _exclusiveMktStatus = CROSSED;
  else if( cmp<4>::EQ(_exclusiveBookTop._askPx,_exclusiveBookTop._bidPx) ) _exclusiveMktStatus = LOCKED;
  else                                                                     _exclusiveMktStatus = NORMAL;
}


StocksState::StocksState() :
  _states(0),
  _nDuSinceLastWakeup(0)
{
  
  _dm = factory<DataManager>::find(only::one);
  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in StocksState::StocksState)" );

  // Initialize the vector of SingleStockStates
  int nStocks = _dm->cidsize();
  _states.resize( nStocks );
  for( int cid=0; cid < nStocks; cid++ )
    _states[cid] = new SingleStockState( cid );

  _nDuSinceLastWakeup.resize( nStocks, 0 );

  _dm->add_listener_front( this );
}

StocksState::~StocksState() {
  for( int cid=0; cid<_dm->cidsize(); cid++ )
    delete _states[cid];
  //  _dm->remove_listener( this ); ==> caused problems on end of day
}

void StocksState::update( const OrderUpdate& ou ) { 
  if( ou.action() == Mkt::FILLED )
    _states[ ou.cid() ]->addFill( ou );
}

void StocksState::update( const WakeUpdate& wu ) { 
  // refresh all stock-states
  for( int cid=0; cid<_dm->cidsize(); cid++ ) 
    _states[cid]->onWakeup( _nDuSinceLastWakeup[cid]>0 );

  _nDuSinceLastWakeup.assign( _dm->cidsize(), 0 );
}

