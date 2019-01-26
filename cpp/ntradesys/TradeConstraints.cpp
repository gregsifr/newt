#include "TradeConstraints.h"

using trc::compat::util::DateTime;

const TimeVal HALT_TIME_FOLLOWING_UNSOLICITED_CANCEL = TimeVal( 10, 0 ); // 10 seconds
const int  MIN_CACACPITY_TO_TRADE=0;
const int IOC_TIMEOUT = 0;
const static int MAX_PER_TICKER_ORDER_RATE = 75;    // 75 orders per second (default)

PerSymbolConstraints::PerSymbolConstraints( int cid )
  : _cid( cid ),
    _dm( factory<DataManager>::get(only::one) ),
    _logPrinter( factory<debug_stream>::get(std::string("trader")) ),
    _unsolicitedCxlsLog( factory<debug_stream>::get(std::string("unsolicited-cancels")) ),
    _centralOrderRepo( factory<CentralOrderRepo>::get(only::one) ),
    _canBeTradedForSure( false ),
    _timeToResumeTrading( TimeVal(0) ),
    _openTracker( factory<OpenTracker>::get(only::one) ),
    _capTracker( factory<CapacityTracker>::get(only::one)),
    _ecnsAllowed(),
    _opWindow(),
    _orderRate(MAX_PER_TICKER_ORDER_RATE)
{}

/*
  Check whether placing an order in _cid would violate per-stock 
    order placement rate limits.
  Note:
  - These limits are currently implemented on the execution engine
    side as per-stock, not per-stock x ecn.  Infra supposedly implements
    these as per stoxk x virtual-account, but does not reliably communicate
    ecn x session x virtual account mappings to execution, so we choose
    to implenent this per stock to be conservative.
*/
bool PerSymbolConstraints::checkOPRateLimit() {
  /*  current wall/sim time */
  TimeVal curtv = _dm->curtv();

  /*  wall/sim time as of 1 second ago.  */
  TimeVal diff(1, 0);
  curtv -= diff;
 
  /*  remove time-stamps of any order placed > 1 second ago.  */
  _opWindow.advance(curtv);

  /*  see how many orders have been placed in last second.  */
  int sz = _opWindow.size();
  if (sz >= _orderRate) {
    // too many orders - would violate rate limit.
    TAEL_PRINTF(_logPrinter.get(), TAEL_WARN, "%-5s WARNING: disallowing order placement that would exceed per-stock order rate limit.",
			 _dm->symbol(_cid) );
    return false;
  } 

  // not going to violate rate limit.
  return true;
}

void PerSymbolConstraints::onPlace() {
  /*  Place current wall/sim time into sliding window.  */
  TimeVal curtv = _dm->curtv();
  _opWindow.push(curtv);
}

bool PerSymbolConstraints::canPlace() {
  // check % of volume constraints.
  if (!_capTracker->capacity(_cid)) return false;
  // check per-ticker risk-mgmt imposed order placement rate limit constraints.
  if (!checkOPRateLimit()) return false;
  
  // check whether stock is trading in normal market session, and
  //   also whether execution system user has temporarily ask for a
  //   halt in trading it.
  if( _canBeTradedForSure ) return true;
  
  // _canBeTradedForSure is false ==> check if market is open and if there are recent rejections
  if( !_openTracker->hasOpenedOnPrimaryExchange(_cid) ) return false; // market is not open for this symbol
  if( _timeToResumeTrading > _dm->curtv() ) return false;             // it's not time to resume yet
  if (_dm->stops[_cid]) return false; // trading is halted right now for this symbol
  
  // ok, we can start/resume trading
  _canBeTradedForSure = true;
  return true;
}

bool PerSymbolConstraints::canPlace( ECN::ECN ecn, double price ) {
  if( !canPlace(ecn) ) return false;

  if( _specificEcnPxConstraints[ecn].empty() ) return true; // I added this line mostly for efficiency: this is the common scenario by far
  
  price_to_time::iterator it = _specificEcnPxConstraints[ecn].find( price );
  // GVNOTE: Not sure if the following is the right behavior (for both buy/sell). Check on it.
  if( it == _specificEcnPxConstraints[ecn].end() ) 
    return true;
  
  // Are we passed the time-to-resume?
  if( it->second <= _dm->curtv() ) {
    _specificEcnPxConstraints[ecn].erase( it );
    return true;
  }
  return false;
}

/// assuming this is a rejection for this cid. 
/// setting "time-to-resume-trading" according to the type of rejection
void PerSymbolConstraints::onReject( const OrderUpdate& ou ) {
  TimeVal haltTime = getHaltTimeFollowingRejection( ou.error() );
  _timeToResumeTrading = _dm->curtv() + haltTime;
  _canBeTradedForSure = false;
  TAEL_PRINTF(_logPrinter.get(), TAEL_INFO, "%-5s Due to rejection, halting trading until %s",
		       _dm->symbol(_cid), DateTime(_timeToResumeTrading).gettimestring() );
}

void PerSymbolConstraints::onCxlRej( const OrderUpdate& ou ) {
  orders_id_set::const_iterator it = _cancelingOrdersSet.find( ou.id() );
  if( it != _cancelingOrdersSet.end() ) _cancelingOrdersSet.erase( it );
  else TAEL_PRINTF(_logPrinter.get(), TAEL_WARN, "%-5s WARNING: got a cxl-reject for order ID %d but this order is not in the "
			    "canceling-set", _dm->symbol(ou.cid()), ou.id() );
}

void PerSymbolConstraints::onFullCxled( const OrderUpdate& ou ) {
  if (ou.timeout() == IOC_TIMEOUT )
    return;
  orders_id_set::const_iterator it = _cancelingOrdersSet.find( ou.id() );
  if( it != _cancelingOrdersSet.end() ) _cancelingOrdersSet.erase( it );
  // if order is not canceling: This is an unsolicited cancel
  else onUnsolicitedCxl( ou );
}

void PerSymbolConstraints::onUnsolicitedCxl( const OrderUpdate& ou ) {
  int bidSzAtLevel = getMarketSize(_dm->subBook(ou.ecn()), ou.cid(), Mkt::BID, ou.price());
  int askSzAtLevel = getMarketSize(_dm->subBook(ou.ecn()), ou.cid(), Mkt::ASK, ou.price());
  // Don't halt trading if there is a plausible reason for the cancel:
  // - we crossed and now the other side is empty 
  // - We joined the queue and now that queue is empty.
  const OrderRecord* ordRec = _centralOrderRepo->getOrderRecord( ou.cid(), ou.id() );
  if (!ordRec)
    return;
  if( ordRec->placementReason() == OrderPlacementSuggestion::CROSS ) {
    int otherSideSize = ou.side() == Mkt::BID ? askSzAtLevel : bidSzAtLevel;
    if( otherSideSize == 0 ) 
      return;
  }
  if( ordRec->placementReason() == OrderPlacementSuggestion::JOIN_QUEUE ) {
    int thisSideSize = ou.side() == Mkt::BID ? bidSzAtLevel : askSzAtLevel;
    if( thisSideSize == 0 ) 
      return;
  }

  TimeVal resumeTime = _dm->curtv() + HALT_TIME_FOLLOWING_UNSOLICITED_CANCEL;
  _specificEcnPxConstraints[ ou.ecn()][ ou.price() ] = resumeTime;
  TAEL_PRINTF(_logPrinter.get(), TAEL_INFO, "%-5s Due to unsolicited cancel, halting trading at price %.2f on %s until %s",
		       _dm->symbol(_cid), ou.price(), ECN::desc(ou.ecn()), DateTime(resumeTime).gettimestring() );

  TAEL_PRINTF(_unsolicitedCxlsLog.get(), TAEL_WARN, "%-5s Unsolicited cxl on %s (%s,%d@%.2f). "
			       "The sizes (bid,ask) we see in this level on that ECN: (%d,%d)",
			       _dm->symbol(ou.cid()), ECN::desc(ou.ecn()), Mkt::TradeDesc[ou.dir()], ou.size(), ou.price(), 
			       bidSzAtLevel, askSzAtLevel );
  return;
}

TimeVal PerSymbolConstraints::getHaltTimeFollowingRejection( Mkt::OrderResult error ) const {
  switch( error ) {
  case Mkt::UNSHORTABLE:
  case Mkt::GO_SHORT:
  case Mkt::NO_TRADING:
  case Mkt::NO_ROUTE:
  case Mkt::NO_REASON:
//  case Mkt::MARGIN_SERVER:
  case Mkt::POSITION_SERVER:
    return TimeVal(10,0); // GVNOTE: reduced this from 60 sec to 10 sec
  case Mkt::POSITION:
  case Mkt::SIZE:
  case Mkt::BUYING_POWER:
  case Mkt::RATE:
  case Mkt::NUM_ORDERS:
    return TimeVal(10,0); // 10 seconds
  case Mkt::GOOD:
    {
      TAEL_PRINTF(_logPrinter.get(), TAEL_ERROR, "%-5s ERROR: Shouldn't have got a rejection with error==Mkt::GOOD", _dm->symbol(_cid) );
      return TimeVal( 10,0 );
    }
    // should have covered them all, but to be on the safe side
  default:
    {
      TAEL_PRINTF(_logPrinter.get(), TAEL_WARN, "%-5s WARNING: Unfamiliar error upon rejection", _dm->symbol(_cid) );
      return TimeVal( 10,0 );
    }
  }
}

TradeConstraints::TradeConstraints()
  : _dm( factory<DataManager>::get(only::one) ),
    _logPrinter( factory<debug_stream>::get(std::string("trader")) ) 
{
  _perSymbolConstraints.resize( _dm->cidsize() );
  for( int cid=0; cid<_dm->cidsize(); cid++ )
    _perSymbolConstraints[cid] = new PerSymbolConstraints( cid );

  _dm -> add_listener_front( this );
}

TradeConstraints::~TradeConstraints() {
  for( int cid=0; cid<_dm->cidsize(); cid++ )
    delete _perSymbolConstraints[cid];
}
  
void TradeConstraints::set( ECN::ECN ecn, bool allowedToTrade ) {
  if( ecn != ECN::NYSE ) {
    for( int cid=0; cid<_dm->cidsize(); cid++ )
      _perSymbolConstraints[cid] -> set( ecn, allowedToTrade );
    TAEL_PRINTF(_logPrinter.get(), TAEL_INFO, "TradeConstraints: %s %s", (allowedToTrade ? "enabled" : "disabled"), ECN::desc(ecn) );
  } else { // for NYSE: enable only tape-A tickers
    factory<ExchangeTracker>::pointer exchangeT = factory<ExchangeTracker>::get(only::one);
    for( int cid=0; cid<_dm->cidsize(); cid++ ) 
      if( exchangeT->getTape( cid ) == Mkt::TAPE_A )
	_perSymbolConstraints[cid] -> set( ecn, allowedToTrade );
    TAEL_PRINTF(_logPrinter.get(), TAEL_INFO, "TradeConstraints: %s %s for tape A symbols",
			 (allowedToTrade ? "enabled" : "disabled"), ECN::desc(ecn) );
  }
}

void TradeConstraints::setOrderRate( int orderRate ){
  TAEL_PRINTF(_logPrinter.get(), TAEL_INFO, "TradeConstraints:: setting order rate to %d",orderRate );
   for( int cid=0; cid<_dm->cidsize(); cid++ )
     _perSymbolConstraints[cid] -> setOrderRate( orderRate );
}

void TradeConstraints::update( const OrderUpdate& ou ) {
  switch( ou.action() ) {
  case Mkt::CANCELING:
    if( ou.thisShares() == ou.sharesOpen() ) // order considered as canceling only if this is a full-cancel, not a "reduce-size"
      _perSymbolConstraints[ou.cid()] -> onFullCxling( ou );
    break;
  case Mkt::CXLREJECTED:
    _perSymbolConstraints[ou.cid()] -> onCxlRej( ou );
    break;
  case Mkt::CANCELED:
    if( ou.sharesOpen() == 0 ) // only care about full-cancelations
      _perSymbolConstraints[ou.cid()] -> onFullCxled( ou );
    break;
  case Mkt::REJECTED:
    _perSymbolConstraints[ou.cid()] -> onReject( ou );
    break;
  default:
    break;
  }
}

void TradeConstraints::update( const TimeUpdate &t ) {
    if (t.timer() == _dm->marketClose()) {
        for( int cid=0; cid<_dm->cidsize(); cid++ )
            _perSymbolConstraints[cid] -> set( false );
        TAEL_PRINTF(_logPrinter.get(), TAEL_INFO, "TradeConstraints:: MARKET IS CLOSED, should not trade anymore" );
    }
}


void TradeConstraints::onPlace( int cid ) {
  _perSymbolConstraints[cid]->onPlace();
}
