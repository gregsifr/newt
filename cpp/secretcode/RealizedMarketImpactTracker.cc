/*
  RealizedMarketImpactTracker.cc
 
*/

#include "RealizedMarketImpactTracker.h"
using trc::compat::util::DateTime;

/************************************************************
  RealizedMarketImpactTracker code.
************************************************************/
const static string RMI_LOG_FILE = "realizedmarketimpact";
RealizedMarketImpactTracker::RealizedMarketImpactTracker() :
  _printMS(60 * 1000 * 10),
  _marketOpen(false),
  _lastPrintTV(0),
  _shsTL(0),
  _shsPL(0),
  _shsAccTL(0),
  _impactTL(0),
  _impactAccTL(0),
  _lastTRV(0, TradeRequest(0, 0, 0, 0, 0.0, 0.0, 0.0, _lastPrintTV, -1, -1))
{
  _dm = factory<DataManager>::find(only::one);
  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in RealizedMarketImpactTracker::RealizedMarketImpactTracker)" );

  _ddebug = factory<debug_stream>::get( RMI_LOG_FILE );
  if( !_ddebug )
    throw std::runtime_error( "Failed to get DebugStream from factory (in RealizedMarketImpactTracker::RealizedMarketImpactTracker)" );

  _tlMIM = factory<TakeLiquidityMarketImpactModel>::get(only::one);
  if (!_tlMIM) 
    throw std::runtime_error( "Failed to get TakeLiquidityMarketImpactModel from factory (in RealizedMarketImpactTracker::RealizedMarketImpactTracker)" );

  _orderRepo = factory<CentralOrderRepo>::get(only::one);
  if (!_orderRepo) 
    throw std::runtime_error( "Failed to get CentralOrderRepo from factory (in RealizedMarketImpactTracker::RealizedMarketImpactTracker)" );

  _shsTL.resize(_dm->cidsize(), 0);
  _shsPL.resize(_dm->cidsize(), 0); 
  _shsAccTL.resize(_dm->cidsize(), 0);  
  _impactTL.resize(_dm->cidsize(), 0.0);
  _impactAccTL.resize(_dm->cidsize(), 0.0);
  _lastTRV.resize(_dm->cidsize(), TradeRequest(-1, 0, 0, 0, 0.0, 0.0, 0.0, _lastPrintTV, -1, -1));

  // Try to add in front of other components, if possible, so that gets FILL updates
  //   before CentralOrderRepo does (and removes orders).
  _dm->add_listener_front(this);
}

RealizedMarketImpactTracker::~RealizedMarketImpactTracker() {

}

void RealizedMarketImpactTracker::clear(int cid) {
  _shsTL[cid] = 0;
  _shsPL[cid] = 0;
  _shsAccTL[cid] = 0;
  _impactTL[cid] = 0.0;
  _impactAccTL[cid] = 0.0;
}

/*
  Periodically dump state info to log file in easily human readable format....
  Example printout:
  TIMESTAMP  DELL  0  REAL-TC-$ 100.40  REAL-TC 0.00043567  MI-TL 0.00024567  MI-PL 0.00006000  MI-ACC-TL 0.00010000  SHS-TL 125  SHS-PL-400  SHS-ACC-TL 0 
*/
void RealizedMarketImpactTracker::printState() {
  TimeVal curtv = _dm->curtv();
  DateTime dt(curtv);
  string type("HEARTBEAT");
  TAEL_PRINTF(_ddebug.get(), TAEL_WARN, "RealizedMarketImpactTracker::printState called  curtv = %s", dt.getfulltime());
  for (int i=0;i<_dm->cidsize();i++) {
    printStockState(i, type);
  }
  _lastPrintTV = curtv;
}

void RealizedMarketImpactTracker::printStockState(int cid, string &type) {
  TAEL_PRINTF(_ddebug.get(), TAEL_INFO, "%-5s  CID %i  %s  REAL-MI-TL %.6f  REAL-MI-ACC-TL %.6f  REAL-SHS-TL %i  REAL-SHS-PL %i  REAL-SHS-ACC-TL %i",
		    _dm->symbol(cid), cid, type.c_str(),_impactTL[cid], _impactAccTL[cid], 
		    _shsTL[cid], _shsPL[cid], _shsAccTL[cid]);   
}

/*
  On recipt of new TradeRequest for stock:
  - Record new request.
  - Clear shs & MI data for stock.
*/
void RealizedMarketImpactTracker::update(const TradeRequest &tr) { 
  char buf[256];
  /*
    Print summary of new TR.
  */
  tr.snprint(buf, 256);
  TAEL_PRINTF(_ddebug.get(), TAEL_INFO, "%-5s  CID %i  NEW-TR   %s  ",
		    _dm->symbol(tr._cid), tr._cid, buf);
  /*
    Print summary of old TR.
  */
  _lastTRV[tr._cid].snprint(buf, 256);
  TAEL_PRINTF(_ddebug.get(), TAEL_INFO, "%-5s  CID %i  PREV-TR  %s  ",
		    _dm->symbol(tr._cid), tr._cid, buf);  

  /*  
    Print realized market info over course of old TR (for stock).
  */
  string type("ACC-IMP");
  printStockState(tr._cid, type);

  /*
    Re-set internal accumlated state for cid.
  */
  _lastTRV[tr._cid].applyNew(tr);
}


void RealizedMarketImpactTracker::update(const TimeUpdate &au) {
  if (au.timer() == _dm->marketOpen()) {
    onMarketOpen(au);
  } else if (au.timer() == _dm->marketClose()) {
    onMarketClose(au);
  } 
  onTimerUpdate(au);
}

void RealizedMarketImpactTracker::onMarketOpen(const TimeUpdate &au) {
  _marketOpen = true;
  _lastPrintTV = au.tv();
}

void RealizedMarketImpactTracker::onMarketClose(const TimeUpdate &au) {
  _marketOpen = false;
}

void RealizedMarketImpactTracker::onTimerUpdate(const TimeUpdate &au) {
  if (!_marketOpen) {
    return;
  }
  double msDiff = HFUtils::milliSecondsBetween(_lastPrintTV, au.tv());
  if (msDiff >= _printMS) {
    printState();
  }
}

/*
  On order-update:
  - Try to map orderID --> OrderPlacementSuggestion.
    - No map:  dump error message.
    - Map:  
      - Extract MI estimate from OrderStateRecord.
      - Guess whether order was liquidity-taking, liquidity-providing, or 
        accidentally liquidity-providing.
      - Incremement _shs?[cid] and _impact?{cid] fields accordingly.
*/
void RealizedMarketImpactTracker::update(const OrderUpdate &ou) {
  double orderMI;
  char buf[256];
  Liq::Liq liqType;
  // process FILL messages only.
  if (ou.action() != Mkt::FILLED) {
    return;
  }
  // Map from OrderUpdate --> OrderPlacementSuggestion --> MI estimate.
  if (!extractOrderInfo(ou, orderMI, liqType)) {
    ou.snprint(buf, 256);
    TAEL_PRINTF(_ddebug.get(), TAEL_WARN, "%-5s  CID %i  Unable to extract MarketImpact for order %s",
		  _dm->symbol(ou.cid()), ou.cid(), buf);
    return;
  }
  int tsign = (ou.dir() == Mkt::BUY ? 1 : -1);
  
  switch (liqType) {
  case Liq::remove:
    _shsTL[ou.cid()] += ou.thisShares() * tsign;
    _impactTL[ou.cid()] += orderMI;
    break;
  case Liq::add:
    _shsPL[ou.cid()] += ou.thisShares() * tsign;
    break;
  case Liq::other:
    _shsAccTL[ou.cid()] += ou.thisShares() * tsign;
    _impactAccTL[ou.cid()] += orderMI;    
    break;
  default:
    break;
  }
}

/*
  Extract information about order, including:
  - Market impact for shares reported as filled in this update only.
  - Guesstimate of liquidity type.
  In particular, attempts to classify orders that were intended to provide
    liquidity, but accidentally ended up taking liquidity instead.
  For such orders, the original order estimate of market-impact (0) is not used.
  Instead, market-impact is re-estimated from *current* market conditions.
*/
bool RealizedMarketImpactTracker::extractOrderInfo(const OrderUpdate &ou, double &fillMI, Liq::Liq &fillLT) {
  fillMI = 0.0;
  fillLT = Liq::UNKN;

  // Map from orderID --> OrderRecord.
  const OrderRecord *orec = _orderRepo->getOrderRecord(ou.id());
  if (orec == NULL) {
    return false;
  }

  // Guess whether this order was intended to provide liquidity but instead ended
  //   up taking liquidity.
  if (ou.liq() == Liq::remove && ou.timeout() > 0) {
    fillLT = Liq::other;
    MarketImpactEstimate est;
    _tlMIM->marketImpactFill(ou.cid(), ou.ecn(), ou.thisShares(), ou.price(), ou.side(), ou.timeout(), 
			     ou.invisible(), est);
    fillMI = est.permanentImpact();
  } else if (ou.liq() == Liq::remove) {
    fillLT = Liq::remove;
    fillMI = orec->_marketImpact * ou.thisShares() / ou.size();
  } else if (ou.liq() == Liq::add) {
    fillLT = Liq::add;
    fillMI = 0.0;
  } else {
    fillLT = Liq::UNKN;
    fillMI = 0.0;
  }
  return true;
}
