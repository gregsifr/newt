/*
  SpdTracker.h
  Contains logic for various types of spread calculation.  Used for stock
    market-making.
  Spread calc methods here replicate some model factors fitted in LRCF model
    generating code - and probably need to model coefficients from that code
    to be applied as intended.  Be careful changing spd calculation here unless
    also changing factor definitions (and recalculating betas) in LRCF model
    generation code.

*/

#ifndef __SPDTRACKER_H__
#define __SPDTRACKER_H__

#include <cl-util/factory.h>
using namespace clite::util;

#include "CircBuffer.h"
#include "DataManager.h"
#include "c_util/Time.h"
using trc::compat::util::TimeVal;

/*
  A simple trailing spread tracking widget.  Keeps data for specified trailing # of periods.
  Tries to sample every _msec millseconds.  _msec can be set to 0 to mean sample
    on every wakeup.
  Notes:  
  - Current implementation makes use with _msec = 0 expensive (sample all stocks every wakeup).
    May want to change so that keeps internal set of stocks for which it has seen a DataUpdate
    since last wakeup, and only re-samples those stocks.
  - Current implementation only tracks/reports spread information across all subscribed
    books.  probably should change to allow some access to CBBO spread, plus per book
    spreads.
*/
class SpdTracker : public TimeHandler::listener {
 protected:
  /*
    Internal state:
  */
  factory<DataManager>::pointer _dm;               // Exists externally.
  factory<debug_stream>::pointer _ddebug;          // Exists externally.
  int _minpts;         // Minimum # of points for computing spread info.
  int _nperiods;       // Number of trailing periods over which to compute average spreads.
  int _msec;           // Length of period, in milliseconds.
  vector< CircBuffer<double> *> _buf;  // Exists internally.  Holds sampled aspd data points.
  TimeVal _lastUpdateTV;
  TimeVal _lastPrintTV;
  bool _mktOpen;       // Is market open or closed?

  /*
    Protected member functions:
  */
  // Add a point estimate of spread for specified stock.
  void addSampledSpd(int cid, double fv);
  // For all stocks in _dm->populationSet:
  //   sample spread (getSpd).
  //   write to _buf (addSampledSpd).
  void sampleAllStocks();
  // Clear sampled values for all stocks.
  void clearAllStocks();
  // Dump some state info in human readable form.
  void printAllStocks();

  void processTimeUpdate(const TimeUpdate &au);

 public:
  // Default parameters:
  // - minpts = 50.
  // - nperiods = 60.
  // - msec = 1000.
  // Represents something that tracks spreads over relatively short timer horizon:
  //   last 1 minute of wall/sim time.
  SpdTracker();
  SpdTracker(int minpts, int nperiods, int msec);
  virtual ~SpdTracker();

  // Get current trailing average spread. Requires at least _minpts data point to calculate.
  bool getASpd(int cid, double &fv);
  // Current spread - trailing average.
  bool getDSpd(int cid, double &fv);
  // Current inside of market spread.  Exact data sources (and whather includes/excludes odd lots)
  //   should be defined by how DataManager/DataCache are configured.
  // Currently defined to return true only if it can find valid CBBO as of current wakeup.  Otherwise,
  //   returns false.
  bool getSpd(int cid, double &fvSpd);
  // Get current market - pass through to DataCache.
  bool getMkt(int cid, double &bid, double &ask);

  // Current # of sampled points.  Only includes pts still in buffer, aka pts that have
  //   "fallen off" the end of the buffer are subtracted out from this total.
  int nPts(int cid);

  /*
    UpdateListener....
   */
  virtual void update(const TimeUpdate &au);
};

#endif    // __SPDTRACKER_H__
