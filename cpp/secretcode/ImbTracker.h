/**
 * A class to compute unconditional national imbalance and maintain running averages for it
 * Assumes a maximum depth of 10,000 shares and enforces a maximum deviation of 25bps
 * The code is optimized for computing the National unconditional imbalance
 * specifically it uses the fact that getMarket is o(1) operation and simplified 
 * aggregation equation for imbalance computation which is also o(1)
 *
*/

#ifndef __IMBTRACKER_H__
#define __IMBTRACKER_H__

#include "CircBuffer.h"
#include "DataManager.h"
#include "c_util/Time.h"
using trc::compat::util::TimeVal;

#include "StocksState.h"
#include "AlphaSignal.h"

#include <cl-util/table.h>
#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>
#include <cl-util/float_cmp.h>

using namespace clite::util;

class ImbTracker : public AlphaSignal, public WakeupHandler::listener, public TimeHandler::listener {
 protected:
  /*
    Internal state:
  */
  factory<DataManager>::pointer _dm;
  factory<debug_stream>::pointer _ddebug;
  factory<StocksState>::pointer       _stocksState;

  int _minpts;         // Minimum # of points for computing spread info.
  int _nperiods;       // Number of trailing periods over which to compute average spreads.
  int _msec;           // Length of period, in milliseconds.
  vector< CircBuffer<double> *> _buf;  // Exists internally.  Holds sampled aspd data points.
  TimeVal _lastUpdateTV;
  TimeVal _lastPrintTV;
  bool _mktOpen;       
  vector< vector<double> > _Fv;  // Precomputed CDFs per cid
  vector<double> imb_cache;
  vector<double> _klist;         // K-estimates per stock.

  // Add a point estimate of spread for specified stock.
  void addSampledImb(int cid, double fv);
  void sampleAllStocks();
  // Clear sampled values for all stocks.
  void clearAllStocks();
  // Precompute CDFs
  void computeImbalanceProbabilities(double k,vector<double> &Fv);

 public:
  ImbTracker();
  virtual ~ImbTracker();

  /*
    Summary calculations for trailing imbalance numbers.
   */
  // Get current trailing average spread. Requires at least _minpts data point to calculate.
  bool getAImb(int cid, double &fv);
  // Current excess Imbalance
  bool getDImb(int cid, double &fv);
  // Get both current and excess imbalance
  bool getCDImb(int cid, double &currImb, double &excessImb);

  /*
    Functions for calculating imbalance, potentially with some extra potential for
      scenario-analysis thrown in.
  */
  // Stated/known book version of imbalance calculation.
  bool calcImb(int cid,double &fvImb,double &truemid);
  // Simple scenario analysis version - Calculate imbalance, with known book + some specified
  //   extra shares.
  // Initial version of scenario analysis functionality allows caller to specify 1 clump
  //   of extra shares on bid size (level, starting with 0, plus number of shares) and
  //   1 clump on ask side.
  bool calcImb(int cid, int extraBidLvl, size_t extraBidShs, int extraAskLvl, size_t extraAskShs, double &fvImb, double &truemid);

  /*
    Functions that try to calculate imbalance, but use cached value if unable to do so.
  */
  bool getImb(int cid, double &fvImb);  // Try to calculate imbalance and if it fails use the cached one
  bool getImb(int cid, double &fvImb,double &truemid);  // Try to calculate imbalance and if it fails use the cached one
  // Current # of sampled points.  Only includes pts still in buffer, aka pts that have
  //   "fallen off" the end of the buffer are subtracted out from this total.
  int nPts(int cid);
  virtual void update( const TimeUpdate & t );
  virtual void update( const WakeUpdate& wu );
 
  // AlphaSignal interface functions.
  virtual bool getAlpha(int cid, double &alphaEst);

  // Public access to per-stock k-param. Intended for ImbTradingImpactModel.
  // Advanced swim only....
  double getK(int cid) {return _klist[cid];}
  
};


class KTracker {

public:

  typedef std::string key_type;  // the key-word "key_type" is used in clite::util::file_table
  /// returns the symbol this line refers to
  std::string const &get_key() const { return _symbol; }


  /// fields - those in the line dedicated to a specific symbol; fieldNames - the header line (ignored for now)
  KTracker( const vector<string>& fieldNames, const vector<string>& fields );
  /// for manual initialization
  KTracker( const string& symbol, double k );
  KTracker( const char*   symbol, double k );
  double getk() const { return _k;}

protected:

  std::string    _symbol;
  double _k;

};

#endif    // __IMBTRACKER_H__
