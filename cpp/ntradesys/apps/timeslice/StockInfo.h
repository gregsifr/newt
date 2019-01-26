#ifndef _STOCKINFO_H_
#define _STOCKINFO_H_

#include <string>
#include <iostream>
using std::string;

#include "c_util/Time.h"
using trc::compat::util::TimeVal;

#include "DataManager.h"
#include "Markets.h"  // for the namespace "Cmp", and possibly for other usages as well

#ifndef ABS 
#define ABS(x) ( (x)>0 ? (x) : -1*(x) )
#endif

#ifndef MAX
#define MAX(x,y) ( (x)>(y) ? (x) : (y) )
#endif

#ifndef MIN
#define MIN(x,y) ( (x)<(y) ? (x) : (y) )
#endif

namespace quan { // parameters for the program that Quan is supposed to use
  //  extern const char*  NA_STRING;// = "N.A."; // this is how NAs (should) look like in the summary file
  const char*  const NA_STRING = "N.A."; // this is how NAs (should) look like in the summary file
  const double ERR_PX    = -1.0;  // This is used mainly to describe invalid bid/ask, or unavailable VWAP

  enum MktStatus {
    NORMAL,    // data is accessible and spread is positive
    LOCKED,    // data is accessible but spread shows as zero
    CROSSED,   // data is accessible but spread shows as negative
    NODATA,    // data is not accessible
    UNKNOWN    // to be used only in initialization
  };
  const char *const MktStatusDesc[] = {
    "Normal ", "Locked ", "Crossed", "NoData ", "Unknown" };

}

// A Stock-Info: a set of stock-specific information fields
class StockInfo {
public:
  // constructor
  // logPrinter: used to print out warnings / error messages
  StockInfo( DataManager &dm, unsigned int cid, tael::Logger& logPrinter );

  ////////////////////////////////////////////////////////////////////////////////////////////
  // updating 
  ////////////////////////////////////////////////////////////////////////////////////////////

  void updateMarketVars();   // update the variables referring to market info (market status / best bid/ask etc.)
  void updateClosePx() { _closePx = getClosePrice(); }  // updates _closePx (works only for historical days)

  ////////////////////////////////////////////////////////////////////////////////////////////
  //  Get general info
  ////////////////////////////////////////////////////////////////////////////////////////////
  unsigned int cid()    const { return _cid; }
  const char*  symbol() const { return _symbol; }

  ////////////////////////////////////////////////////////////////////////////////////////////
  //  Get spreads, prices, mid-prices etc.
  ////////////////////////////////////////////////////////////////////////////////////////////
  
  inline double spread() const { return _askPx-_bidPx; }
  inline double midPx() const { return (_askPx + _bidPx) / 2; }
  inline const char*  getMktStatusDesc() const { return quan::MktStatusDesc[_mktStatus]; }
  inline bool   symbolTraded() const { return _tradedToday; }

private:
  ////////////////
  // MEMBERS
  ////////////////

  // The DataManager instance that's listening to data associated with this name
  DataManager& _dm;

  // The cid and name of this ticker
  const unsigned int _cid;
  const char*        _symbol;

  // the closing price of the day (another thing that works only in sim-mode, gets updated upon market open)
  double _closePx;

  // Market book and status
  quan::MktStatus _mktStatus; // normal / locked / crossed / no-data / unknown
  bool            _tradedToday;    // set to true once there is some market data coming
  double          _bidPx, _askPx;
  int             _bidSz, _askSz;  // These are the PASSIVE values, i.e. excluding our open orders

  // Get the closing price via Bloomberg
  double getClosePrice() const;

  // errors/warnings printer (into a log file)
  tael::Logger& _logPrinter;

};


inline const char* getCurrTimeString( DataManager& dm ) {  
  return DateTime(dm.curtv()).gettimestring(); 
}

inline const string getHhmmssString( DataManager& dm ) {
  string ret( getCurrTimeString(dm) );
  ret.resize( 8 );
  return ret;
}

#endif
