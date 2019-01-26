
#include "PNLTracker.h"
#include "DataManager.h"

const string RELEVANT_LOG_FILE = "pnltracker";

/*
  PNLTracker code!!!!
*/
PNLTracker::PNLTracker() :
  _fills(0),
  _printOnClose(true),
  _seenMktClose(false),
  _mktCloseTV(),
  _printedOnClose(false)
{
  _dm = factory<DataManager>::find(only::one);
  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in PNLTracker::PNLTracker)" );
  _fills.resize( _dm->cidsize());

  _ddebug = factory<debug_stream>::get( RELEVANT_LOG_FILE );

  //
  //  Unclear whether PNLTracker should explicitly register for updates, or
  //    allow its creator to do so for it.  Here we choose to have the PNLTracker
  //    explicitly register with the DataManager, although there is an argument
  //    that the constructor should do the registration instead, because its unclear
  //    here inside the PNLTracker constructor whether the PNLTracker should
  //    register with the DataManager directly, or with the TSIDispatcher.
  //  
  //
  _dm->add_listener(this);
}

PNLTracker::PNLTracker(vector<int> &initPosV, vector<double> &initPxV) :
  _fills(0),
  _printOnClose(true)
{
  _dm = factory<DataManager>::find(only::one);
  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in PNLTracker::PNLTracker)" );
  _fills.resize( _dm->cidsize());
  _ddebug = factory<debug_stream>::get( RELEVANT_LOG_FILE );
  _dm->add_listener(this);

  specifyInitialPositions(initPosV, initPxV);  
}

PNLTracker::~PNLTracker() {
  // Will this successfully catch the case where the DataManager has already been 
  //   deleted before the tracker?
  //if (dynamic_cast<DataManager*>(_dm) != NULL) {
  //  _dm->removeListener(this);
  //}
}


void PNLTracker::specifyInitialPositions(vector<int> &initPosV, 
					 vector<double> &initPxV) {
  TimeVal dummyTV;
  int numStocks = _dm->cidsize();
  assert(initPosV.size() == numStocks);
  assert(initPxV.size() == numStocks);
  for (int i=0;i<numStocks;i++) {
    int ipos = initPosV[i];
    double ipx = initPxV[i];
    if (ipos > 0) {
      _fills[i].AddFill(ipx, Mkt::BUY, Mkt::BID, ECN::UNKN, ipos, dummyTV, 0.0, ipx, ipx);
    } else if (ipos < 0) {
      _fills[i].AddFill(ipx, Mkt::SHORT, Mkt::ASK, ECN::UNKN, ipos, dummyTV, 0.0, ipx, ipx);
    }
  }
}

void PNLTracker::update(const OrderUpdate &ou) {
  // Only add fill info on Mkt::OPEN (partial fill), or Mkt::DONE (total fill).
  if (ou.action() == Mkt::FILLED) {
    double bidPx = -1, askPx = -1;
    //int bidSz, askSz;
    //_dm->getMarket(ou.cid, Mkt::BID, 0, &bidPx, &bidSz);
    //_dm->getMarket(ou.cid, Mkt::ASK, 0, &askPx, &askSz);
    HFUtils::bestPrice(_dm.get(), ou.cid(), Mkt::BID, bidPx);
    HFUtils::bestPrice(_dm.get(), ou.cid(), Mkt::ASK, askPx);
    _fills[ou.cid()].AddFill(ou,
			   bidPx, askPx);
  }  
}

void PNLTracker::update(const TimeUpdate &au) {
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
      printPNL(TAEL_WARN);
    }
  }
}


double PNLTracker::getPNL(int cid, double prcClose, bool includeTC) {
  double rv = _fills[cid].computeChangeCash(prcClose, includeTC);
  return rv;
}

double PNLTracker::getPNL(vector<double> &prcCloseV, bool includeTC) {
  double rv = 0;
  int numStocks = _dm->cidsize();
  for (int i=0;i<numStocks;i++) {
    double prcClose = prcCloseV[i];
    rv += getPNL(i, prcClose, includeTC);
  }
  return rv;
}

int PNLTracker::getAbsoluteSharesFilled(int cid) {
  int rv = _fills[cid].absoluteSharesFilled();
  return rv;
}

int PNLTracker::getAbsoluteSharesFilled() {
  int rv = 0;
  for (unsigned int i=0;i<_fills.size();++i) {
    rv += getAbsoluteSharesFilled(i);
  }
  return rv;
}

double PNLTracker::getAbsoluteDollarsFilled(int cid) {
  double rv = _fills[cid].absoluteDollarsFilled();
  return rv;
}

double PNLTracker::getAbsoluteDollarsFilled() {
  double rv = 0;
  for (unsigned int i=0;i<_fills.size();++i) {
    rv += getAbsoluteDollarsFilled(i);
  }
  return rv;
}

void PNLTracker::printPNL(debug_stream *pdebug, tael::Severity plevel) {
  double totPNLInclTC = 0.0;
  double totPNLExclTC = 0.0;
  double thisPNLInclTC, thisPNLExclTC, midPx, absDollars;
  int absShares;
  int numStocks = _dm->cidsize();
  for (int i=0;i<numStocks;i++) {
    if (!HFUtils::bestMid(_dm.get(), i, midPx, NULL)) {
    	TAEL_PRINTF(pdebug, plevel, "%-5s    Stock %i - can't find valid mid px, not including in PNL calc",
		     _dm->symbol(i), i);
      midPx = 0.0;
    } else {
      thisPNLInclTC = getPNL(i, midPx, true);
      thisPNLExclTC = getPNL(i, midPx, false);
      absShares = getAbsoluteSharesFilled(i);
      absDollars = getAbsoluteDollarsFilled(i);
      TAEL_PRINTF(pdebug, plevel, "%-5s    Stock %i - PNL = %f (excl TC),  or %f (incl TC),  trading volume %5i shares %.2f dollars",
		     _dm->symbol(i), i, thisPNLExclTC, thisPNLInclTC, absShares, absDollars);
      totPNLInclTC += thisPNLInclTC;
      totPNLExclTC += thisPNLExclTC;
    }
  } 
  TAEL_PRINTF(pdebug, plevel, "    Total PNL = %f (excl TC), or %f (incl TC)",
		     totPNLExclTC, totPNLInclTC); 
}

void PNLTracker::printPNL(tael::Severity plevel) {
  printPNL(_ddebug.get(), plevel);
}

/*
  PNLTrackerExplicit code!!!!
*/

PNLTrackerExplicit::PNLTrackerExplicit() :
  PNLTracker()
{

}

PNLTrackerExplicit::PNLTrackerExplicit(vector<int> &initPosV, vector<double> &initPxV) :
  PNLTracker(initPosV, initPxV)
{

}

PNLTrackerExplicit::~PNLTrackerExplicit() {
  // nop should currently be sufficient....
}

//  Explicitly set to NOP - not a bug.
void PNLTrackerExplicit::update( const OrderUpdate &u ) {

}


void PNLTrackerExplicit::addFill(int cid, double price, Mkt::Trade direction, Mkt::Side side, 
				 ECN::ECN ecn, int size, TimeVal fillTime, 
				 double tcost, double cbid, double cask) {

  assert(size >= 0);
  _fills[cid].AddFill(price, direction, side, ecn, size, fillTime, tcost, cbid, cask);
}


