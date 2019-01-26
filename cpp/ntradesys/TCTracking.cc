/*
  TCTracking.cc - Short for Transaction Cost Tracking.
  Code/utilities used for recording trade rquest, order placement,
    and fill info, and for trying to map those back to transaction cost
    calculation/estimation.
  Many of the widgets defined in this file are written to help debugging while
    test-driving the execution engine or new components/trading-strategies/trading-algos
    thereof.  They are generally *not* written to perform efficiently with
    larger numbers of trade-requests, orders, or market responses.
  They probably should *not* be used as part of the production execution engine code
    without modification.
  Part of ntradesys project.  
  Code by Matt Cheyney, 2008.  All rights reserved.
*/

// ntradesys includes
#include "TCTracking.h"
#include "HFUtils.h"

// cpputil includes
#include "RUtils.h"

using trc::compat::util::DateTime;
const string RELEVANT_LOG_FILE = "tctracking";
const trc::tael::Severity TROPFTrackerImmediate::plevel = TAEL_WARN;

/***********************************************************
  TradeRequestRecord code
***********************************************************/
//TradeRequestRecord::TradeRequestRecord() :
//  _initPos(0),
//  _targetPos(0),
//  _priority(0),
//  _cbid(0.0),
//  _cask(0.0),
//  _recvTime(0),
//  _fills(),
//  _cancels(0),
//  _places(0)
//{
//
//  
//}
TradeRequestRecord::TradeRequestRecord(const TradeRequest &tr) :
  _tr(tr),
  _places(),
  _cancels(),
  _fills()
{
  
}

TradeRequestRecord::~TradeRequestRecord() {
  // nop sufficient for now.
}

void TradeRequestRecord::addFill(const OrderUpdate &ou, double cbid, double cask) {
  _fills.AddFill(ou, cbid, cask);
}

void TradeRequestRecord::addOrder(const OrderPlacementSuggestion &ops) {
  _places.push_back(ops);
}

void TradeRequestRecord::addCancelSuggestion(const OrderCancelSuggestion &ca) {
  _cancels.push_back(ca);
}

bool TradeRequestRecord::initialMidPx(double *dv) {
  *dv = (_tr._cbid + _tr._cask)/2.0;
  return true;
}

bool TradeRequestRecord::initialPx(Mkt::Side side, double *dv) {
  if (side == Mkt::BID) {
    *dv = _tr._cbid;
  } else {
    *dv = _tr._cask;
  }
  return true;
}


double TradeRequestRecord::sAvgFillPx() {
  return _fills.sAvgFillPx();
}

double TradeRequestRecord::wAvgFillPx() {
  return _fills.wAvgFillPx();
}

/*
  Defined to be 0 if we don't have a trusted initialMidPx.
  Positive # = good = made $ on trading.
  Negative # = bad = cost us $ on trading.
*/
double TradeRequestRecord::sAvgSlippage() {
  double initialMid;
  if (initialMidPx(&initialMid) == false) {
    return 0;
  }
  double safp = sAvgFillPx();
  if (_tr._targetPos >= _tr._initPos) {
    return (initialMid - safp);
  }
  return (safp - initialMid);
}

/*
  Defined to be 0 if we don't have a trusted initialMidPx.
  Positive # = good = made $ on trading.
  Negative # = bad = cost us $ on trading.
*/
double TradeRequestRecord::wAvgSlippage() {
  double initialMid;
  if (initialMidPx(&initialMid) == false) {
    return 0;
  }
  double safp = wAvgFillPx();
  if (_tr._targetPos >= _tr._initPos) {
    return (initialMid - safp);
  }
  return (safp - initialMid);
}

/*
  Defined to be 0 for no fills.
*/
double TradeRequestRecord::totalFillMinutes() {

  // Move to TROrderPlacementSet::totalFillMinutes()....
  TimeVal lastFillTime;
  if (!_fills.lastFillTime(lastFillTime)) {
    return 0.0;
  }
  TimeVal diff = lastFillTime - _tr._recvTime;
  long numSeconds = diff.sec();
  long numUSeconds = diff.usec();
  double ret = (numSeconds / 60.0) + (numUSeconds / (60000000.0));
  return ret;
}


/*
  Walk through fills, guessing which ones result from opb.
  Stick (copies of) those into fillMe.
  How do we guess whether a Fill is associated with a given OPB?
  - Same dir, fillT, ecn.
*/
int TradeRequestRecord::associateFills(const OrderPlacementSuggestion &ops,
					vector<Exec> &fillMe) {
  fillMe.clear();
  vector<Exec>::iterator it;
  int ret = 0;
  for (it = _fills.begin(); it != _fills.end(); ++it) {
    Exec e = (*it);
    if (ops.getOrderId() == e._orderID) {
      fillMe.push_back(e);
      ret++;
    }
  }
  return ret;
}

int TradeRequestRecord::associateCancelSuggestions(const OrderPlacementSuggestion &opr,
						vector<OrderCancelSuggestion> &fillMe) {
  fillMe.clear();
  vector<OrderCancelSuggestion>::iterator it;
  int ret = 0;
  for (it = _cancels.begin(); it != _cancels.end(); it++) {
    if (opr.getOrderId() == it->_orderId) {
      fillMe.push_back(*it);
      ret++;
    }
  }
  return ret;
}

/*
  Output:
  - Number of total fills.
  - Initial mid px.
  - RecvTime
  - Straight avg fill px.
  - Weighted avg fill px.
  - Straight avg fill time.
  - Weighted avg fill time.
  Notes:
  - For slippage numbers:
    - Positive # ==> made $ trading ==> good.
    - Negative # ==> lost $ trading ==> bad.
*/
void TradeRequestRecord::printTCSummaryInfo(DataManager *dm, 
					    debug_stream *tdebug, 
					    tael::Severity plevel,
					    string& prefix, const char* symbol) {
  int numf = numFills();
  DateTime dt(_tr._recvTime);
  // Print aggregate info about TradeRequestRecord
  TAEL_PRINTF(tdebug, plevel, "%-5s %s TradeRequestRecord::printTCSummaryInfo", symbol, prefix.c_str());
  TAEL_PRINTF(tdebug, plevel, "%-5s %s   Aggregate", symbol, prefix.c_str());
  char buf[1024];
  snprint_base(buf, 1024);
  TAEL_PRINTF(tdebug, plevel, "%-5s %s     %s", symbol, prefix.c_str(), buf);

  // Print TC Info across the entire TradeRequestRecord.
  if (numf > 0) {
    snprint_fillandslip(buf, 1024);
    TAEL_PRINTF(tdebug, plevel, "%-5s %s     %s", symbol, prefix.c_str(), buf);
  } 

  // Print TC info broken down per OrderPlacement.
  // Basic algorithm:
  // - Walk through all OrderPlacement records opr.
  //   - print info on opr, e.g. opr...stock, ecn, dir, size, price, ordSentTime.
  //   - If we got any fills, print aggregate fill px & slippage info for the OrderPlacement,
  //     e.g. Fill Px - s avg, w avg, total (abs) $ notional
  //          Slippage - s avg, w avg, fraction
  //   - For each such record, match fills corresponding to record.
  //     - Print info on each fill, e.g. fill...px, sz, fillT, tc 
  TAEL_PRINTF(tdebug, plevel, "%-5s %s   Per-OrderPlacement", symbol, prefix.c_str());
  string nprefix = prefix + "     ";
  vector<OrderPlacementSuggestion >::iterator it;
  vector<Exec> tFills;
  int totalFillsAssociated = 0, thisFillsAssociated = 0;
  vector<OrderCancelSuggestion> tCancels;
  int totalCancelsAssociated = 0, thisCancelsAssociated = 0;
  for (it = _places.begin(); it != _places.end(); ++it) {
    it->snprint(buf, 1024);
    TAEL_PRINTF(tdebug, plevel, "%-5s %s    %s", symbol, prefix.c_str(), buf);
    thisFillsAssociated = associateFills(*it, tFills);
    totalFillsAssociated += thisFillsAssociated;
    vector<Exec>::iterator itInner;
    for (itInner = tFills.begin(); itInner != tFills.end(); itInner++) {
      itInner->snprint(buf, 1024);
      TAEL_PRINTF(tdebug, plevel, "%-5s %s      %s", symbol, prefix.c_str(), buf);
    }
    thisCancelsAssociated = associateCancelSuggestions(*it, tCancels);
    totalCancelsAssociated += thisCancelsAssociated;
    vector<OrderCancelSuggestion>::iterator itInner2;
    for (itInner2 = tCancels.begin(); itInner2 != tCancels.end(); itInner2++) {
      OrderCancelSuggestion ocs = (*itInner2);
      snprint_ocs(buf, 1024, ocs);
      TAEL_PRINTF(tdebug, plevel, "%-5s %s      %s", symbol, prefix.c_str(), buf);
    }
  }
  int numFillsMissing = numFills() - totalFillsAssociated;
  if (numFillsMissing > 0) {
    TAEL_PRINTF(tdebug, plevel, "%-5s %s    WARN - %i fills not associated with any OPR",
		  symbol, prefix.c_str(), numFillsMissing);
  } 
}


/*
  CXLR 14:09:04:561172  Px3.20  SZ621  DIRSELL  ECNISLD  RSNQ_POS_UNFVR   OWN13  CBBID3.19  CBASK  3.20
*/
int TradeRequestRecord::snprint_ocs(char *buf, int n, 
				    const OrderCancelSuggestion &ocs) {
  return ocs.snprint(buf, n);
}

/*
OP 14:27:58:261000  PX3.19  SZ621  DIRSELL  ECNISLD  RSNFLW_LEADER  OWN18  CBBID3.19  CBASK3.19

int TradeRequestRecord::snprint_ord(char *buf, int n,
				    const OrderRecord &orec, 
				    const Order *ord) {
  DateTime dt(orec.tv());
  // Print aggregate info about order.
  return snprintf(buf, n, "OP %02d:%02d:%02d:%06d  PX%f  SZ%i  DIR%s  ECN%s  RSN%s  CMPID%i  BPX%.2f  APX%.2f",
		  dt.hh(), dt.mm(), dt.ss(), dt.usec(), ord->price(), ord->size(), 
		  Mkt::TradeDesc[ord->dir()], ECN::desc(ord->ecn()),
		  OrderPlacementSuggestion::PlacementReasonDesc[orec.placementReason()],
		  orec.componentId(), 
		  orec.cbbid(), orec.cbask());   
}
*/

int TradeRequestRecord::snprint_base(char *buf, int n) {
  int totShsFilled = totalSharesFilled();
  int totShsWanted = _tr._targetPos - _tr._initPos;
  DateTime dt(_tr._recvTime);
  double initialBid = -1, initialAsk = -1;
  initialPx(Mkt::BID, &initialBid);
  initialPx(Mkt::ASK, &initialAsk);

  // Print aggregate info about TradeRequestRecord
  return snprintf(buf, n, "TC-BASE %02d:%02d:%02d:%06d  BPX%.2f  APX%.2f %i -> %i  P%.6f  F%i  U%i",
		  dt.hh(), dt.mm(), dt.ss(), dt.usec(), initialBid, initialAsk, 
		  _tr._initPos, _tr._targetPos, _tr._priority, 
		  totShsFilled, totShsWanted - totShsFilled);
}

int TradeRequestRecord::snprint_fillandslip(char *buf, int n) {
  double wafp = wAvgFillPx();
  double was = wAvgSlippage();
  int totShsFilled = totalSharesFilled();
  double totSlippage = was * abs(totShsFilled);
  double totCost = fabs(wafp * totShsFilled); 
  
  return snprintf(buf, n, "TC-FILL SA%.2f  WA%.2f  NOT%.2f    TC-SLIP  SA%.4f  WA%.4f  TOT%.2f  FRAC%.4f  TC-TIME  MINS%.4f",
		  sAvgFillPx(), wafp, totCost, sAvgSlippage(), was, totSlippage, 
		  totSlippage/totCost, totalFillMinutes()); 
}


/***********************************************************
  TradeRequestRecordSet code
***********************************************************/
TradeRequestRecordSet::TradeRequestRecordSet() :
  trRecords()
{
  
}

TradeRequestRecordSet::~TradeRequestRecordSet() {
  clear();
}

void TradeRequestRecordSet::clear() {
  trRecords.clear();
}

void TradeRequestRecordSet::addTradeRequest(const TradeRequest &tr) {
  TradeRequestRecord trr(tr);
  trRecords.push_back(trr);
}

/*
  Assumes that fill corresponds to most recent TradeRequest.
  May not always be correct, e.g. when 
  - get a TradeRequest.
    - place orders & try to trade for a while.
  - get a new trade request.
    - cancel out of old orders (takes time to occur).
    - start placing some new ones.
    - get filled on old limit orders from previous trade request,
      before cancel request reaches exchange/ecn & get cancelled out.
*/
void TradeRequestRecordSet::addFill(const OrderUpdate &ou, double cbid, double cask) {
  int idx = trRecords.size() - 1;
  assert(idx >= 0);
  trRecords[idx].addFill(ou, cbid, cask);
}


void TradeRequestRecordSet::printTCSummaryInfo(DataManager *dm, 
					       debug_stream *tdebug, 
					       tael::Severity plevel,
					       string& prefix, const char* symbol) {
  int numRequests = trRecords.size();
  TAEL_PRINTF(tdebug, plevel, "%-5s %s TradeRequestRecordSet::printTCSummaryInfo - %i requests",
		symbol, prefix.c_str(), numRequests);
  string nprefix = prefix + "  ";
  for (int i=0;i<numRequests;i++) {
    trRecords[i].printTCSummaryInfo(dm, tdebug, plevel, nprefix, symbol);
  }
}

/*
  Add order placement key/record.  Assumes that new OPRs added
    always correspond to the most recent TradeRequest.
*/
void TradeRequestRecordSet::addOrder(const OrderPlacementSuggestion &ops) {
  int idx = trRecords.size() - 1;
  assert(idx >= 0);
  trRecords[idx].addOrder(ops);
}

void TradeRequestRecordSet::addCancelSuggestion(const OrderCancelSuggestion &car) {
  int idx = trRecords.size() - 1;
  assert(idx >= 0);
  trRecords[idx].addCancelSuggestion(car);
}


/***********************************************************
  TROPFTracker code
***********************************************************/
TROPFTracker::TROPFTracker() :
  _trsSet(),
  _printOnClose(true),
  _seenMktClose(false),
  _mktCloseTV(),
  _printedOnClose(false)
{
  _dm = factory<DataManager>::find(only::one);
  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in ImbTracker::ImbTracker)" );

  _trsSet.resize(_dm->cidsize());
  _ddebug = factory<debug_stream>::get( RELEVANT_LOG_FILE );

  _dm->add_listener(this);
}

TROPFTracker::~TROPFTracker() {
  // nop should be sufficient for now....
}

/*
  Called on OrderUpdate.  
  On fills only, pass along to _trsSet.
*/
void TROPFTracker::update(const OrderUpdate &ou) {
  if (ou.action() !=  Mkt::FILLED) {
    return;
  }
  double bidPx = -1.0, askPx = -1.0;
  HFUtils::bestPrice(_dm.get(), ou.cid(), Mkt::BID, bidPx);
  HFUtils::bestPrice(_dm.get(), ou.cid(), Mkt::ASK, askPx);
  _trsSet[ou.cid()].addFill(ou, bidPx, askPx);
}

/*
  Called on order cancel attempts.
  Pass along to _trsSet.
*/
void TROPFTracker::update(const OrderCancelSuggestion &ocs) {
  const Order *ord = _dm->getOrder(ocs._orderId);
  if (ord != NULL) {
    _trsSet[ord->cid()].addCancelSuggestion(ocs);
  }
}

/*
  Called on OrderPlacements.
  Pass along to _trsSet.
  
*/
void TROPFTracker::update(const OrderPlacementSuggestion &ops) {
  _trsSet[ops._cid].addOrder(ops);  
}

/*
  Call on new Trade Requests.
*/
void TROPFTracker::update(const TradeRequest &tr) {
  _trsSet[tr._cid].addTradeRequest(tr);
}

void TROPFTracker::update(const TimeUpdate &au) {
  if (au.timer() == _dm->marketOpen()) {
    _seenMktClose = false;
    _printedOnClose = false;
  }
  if (au.timer() == _dm->marketClose()) {
    _seenMktClose = true;
    _printedOnClose = false;    
    _mktCloseTV = au.tv();
  }
  if (_seenMktClose == true && _printedOnClose == false) {
    double diff = HFUtils::milliSecondsBetween(_mktCloseTV, _dm->curtv());
    if (diff >= PRINT_ON_CLOSE_DELAY_MILLISECONDS) {
      _printedOnClose = true;
      string prefix("");
      printTCSummaryInfo(TAEL_WARN, prefix);
    }
  }
}


void TROPFTracker::printTCSummaryInfo(debug_stream *tdebug, 
				      tael::Severity plevel,
				      string& prefix) {
  TAEL_PRINTF(tdebug, plevel, "%s TROPFTracker::printTCSummaryInfo called, _trsSet has %i stocks",
		 prefix.c_str(), (int)_trsSet.size());
  string nprefix = prefix + "  ";
  vector<TradeRequestRecordSet>::iterator it;
  int i = 0;
  for (it = _trsSet.begin(); it != _trsSet.end(); ++it) {
    const char* symbol = _dm->symbol( i );
    TAEL_PRINTF(tdebug, plevel, "%-5s %s stock %i (ticker %s)", symbol, prefix.c_str(), i, _dm->symbol(i));
    it->printTCSummaryInfo(_dm.get(), tdebug, plevel, nprefix, symbol);
    i++;
  }
  TAEL_PRINTF(tdebug, plevel, "%s TROPFTracker::printTCSummaryInfo done", prefix.c_str());
}

void TROPFTracker::printTCSummaryInfo(tael::Severity plevel,
				      string& prefix) {
  printTCSummaryInfo(_ddebug.get(), plevel, prefix);
}


/***********************************************************
  TROPFTrackerImmediate code
***********************************************************/
TROPFTrackerImmediate::TROPFTrackerImmediate() :
  _prefix("        ")
{
  _dm = factory<DataManager>::find(only::one);
  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in ImbTracker::ImbTracker)" );
  _ddebug = factory<debug_stream>::get( RELEVANT_LOG_FILE );

  _dm->add_listener(this);
}

TROPFTrackerImmediate::~TROPFTrackerImmediate() {

}

/*
e.g.:
11:01:06.345373 COH             CXLR 09:30:35:280224  ORDID78351001  CMPID1  RSNNO_FOLLOWRS   BPX36.86  APX36.89

*/
void TROPFTrackerImmediate::update(const OrderUpdate &ou) {
  if (ou.action() !=  Mkt::FILLED) {
    return;
  }
  double bidPx = -1.0, askPx = -1.0;
  HFUtils::bestPrice(_dm.get(), ou.cid(), Mkt::BID, bidPx);
  HFUtils::bestPrice(_dm.get(), ou.cid(), Mkt::ASK, askPx);
  char buf[1024];
  Exec e(ou, bidPx, askPx);
  e.snprint(buf, 1024);
  const char* symbol = _dm->symbol( ou.cid() );
  TAEL_PRINTF(_ddebug.get(), plevel, "%-5s %s      %s", symbol, _prefix.c_str(), buf);
}

/*
e.g.:
11:01:06.345373 COH             CXLR 09:30:35:280224  ORDID78351001  CMPID1  RSNNO_FOLLOWRS   BPX36.86  APX36.89

*/
void TROPFTrackerImmediate::update(const OrderCancelSuggestion &ocs) {
  char buf[1024];
  ocs.snprint(buf, 1024);
  const Order* order = _dm->getOrder( ocs._orderId );
  if (!order) {
	  TAEL_PRINTF(_ddebug.get(), TAEL_ERROR, "In TROPFTrackerImmediate::update: could not "
			  "get order from OrderCancelSuggestion with associated orderId: %d", ocs._orderId);
	  return;
  }
  const char* symbol = _dm->symbol( order->cid() );
  TAEL_PRINTF(_ddebug.get(), plevel, "%-5s %s      %s", symbol, _prefix.c_str(), buf);
}

/*
e.g.:
11:01:06.345202 COH           OPS CID0  ECNISLD  SIDEBID  SIZE259  PX36.79  TOUT0  TLID8  CMPID7  CSQN0  RSNTAKE_INVSB  09:30:19:099981  CBID36.62  CASK37.00

*/
void TROPFTrackerImmediate::update(const OrderPlacementSuggestion &ops) {
  char buf[1024];
  ops.snprint(buf, 1024);
  const char* symbol = _dm->symbol( ops._cid );
  TAEL_PRINTF(_ddebug.get(), plevel, "%-5s %s    %s", symbol, _prefix.c_str(), buf);
}

/*
  e.g.:
11:01:06.345090 COH        TradeRequestRecord
11:01:06.345113 COH            TC-BASE 09:30:00:001920  BPX40.15  APX37.20 0 -> 259  P0.000110  F259  U0

*/
void TROPFTrackerImmediate::update(const TradeRequest &tr) {
  const char* symbol = _dm->symbol( tr._cid );
  char buf[1024];
  tr.snprint(buf, 1024);
  TAEL_PRINTF(_ddebug.get(), plevel, "%-5s %s %s", symbol, _prefix.c_str(), buf);
}

