/*
  AlphaSignalDumper.h

  Simple DataManager/UpdateListener based widgets + program for testing 
    *unconditional* alpha signals.
  Operates on a single fixed (pre-specified) predictive horizion.
  Operates on 1...N alpha signals.
  Generates big log holding:
  - Log time.
  - Ticker.
  - Obs Time.
  - 1...N signals values.
  - Actual return over time horizon.
*/

#ifndef __ALPHDUMPOOBSERVATION_H__
#define __ALPHDUMPOOBSERVATION_H__

#include <vector>
using std::vector;

#include <string>
using std::string;

#include "c_util/Time.h"
using trc::compat::util::TimeVal;

/*
  Single observation:  stock info + actual return + 0....N signal predicted returns.
*/
class AlphaDumpObservation {
 public:
  int _cid;
  string _ticker;
  TimeVal _startTV;               // Time of signal observation (aka beginning of period).
  int    _periodMS;               // Length of period, in milliseconds.
  vector<double> _signalValues;   // Predicted return from signals 1...N.
  double _ret;                    // Actual return over period (from beginning --> end of period).
  double _bidPx;                  // Bid px at beginning of period.
  double _askPx;                  // Ask px at beginning of period.
  
  AlphaDumpObservation(int cid, const char *ticker, TimeVal &startTV, int periodMS, 
		       int nsignals, double bidPx, double askPx);
  virtual ~AlphaDumpObservation();

  void associateSignals(vector<double> &signalValues);
  void associateReturn(double curMid);

  bool calculateReturn(int cid, DataManager *dm, double &fv);
  int snprint(char *buf, int n) const;
  static int snprint_desc(char *buf, int n);  
};


/*
  Simple UpdateListener based widget for testing out (unconditional) alpha signals:
  - For specified population set:
  - Periodically samples signal.
  - Associates with subsequent returns.
  - At end of trading day, dumps records into specified output file.

  Contains some performanace optimizations:
  - Only tries to associate observations & returns on wakeups.
  - Uses debug printer for output, so that output can be 
    logged asynchronously.

  Design Gestalt:
  - For each stock in population set:
    - Keeps queue of live observations, aka those that have not yeet been associated
      with returns over all desired time horizons.
  - On wakeups:
    - Walks through live observations, trying to associate with future period
      returns.
    - When it finds a live observation that has just been associated with the longest
      period desired return:
      - Pops from queue.
      - Writes to putput file.
*/

class AlphaDumpTester : public AdminHandler::listener {
 protected:
  factory<DataManager>::pointer _dm;        // Wrapper around underlying HF infra.
  vector <AlphaSignal*> _signals;           // Ordered set of signals to test.
  tael::Logger &_dlog;                   // Logger for output.
  vector<AlphaDumpObservation> _libeObservations;  // 1 per stock.
  TimeVal _startTV;                         // TimeVal of 1st sample point.
  int _sampleNum;
  bool _marketOpen;                         // Is market currently open for trading?
  bool _exitOnClose;                        // Should program auotmatically exit when it sees a mkt close message?
  int _firstSampleIndex;                    // Index of 1st stock in population set to start sampling.  Default = 0
  int _sampleMilliSeconds;                  // How frequently to sample, in milliseconds. 

  // Write header/format-info to dlog.
  void logHeader();
  // Write specified record (for single fully associated AlphaSignalObservation) to dlog.
  void logObservation(const AlphaSignalObservation &aso);
  // Make new (unassociated) ASO for each stock and all to _liveObservations.
  void sampleAllStocks();
  // Called on timer update.  Does actual work & sampling signal & associating with future returns.
  void processTimerUpdate(const AdminUpdate &au);  

 public:
  AlphaSignalDumper(vector<AlphaSignal*> signals, tael::Logger &dlog, int firstSampleIndex,
		    int sampleMIlliSeconds, bool exitOnClose);
  virtual ~AlphaSignalDumper();

  /*
    GroupUpdate functions.
  */
  virtual void update(const AdminUpdate &au);  
}

#endif  // __ALPHDUMPOOBSERVATION_H__
