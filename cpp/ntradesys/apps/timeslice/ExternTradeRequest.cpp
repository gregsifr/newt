#include "ExternTradeRequest.h"

const double HIGH_PRIORITY = 0.0020;  // try to force trades


ExternTradeRequest::ExternTradeRequest(  ExecutionEngine& ee, 
					StockInfo& si, tael::Logger& logPrinter,
					TimeVal startTrade, TimeVal deadline, 
					int deltaTarget,double priority )
  : _dm( factory<DataManager>::get(only::one)),
    _ee( ee ),
    _si( si ),
    _startTradeTime( startTrade ),
    _deadline( deadline ),
    _deltaTarget( deltaTarget ),
    _tradingBegan( false ),
    _reachedTarget( deltaTarget == 0 ),
    _deadlinePassed( false ),
    _midPxAtStart( quan::ERR_PX ),
    _logPrinter( logPrinter ),
    _priority( priority )
{
  // Just make sure that _deadline >= _startTradeTime
  if( _deadline <= _startTradeTime )
    TAEL_PRINTF(&_logPrinter, TAEL_WARN, "%-5s WARNING: Initialized a ExternTradeRequest with deadline>=startTradeTime (%s >= %s)",
			_si.symbol(), DateTime( _deadline ).gettimestring(), DateTime( _startTradeTime ).gettimestring() );
}

ExternTradeRequest::~ExternTradeRequest() {
  // free the fills-vector
  for( unsigned int i=0; i<_fills.size(); i++ ){
    delete _fills[ i ];
    _fills[ i ] = NULL;
  }
}

// Is it time to start/stop trading?
void ExternTradeRequest::wakeup() {
  if( _deadlinePassed ) 
    // done with this trade-request
    return;
  
  // Should we start trading?
  if( !_tradingBegan && _dm->curtv() >= _startTradeTime ) 
    // send the new target to the execution-engine, update some internal variables and report
    onStartTrading();

  // Have we just arrived to deadline?
  if( !_deadlinePassed && _dm->curtv() >= _deadline )
    // keep the curr mid px, send "stop" to the execution-engine, update some internal variables and print a summary of this trade-request
    onDeadline();
}

// This function assumes that 'ou' is a fill-update (i.e. ou.state is in {Mkt::OPEN, Mkt::DONE})
void ExternTradeRequest::processAFill( const OrderUpdate& ou ) {
  // 3 sanity checks
  // 1. This is indeed a fill-update
  if( ou.state() != Mkt::DONE && ou.state() != Mkt::OPEN ) {
    TAEL_PRINTF(&_logPrinter, TAEL_ERROR, "%-5s ERROR/BUG: At %s, in processAFill(OrderUpdate) got an OrderUpdate with state=%s",
			_si.symbol(), DateTime(_dm->curtv()).gettimestring(), Mkt::OrderStateDesc[ou.state()] );
    return;
  }
  // 2. This request is indeed active
  if( !isActive() ) {
    TAEL_PRINTF(&_logPrinter, TAEL_ERROR, "%-5s ERROR/BUG: At %s, in processAFill(OrderUpdate) got an OrderUpdate even though this "
			"trade-request is not active", _si.symbol(), DateTime(_dm->curtv()).gettimestring() );
    return;
  }  
  // 3. This fill is in the right direction
  if( ( _deltaTarget > 0 && ou.dir() != Mkt::BUY ) || 
      ( _deltaTarget < 0 && ou.dir() == Mkt::BUY ) ) {
    TAEL_PRINTF(&_logPrinter, TAEL_ERROR, "%-5s ERROR/BUG: At %s, in processAFill(OrderUpdate) got an OrderUpdate with dir=%s even though "
			" our delta-to-target is of the opposite direction. ",
			_si.symbol(), DateTime(_dm->curtv()).gettimestring(), Mkt::OrderStateDesc[ou.state()] );
    return;
  }
  
  // update the "fills" vector
  int signedSize = ou.thisShares() * ( ou.dir() == Mkt::BUY ? 1 : -1 );
  ExternTradeFill* newFill = new ExternTradeFill( signedSize, ou.thisPrice(), ou.tv(), ou.totalFees() );
  _fills.push_back( newFill );
  
  // If reached target ==> update the relevant internal variables and print a summary
  bool reachedTarget = ( _deltaTarget >= 0 && getTotalSignedFills() >= _deltaTarget ) ||
    ( _deltaTarget  < 0 && getTotalSignedFills() <= _deltaTarget );
  if( reachedTarget )
    onReachingTarget();
  return;
}

// Get the total (signed) size of fills
int ExternTradeRequest::getTotalSignedFills() const { 
  int totalSignedSize = 0;
  
  for( unsigned int i=0; i<_fills.size(); i++ )
    totalSignedSize += _fills[i] -> signedSize;
  
  return totalSignedSize;
}

// Return the Transaction-costs of the trades so far (an average per share, in $)
double ExternTradeRequest::getNominalTCofFills() const {
  double cashInvested = 0.0; // positive is money we paid, negative is money we recieved
  int    totalSignedSize = 0; 
  
  for( unsigned int i=0; i<_fills.size(); i++ ) {
    //    cashInvested += _fills[i]->signedSize * _fills[i]->px + _fills[i]->fees; // fees are wrong in simulator, so we omit them.
    cashInvested += _fills[i]->signedSize * _fills[i]->px;
    totalSignedSize += _fills[i] -> signedSize;
  }
  
  // this is the cash we would have invested in a zero-tc world
  double idealCashInvested = totalSignedSize * _midPxAtStart;

  if( totalSignedSize==0 )
    return 0.0;
  return (cashInvested - idealCashInvested) / ABS(totalSignedSize);
}

// Return the Average time-till-fill 
// (return 0 if there are no fills)
double ExternTradeRequest::getAvgTimeToFill() const {
  double avgTimeTimesVol = 0.0; 
  int    totalSize = 0;

  for( unsigned int i=0; i<_fills.size(); i++ ) {
    TimeVal timeToFill = _fills[i]->time - _startTradeTime;
    double  timeToFillInSecs = timeToFill.sec() + 0.000001 * timeToFill.usec();
    avgTimeTimesVol += ABS(_fills[i]->signedSize) * timeToFillInSecs;
    totalSize += ABS( _fills[i]->signedSize );
  }

  if( totalSize==0 )
    return 0.0;
  return avgTimeTimesVol / totalSize;
}

// get a header for the summary-lines
int ExternTradeRequest::snprintSummaryHeader( char *buf, int n, const char* delimiter ) {

  return snprintf( buf, n,
		   "%-6s%s" // symbol
		   "%6s%s"  // delta-target
		   "%16s%s" // start-trade-time
		   "%16s%s" // deadline
		   "%8s%s"  // mid-px in trade-start
		   "%8s%s"  // mid-px in deadline
		   "%6s%s"  // actual fills
		   "%8s%s"  // fill-rate
		   "%12s%s" // avg-time-till-fill
		   "%12s%s" // Nominal TC in mills (i.e. in 10^(-4) of $)
		   "%12s",  // Relative TC in bps (i.e. in 10^(-4) of px)

		   "symbol", delimiter,   // symbol
		   "target", delimiter,   // delta-target
		   "start", delimiter,    // start-trade-time
		   "deadline", delimiter, // deadline
		   "midPxBeg", delimiter, // mid-px in trade-start
		   "midPxEnd", delimiter, // mid-px in deadline
		   "fills", delimiter,    // actual fills
		   "fillRate", delimiter, // fill-rate
		   "avgTime2Fill", delimiter, // avg-time-till-fill
		   "TC($e-4/sh)", delimiter,  // Nominal TC in mills (i.e. in 10^(-4) of $)
		   "TC(e-4/sh)"); // Relative TC in bps (i.e. in 10^(-4) of px)
}  

// put a summary of this trade-request into 'buf'
int ExternTradeRequest::snprintSummary( char *buf, int n, const char* delimiter ) const {
  int    totalSignedFills = getTotalSignedFills();
  double fillRate = (float)totalSignedFills / _deltaTarget;
  double nominalTcPerSh = getNominalTCofFills();
  double relativeTcPerSh = nominalTcPerSh / _midPxAtStart;

  return snprintf( buf, n,
		   "%-6s%s"  // symbol
		   "%6d%s"   // delta-target
		   "%16s%s"  // start-trade-time
		   "%16s%s"  // deadline
		   "%8.3f%s" // mid-px in trade-start
		   "%8.3f%s" // mid-px in deadline
		   "%6d%s"   // actual fills
		   "%8.2f%s" // fill-rate
		   "%12.2f%s"// avg-time-till-fill
		   "%12.2f%s"// Nominal TC in mills (i.e. in 10^(-4) of $)
		   "%12.2f", // Relative TC in bps (i.e. in 10^(-4) of px)

		   _si.symbol(), delimiter, // symbol
		   _deltaTarget, delimiter, // delta-target
		   DateTime(_startTradeTime).gettimestring(), delimiter, // start-trade-time
		   DateTime(_deadline).gettimestring(), delimiter, // deadline
		   _midPxAtStart, delimiter, // mid-px in trade-start
		   _midPxAtDeadline, delimiter, // mid-px in deadline
		   totalSignedFills, delimiter, // actual fills
		   fillRate, delimiter, // fill-rate
		   getAvgTimeToFill(), delimiter, // avg-time-till-fill
		   nominalTcPerSh * 10000, delimiter, // Nominal TC in mills (i.e. in 10^(-4) of $)
		   relativeTcPerSh * 10000 ); // Relative TC in bps (i.e. in 10^(-4) of px)
}

// send a trade-request, update internal variables
void ExternTradeRequest::onStartTrading() {
  // update internal variables
  _tradingBegan = true;
  _midPxAtStart = _si.midPx();

  // We should be in target position in Execution-Engine, and with no outstanding orders
  unsigned int cid = getCid();
  int currTargetPos = _ee.getTargetPosition(cid);
  int currPos = _dm->position( cid );
  if( currTargetPos != currPos ) 
    TAEL_PRINTF(&_logPrinter, TAEL_WARN, "%-5s WARNING: At %s, in onStartTrading(), execution-engine is still trading this symbol "
			"(curr_position=%d, target=%d)", _si.symbol(), DateTime(_dm->curtv()).gettimestring(), currPos, currTargetPos );
  if( getMarketCumSize( _dm->orderBook(), cid, Mkt::BID ) + 
      getMarketCumSize( _dm->orderBook(), cid, Mkt::ASK ) > 0 )
    TAEL_PRINTF(&_logPrinter, TAEL_WARN, "%-5s WARNING: At %s, there are still open orders of this symbol. "
			"(# bid-orders = %zu; # ask orders = %zu)", 
			_si.symbol(), DateTime(_dm->curtv()).gettimestring(), 
			getMarketCumSize(_dm->orderBook(), cid, Mkt::BID), 
			getMarketCumSize(_dm->orderBook(), cid, Mkt::ASK) );

  // Send a request to the execution-engine
  int newTarget = currPos + _deltaTarget; 
  // *** Changed format to targets instead of deltas
  //int newTarget = _deltaTarget; 
  _ee.tradeTo( cid, newTarget, _priority );
  TAEL_PRINTF(&_logPrinter, TAEL_INFO, "%-5s At %s sent new trade-request with target=%d (curr_mid_px=%.3f)",
		      _si.symbol(), DateTime(_dm->curtv()).gettimestring(), newTarget, _midPxAtStart );
}

// send a trade-request, update internal variables
void ExternTradeRequest::onReachingTarget() {
  // update internal variables
  _reachedTarget = true;

  // report
  TAEL_PRINTF(&_logPrinter, TAEL_INFO, "%-5s At %s target reached", _si.symbol(), DateTime(_dm->curtv()).gettimestring() );
}
  
// send a trade-request if needed, update internal variables
void ExternTradeRequest::onDeadline() {
  // update internal variables
  _deadlinePassed = true;
  _midPxAtDeadline = _si.midPx();
  
  // send a stop-request if needed
  if( !_reachedTarget ) {
    TAEL_PRINTF(&_logPrinter, TAEL_INFO, "%-5s At %s reached deadline before reaching target. Sending stop request."
			, _si.symbol(), DateTime(_dm->curtv()).gettimestring() );
    _ee.stop( getCid() );
  }
}
