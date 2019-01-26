// A trader that trades a given set of symbols along a single day.
// It gets a series of time points along the day.
// For each time-point and each symbol, it gets a target position. It should then start trading towards this target, until the 
//  next time-point or until market closes.

#ifndef __TIMESLICESTRADER_H__
#define __TIMESLICESTRADER_H__

#include <vector>
#include <fstream>

using std::ofstream;

#include "DataUpdates.h"
#include "ExecutionEngine.h"
#include "DataManager.h"
#include "StockInfo.h"
#include "StocksState.h"
#include "ExternTradeRequest.h"
#include "FollowLeaderSOB.h"

#include <cl-util/table.h>
#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>

using namespace clite::util;

class TimeSlicesTrader : public TimeHandler::listener,
			 public WakeupHandler::listener, 
			 public OrderHandler::listener {
  
public:
  TimeSlicesTrader( ExecutionEngine& execEngine, 
		    DataManager& dm, 
		    string tradeRequestsFilename, string summaryFilename, 
		    string logFilename, string mattFilename,
		    char delimiter, int printLevel, double priority );
  virtual ~TimeSlicesTrader();

  /////////////////////////////////
  // Virtual methods of the base-class "UpdateListener"
  /////////////////////////////////
  virtual void update( const TimeUpdate &tu );
  virtual void update( const OrderUpdate &ou );
  virtual void update ( const WakeUpdate &wu);

  ////////////////////////////////
  // Main entry point for class
  ////////////////////////////////
  static int main( int argc, char **argv );

protected:
  ////////////////////////////////
  // Methods of updating 
  ////////////////////////////////
  void updateClosePrices(); // update the closing prices of all names (should be ran every day in simulation)

  ////////////////////////////////
  // Methods of Input
  ////////////////////////////////
  // read the trade-requests
  void readTradeRequests( const string& tradeRequestsFilename );

  ////////////////////////////////
  // Methods of Printing to the summary file
  ////////////////////////////////
  void printSummary();
  void printMattSummaryInfo();
  void printPNLTracker();

  ////////////////////////////////
  // Internal State
  ////////////////////////////////
  ExecutionEngine&  _ee;
  DataManager&  _dm;
  factory<StocksState>::pointer _stocksState;

  vector<StockInfo*>        _stocksInfo;    // per-stock info
  vector<ExternTradeRequest*> _tradeRequests; // individual trade-requests
  double _priority;
  // market status
  bool _marketIsOpen;   
  //////////////////////////////////
  // for print-outs
  //////////////////////////////////
  const  string     _summaryFilename;
  char              _delimiter[2]; // The delimiter used for the summary file and open-vwap file
  factory<debug_stream>::pointer   _logPrinter;   // For handling the log-file print-outs
  tael::Logger   _MattPrinter;  // For handling Matt's print-outs
};

inline int secondsSinceMidnight( const TimeVal &tv ) {
  DateTime dt(tv);
  return 3600 * dt.hh() + 60 * dt.mm() + dt.ss();
}

inline int hhmmssToSecs( int hhmmss ) {
  int hours = hhmmss / 10000;
  int minutes = hhmmss % 10000 / 100;
  int seconds = hhmmss % 100;
  return hours*3600 + minutes*60 + seconds;
}

inline int secsToHhmmss( int secs ) {
  int hours = secs / 3600;
  int minutes = secs % 3600 / 60;
  int seconds = secs % 60;
  return hours*10000 + minutes*100 + seconds;
}

// convert a string of the format "HHMMSS.mmm" (mmm = milliseconds) to the corresponding TimeVal (in the given date)
inline TimeVal HHMMSS_mmmToTimeVal( char* hhmmss_mmm, int yyyymmdd ) {
  TimeVal midnight = DateTime::getMidnight( yyyymmdd );
  long secsTillLastMidnight = midnight.sec();
  char *hhStr   = strtok( hhmmss_mmm, ":" );
  char *mmStr   = strtok( NULL, ":" );
  char *ssStr   = strtok( NULL, "." );
  char *msecStr = strtok( NULL, "." );
  int hh   = atoi( hhStr );
  int mm   = atoi( mmStr );
  int ss   = atoi( ssStr );
  int msec = atoi( msecStr );
  long secs = secsTillLastMidnight + hh*3600 + mm*60 + ss;
  long usecs = 1000 * msec;
  return TimeVal( secs, usecs );
}  

#endif // __TIMESLICESTRADER_H__
