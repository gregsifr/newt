/*
  Fills.cc - Code for fill/execution tracking.
  Used for transaction cost analysis and PNL tracking.
*/

#include "Fills.h"
#include "RUtils.h"

using trc::compat::util::DateTime;

/*******************************************************************
  Exec code:
*******************************************************************/
Exec::Exec() :
  _price(-1.0),
  _size(-1),
  _dir(Mkt::BUY),
  _side(Mkt::BID),
  _ecn(ECN::UNKN),
  _fillTV(0),
  _orderID(-1),
  _tc(0.0),
  _cbid(0.0),
  _cask(0.0)
{

}

Exec::Exec(const OrderUpdate &ou, double bid, double ask) :
  _price(ou.thisPrice()), 
  _size(ou.thisShares()), 
  _dir(ou.dir()), 
  _side(ou.side()),
  _ecn(ou.ecn()), 
  _fillTV(ou.tv()),
  _orderID(ou.id()), 
  _tc(-1 * ou.thisFee()), 
  _cbid(bid), 
  _cask(ask)
{
  
}

Exec::Exec( double price, int size, Mkt::Trade dir, Mkt::Side side, ECN::ECN ecn, const TimeVal &fillTV,
	    double tc, double cbid, double cask) :
  _price(price),
  _size(size),
  _dir(dir),
  _side(side),
  _ecn(ecn),
  _fillTV(fillTV),
  _orderID(-1),
  _tc(tc),
  _cbid(cbid),
  _cask(cask)
{
}

int Exec::snprint(char *buf, int n) const {
  DateTime dt(_fillTV);
  return snprintf(buf, n, "FILL %02d:%02d:%02d:%06d  Px%.2f  SZ%i  SIDE%s  ECN%s  CBBID%.2f  CBASK  %.2f  TC%.4f  ORDID%i",
		  dt.hh(), dt.mm(), dt.ss(), dt.usec(),
		  _price, _size, Mkt::SideDesc[_side], ECN::desc(_ecn),
		  _cbid, _cask, _tc, _orderID);  
}

/*******************************************************************
  Fills code
*******************************************************************/

Fills::Fills() :
  fillList(0)
{

}

Fills::~Fills() {
  Clear();
}

/*
  From an actual fill.
*/
void Fills::AddFill(const OrderUpdate &ou,
		    double cbid, double cask) {
  Exec e(ou, cbid, cask);
  fillList.push_back(e);
}

/*
  Synthetic fill, e.g. from specififcation of BOD or EOD position.
*/
void Fills::AddFill(double price, Mkt::Trade dir, Mkt::Side side, 
		    ECN::ECN ecn, int size, const TimeVal &tv,
		    double tcost, double cbid, double cask) {
  Exec e(price, size, dir, side, ecn, tv, tcost, cbid, cask);
  fillList.push_back(e);
}


void Fills::Clear() {
  fillList.clear();
}

void Fills::fillPriceVector(vector<double> &pv) {
  pv.clear();
  vector<Exec>::iterator it;
  for (it = fillList.begin(); it != fillList.end(); ++it) {
    pv.push_back(it->_price);
  }
}

void Fills::fillSizeVector(vector<int> &sv) {
  sv.clear();
  vector<Exec>::iterator it;
  for (it = fillList.begin() ; it != fillList.end(); ++it) {
    sv.push_back(it->_size);
  }
}

//  Directional, aka 1 SHS BUY + 1 SHS sell ==> 0 shs.
int Fills::totalSharesFilled() {
  int ret = 0;
  vector<Exec>::iterator it;
  for (it = fillList.begin(); it != fillList.end(); ++it) {
    if (it->_side == Mkt::BID) {
      ret += it->_size;
    } else {
      ret -= it->_size;
    }
  }
  return ret;
}

int Fills::absoluteSharesFilled() {
  int ret = 0;
  vector<Exec>::iterator it;
  for (it = fillList.begin(); it != fillList.end(); ++it) {
    ret += it->_size;
  }
  return ret;
}

//  Directional, aka 1 SHS BUY + 1 SHS sell ==> 0 shs.
double Fills::totalDollarsFilled() {
  double ret = 0;
  vector<Exec>::iterator it;
  for (it = fillList.begin(); it != fillList.end(); ++it) {
    if (it->_side == Mkt::BID) {
      ret += it->_size * it->_price;
    } else {
      ret -= it->_size * it->_price;
    }
  }
  return ret;
}

double Fills::absoluteDollarsFilled() {
  double ret = 0;
  vector<Exec>::iterator it;
  for (it = fillList.begin(); it != fillList.end(); ++it) {
    ret += it->_size * it->_price;
  }
  return ret;
}


double Fills::sAvgFillPx() {
  vector<double> fillPrices;
  fillPriceVector(fillPrices);
  double meanP = RVectorNumeric::mean(fillPrices);
  return meanP;
}

double Fills::wAvgFillPx() {
  vector<double> fillPrices;
  vector<int> fillSizes;
  vector<double> fillSizesD;
  fillPriceVector(fillPrices);
  fillSizeVector(fillSizes);
  RVector::copy(fillSizes, fillSizesD);
  double meanP = RVectorNumeric::wmean(fillPrices, fillSizesD);
  return meanP;
}

void Fills::fillFillTimeVector(vector<TimeVal> &tv) {
  tv.clear();
  vector<Exec>::iterator it;
  for (it = fillList.begin(); it != fillList.end(); ++it) {
    tv.push_back(it->_fillTV);
  }
}

bool Fills::lastFillTime(TimeVal &tv) {
  int sz = fillList.size();
  if (sz <= 0) {
    return false;
  }
  Exec e = fillList.at(sz-1);
  tv = e._fillTV;
  return true;
}


// Compute the change in cash that has occurred over the course of the day.
// If any positions remain, they are assumed to be liqudiated for prcClose.
// By default, results from this call include transaction costs.
// If strategy entered day with 0 position, or if a fake fill was inserted 
//   to represent the initial BOD position, then this # should also
//   represent intra-day PNL in the stock.
double Fills::computeChangeCash(double prcClose, bool includeTC) {
  double ret = 0.0;
  int cumpos = 0;

  vector<Exec>::iterator it;
  for (it = fillList.begin(); it != fillList.end(); ++it) {
    // Going long consumes cash.
    // Selling or going short generates cash.
    // Transaction costs are specified as positive # ==> made $ ==> good.
    double tamt = (it->_price * it->_size);
    if (it->_dir == Mkt::BUY) {
      ret -= tamt;
      cumpos += it->_size;
    } else {
      ret += tamt;
      cumpos -= it->_size;
    }
    if (includeTC == true) {
      ret += it->_tc;
    }
  }

  // Assume that any remaining cumulative position in stock can be liquidated
  //   for prcClose per share....
  ret += (cumpos * prcClose);
  return ret;
}

vector<Exec>::iterator Fills::begin() {
  return fillList.begin();
}

vector<Exec>::iterator Fills::end() {
  return fillList.end();
}
