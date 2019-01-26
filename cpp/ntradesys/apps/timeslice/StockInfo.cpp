#include <cstdio>
#include <typeinfo>

#include "Client/bb/BBDataFileSource.h" // Bloomberg Data Source
#include <cl-util/float_cmp.h>
#include <BookTools.h>

#include "StockInfo.h"

using namespace clite::util;

StockInfo::StockInfo( DataManager& dm, unsigned int cid, tael::Logger& logPrinter )
  :_dm(dm), _cid(cid), _symbol( dm.symbol(cid) ),
   _closePx( quan::ERR_PX ),
   _mktStatus( quan::UNKNOWN ), _tradedToday( false ),
   _bidPx(quan::ERR_PX), _askPx(quan::ERR_PX), _bidSz(0), _askSz(0), 
   _logPrinter(logPrinter) {}

// Update the bid/ask numbers, as well as "market-status"
void StockInfo::updateMarketVars() {

  double bidPx, askPx;
  size_t bidSz, askSz;

  bool ok = 
    getMarket( _dm.masterBook(), _cid, Mkt::BID, 0, &bidPx, &bidSz ) &&
    getMarket( _dm.masterBook(), _cid, Mkt::ASK, 0, &askPx, &askSz );

  // invalid market --> report and return
  if( !ok ) {
    // report if that's new
    if( _mktStatus != quan::NODATA )
      TAEL_PRINTF(&_logPrinter, TAEL_ERROR, "%-5s ERROR: Couldn't get bid/ask info at %s", _symbol, getCurrTimeString(_dm) );
    _mktStatus = quan::NODATA;
    return;
  }

  _tradedToday = true; // The stock is indeed traded today (meant to deal with tickers we have no data for)

  // Crossed market --> report and return
  bool crossedMkt = clite::util::cmp<4>::LT(askPx, bidPx);  // both positive spread and zero-spread are considered valid for trading
  if( crossedMkt ) {
    // if that's new, report the negative spread as well as its source
    if( _mktStatus != quan::CROSSED )
      //TAEL_PRINTF(&_logPrinter, TAEL_WARN, "%-5s WARNING: Market crossed at %s: (bid=%.2f,ask=%.2f)",
      //                    _symbol, getCurrTimeString(_dm), bidPx, askPx );
    _mktStatus = quan::CROSSED;
    return;
  }
  // Ok, Market is valid --> report
  if( _mktStatus == quan::CROSSED || _mktStatus == quan::NODATA )
    //TAEL_PRINTF(&_logPrinter, TAEL_INFO, "%-5s market is finally valid (possibly locked) at %s: (bid=%.2f,ask=%.2f)",
    //_symbol, getCurrTimeString(_dm), bidPx, askPx );

  // Is market locked (spread==0)?
  if( clite::util::cmp<2>::EQ(askPx, bidPx) ) {
    // if that's new, report it
    if( _mktStatus != quan::LOCKED )
      //TAEL_PRINTF(&_logPrinter, TAEL_WARN, "%-5s WARNING: Market is locked at %s: (bid=ask=%.2f)",
      // _symbol, getCurrTimeString(_dm), bidPx );
    _mktStatus = quan::LOCKED;
  }
  // If market is normal (valid data and positive spread)
  else {
    // If that's new ==> report
    if( _mktStatus != quan::NORMAL && _mktStatus != quan::UNKNOWN )
      //TAEL_PRINTF(&_logPrinter, TAEL_INFO, "%-5s market is finally back to normal at %s with spread of (bid=%.2f ask=%.2f) "
      //                    "(sizes: bid=%d ask=%d)", _symbol, getCurrTimeString(_dm), bidPx, askPx, bidSz, askSz );
    _mktStatus = quan::NORMAL;
  }

  // update the bid & ask prices
  _bidPx = bidPx;
  _askPx = askPx;

  // update the bid & ask PASSIVE sizes. 
  // These is only an approximation, as we don't really know which of our open orders are already/still listed in the market
  int ourBidSize = getMarketCumSize( _dm.orderBook(), _cid, Mkt::BID, bidPx );
  int ourAskSize = getMarketCumSize( _dm.orderBook(), _cid, Mkt::ASK, askPx );
  _bidSz = ( bidSz > ourBidSize ? bidSz - ourBidSize : bidSz );  // To avoid a 0 bid-size situation
  _askSz = ( askSz > ourAskSize ? askSz - ourAskSize : askSz );  // To avoid a 0 ask-size situation

  // TODO: check if we're alone at inside, in which case the price is wrong
  return;
} // end of StockInfo::updateMarketVars


// get the historical closing price of the current date of DataManager, as recorded by Bloomberg
// if not found, return quan::ERR_PX
double StockInfo::getClosePrice() const {

  HistBBDataSource hbds( _symbol );

  // set the date
  int date = DateTime(_dm.curtv()).getintdate(); // the date as integer in YYYYMMDD format
  hbds.setdate( date );
  // there's a bug when it's the first day of the month, so there's a need in a special treatment
  if( date % 100 == 1 ) {
    // set the date to the previous day
    int secsInDay = 24*60*60;
    TimeVal tv( _dm.curtv().sec()-secsInDay, _dm.curtv().usec() );
    hbds.setdate( DateTime( tv ).getintdate() );
  }

  // roll the events until you get the closing price (This is copied from watcher (via Amrit). I don't completely understand it.
  BBEvent *be = NULL;
  Event *e = NULL;

  while ((e = hbds.next()) != NULL) {
    be = dynamic_cast<BBEvent *>(e);
    if (e->msgtype == BB_MESSAGE && be != NULL) {
      if( be->date() > date )
	break;
      if( be->date() < date )
	continue;
      // so we're in the right date
      if (be->lastpx() != 0.00)
	return be->lastpx();
    }
  }

  TAEL_PRINTF(&_logPrinter, TAEL_ERROR, "ERROR: %-5s couldn't find closing price via Bloomberg for day %d", _symbol, date );
  return quan::ERR_PX;
}

