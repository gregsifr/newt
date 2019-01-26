/**
 * Tracks the EMA of the queue sizes
 * It computes the decay factor given the sampling frequency and the
 * desired period over which an average should be computed
 * Specifying the length in minutes and sampling freq in msec 
 * the lambda can be computed as 2/(N+1) where N is the number
 * of period over which we intened to keep the average
 * It uses an initial warmup period of 30 observations to compute the starting
 * point
*/

#ifndef __QSZTRACKER_H__
#define __QSZTRACKER_H__

#include <cl-util/factory.h>
using namespace clite::util;

#include "DataManager.h"
#include "Markets.h"
#include "c_util/Time.h"
using trc::compat::util::TimeVal;

/**
 *  A simple trailing QSize tracking widget. 
 *  Tracks the EMA of the gloabal queue size, as well as the global queue size in each of the ECNs we listen to.
 */
class QSzTracker : public TimeHandler::listener, public WakeupHandler::listener {
 protected:
  factory<DataManager>::pointer _dm;    // Exists externally.

  vector<double>   _qSz;                   // Moving/decaying qsize (or simply sum of samples as long as not warmed)
  vector<double>   _ecnQSz[ECN::ECN_size]; // Moving/decaying qsize on each ("turned-on") ECN
  vector<ECN::ECN> _turnedOnEcns;          // Which Ecns are listened to by DM

  double         _lambda;
  int            _msec; // msec between samples
  TimeVal        _lastUpdateTV;
  bool           _mktOpen;       // Is market open or closed?
  bool           _warmed;   // True once enough samples are recorded
  int            _ct;       // count the first samples until there are enough to declare this is "warmed"

  void sampleAllStocks();

 public:
  QSzTracker();
  QSzTracker( int msec, int length );
  virtual void update( const TimeUpdate& t );
  virtual void update( const WakeUpdate& wu );
  bool qSz( int cid, double &qSz ) const;
  bool ecnQSz( int cid, ECN::ECN ecn, double &ecnQSz ) const;  
};

#endif    // __QSZTRACKER_H__
