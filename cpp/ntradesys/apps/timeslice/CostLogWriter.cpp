
#include "CostLogWriter.h"

CostLogWriter::CostLogWriter() :
  _logPrinter( factory<debug_stream>::get(std::string("cost")) ),
  _logPrinter2( factory<debug_stream>::get(std::string("cost2")))
{
  _dm = factory<DataManager>::find(only::one);
  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in TradeLogic::TradeLogic)" );

  _dm->add_listener( this );
}


void CostLogWriter::update( const TradeRequest& tr ) {
  double bid,ask;
  TimeVal t = _dm->curtv();
  getMarket(_dm->masterBook(), tr._cid, Mkt::BID, 0, &bid, 0);
  getMarket(_dm->masterBook(), tr._cid, Mkt::ASK, 0, &ask, 0); 
  TAEL_PRINTF(_logPrinter, TAEL_ERROR, &t, "REQ %s %d %d %d %.3f %.2f %.2f", _dm->symbol(tr._cid), tr._previousTarget, tr._targetPos, _dm->position(tr._cid), tr._priority, bid, ask  );
  TAEL_PRINTF(_logPrinter2, TAEL_ERROR, "%s REQ %s %d %d %d %.3f %.2f %.2f", DateTime(_dm->curtv()).gettimestring(),_dm->symbol(tr._cid), tr._previousTarget, tr._targetPos, _dm->position(tr._cid), tr._priority, bid, ask  );

}

void CostLogWriter::update( const OrderUpdate& ou ) { 
   if (ou.action() != Mkt::FILLED)
     return;
   double bid,ask;
   getMarket(_dm->masterBook(), ou.cid(), Mkt::BID, 0, &bid, 0);
   getMarket(_dm->masterBook(), ou.cid(), Mkt::ASK, 0, &ask, 0);
   string liqstr;
    switch (ou.liq()) {
            case Liq::add: liqstr = "add"; break;
            case Liq::remove: liqstr = "remove"; break;
            default: liqstr = "other"; break;
    }
    TimeVal t = _dm->curtv();
   TAEL_PRINTF(_logPrinter, TAEL_ERROR, &t, "FILL %s %s %d %d %.2f %.2f %.2f %s", _dm->symbol(ou.cid()), ECN::desc(ou.ecn()), 0, (ou.side() == Mkt::BID? 1:-1) * ou.thisShares(), ou.thisPrice(), bid,ask,liqstr.c_str());
   TAEL_PRINTF(_logPrinter2, TAEL_ERROR, "%s FILL %s %s %d %d %.2f %.2f %.2f %s", DateTime(_dm->curtv()).gettimestring(),_dm->symbol(ou.cid()), ECN::desc(ou.ecn()), 0, (ou.side() == Mkt::BID? 1:-1) * ou.thisShares(), ou.thisPrice(), bid,ask,liqstr.c_str());

}   




