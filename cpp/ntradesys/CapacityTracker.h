#ifndef _CAPACITYTRACKER_H_
#define _CAPACITYTRACKER_H_

#include "DataManager.h"
#include "DataUpdates.h"
#include "HistVolumeTracker.h"
#include <vector>
#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>
#include <cl-util/float_cmp.h>

#define MAXPERIODS 4

using namespace clite::util;

class CapacityTracker : 
  public TimeHandler::listener,
  public WakeupHandler::listener, 
  public MarketHandler::listener, 
  public OrderHandler::listener  {

 protected:
  factory<DataManager>::pointer       _dm;
  factory<debug_stream>::pointer      _logPrinter;
  factory<HistVolumeTracker>::pointer _hvt;

  static const int    _periods[MAXPERIODS];  // periods in seconds
  static const double _lambda[MAXPERIODS];   // Discount factor (use 2/(N+1) to rounghly keep last N observations)
  static const double _fraction[MAXPERIODS]; // Participation rate

  vector<double>          _accVol;    // Running accumulated volume of each symbol since the program started
  vector<vector<double> > _lastV;     // Volume traded per symbol as of last reset per period
  vector<vector<double> > _estimate;  // Estimate of the average volume per symbol/period (based mostly on live recent volume data)
  vector<vector<double> > _avgOurV;
  vector<int>             _ourAccVol; // Running accumulated volume of OUR trades
  vector<vector<int> >    _lastOurv;  // Volume filled per symbol as of last reset per period
  vector<int> _laststate;
  bool _mktOpen;
  int  _minCap;
  double _participationRate;
  int  _lastTick;       // The last second in which we reevaluated the moving averages

  static int printCount;
  
 public:
  CapacityTracker();
  virtual void update( const TimeUpdate&  t  );
  virtual void update( const WakeUpdate&  wu );
  virtual void update( const OrderUpdate& ou );
  virtual void update( const DataUpdate&  du );
  bool capacity( int cid );
  void set( int mincap ) { _minCap = mincap; }
  void setpcr( double pcr) {_participationRate = pcr;}
};

#endif
