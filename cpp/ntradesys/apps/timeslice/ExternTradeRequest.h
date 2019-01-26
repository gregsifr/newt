// The class ExitTradeRequest contains all the relevant information about a trade request in Steve Anderson and McQuinn's format.

#ifndef _EXIT_TRADE_REQUEST_H_
#define _EXIT_TRADE_REQUEST_H_

#include <vector>

#include "DataManager.h"
#include "ExecutionEngine.h"
#include "StockInfo.h"


#include <cl-util/table.h>
#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>

using namespace clite::util;

// Some basic data of a fill
class ExternTradeFill {
public:
  ExternTradeFill( int signedSize_, double px_, TimeVal time_, double fees_ ):
    signedSize(signedSize_), px(px_), time(time_), fees(fees_) {}
  int     signedSize;
  double  px;
  TimeVal time;
  double  fees;  // fees we pay (i.e., positive fees = bad for us)
};

class ExternTradeRequest {
public:
  // constructor
  // For the symbol specified in 'si', we should try trading to curr_position + 'deltaTarget' at 'startTrade'. 
  // At 'deadline' we should stop trading even if we're not yet in our target.
  ExternTradeRequest( ExecutionEngine& ee, 
		    StockInfo& si, tael::Logger& logPrinter,
		    TimeVal startTrade, TimeVal deadline, int deltaTarget, double priority );
  virtual ~ExternTradeRequest();
  
  ////////////////////////////////////////////////////////////////////////////////
  // General Data
  ///////////////////////////////////////////////////////////////////////////////
  inline unsigned int getCid() const { return _si.cid(); }
  inline int getDeltaTarget() const { return _deltaTarget; }
  
  ///////////////////////////////////////////////////////////////////////////////
  // Wakeup in order to start/stop trading
  ///////////////////////////////////////////////////////////////////////////////
  // used mainly as a "timer", in which I check whether it's time to start-trading / stop-trading
  void wakeup();
  // returns true if this trade-request was sent to the execution-engine, and still hasn't been filled/stopped
  bool isActive() const { return _tradingBegan && !_reachedTarget && !_deadlinePassed; }

  //////////////////////////////////////////////////////////////////////////////
  // Update the fills
  //////////////////////////////////////////////////////////////////////////////
  // ideally, should be an Order-Update-listener of the particular symbol, a listener which is active only
  // while the request is outstanding. (But this is still not available, so we leave this job to the main class (TimeSlicesTrader)
  void processAFill( const OrderUpdate& ou );

  //////////////////////////////////////////////////////////////////////////////
  // Get transaction costs and fill-rate
  //////////////////////////////////////////////////////////////////////////////
  // returns the total number of fills
  int           getTotalSignedFills() const;
  // returns a number in [0,1]
  inline double getFillRate() const {
    return (float)getTotalSignedFills() / _deltaTarget; }
  // have we reached target?
  inline bool   isTargetReached() const { return _reachedTarget; }
  // returns the TC of the fills: the average net price we paid minus the initial mid-price (this is a per-share number)
  double        getNominalTCofFills() const;
  // the average-time-to-fill (is secs) of the fills (returns 0 if there are no fills)
  double        getAvgTimeToFill() const;

  //////////////////////////////////////////////////////////////////////////////
  // Get summary
  //////////////////////////////////////////////////////////////////////////////
  static int snprintSummaryHeader( char* buf, int n, const char* delimiter );
  int  snprintSummary( char* buf, int n, const char* delimiter ) const;

private:

  // All the things it should record on start-trading / stop-trading
  void  onStartTrading();
  void  onReachingTarget();
  void  onDeadline();

  // The DataManager instance that's listening to data associated with this name
  factory<DataManager>::pointer _dm;

  // (A copy of) The execution engine that we use
  ExecutionEngine& _ee;

  // The StockInfo of the symbol to be traded
  StockInfo&   _si;

  // The trade-request time and size
  const TimeVal  _startTradeTime;
  const TimeVal  _deadline;
  const int      _deltaTarget;

  // Flags to mark that trading already began / reached target / deadline passed
  bool  _tradingBegan;
  bool  _reachedTarget;
  bool  _deadlinePassed;
  
  // Tracking variables:
  double                _midPxAtStart;    // The mid price when the trade-requested is 'sent' to the execution engine
  double                _midPxAtDeadline; // The mid price at deadline
  vector<ExternTradeFill*> _fills;              // Fills that are recieved while this request is active and in the matching direction
  
  // errors/warnings printer (into a log file)
  tael::Logger& _logPrinter;
  
  double _priority;

};




#endif
