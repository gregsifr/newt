#ifndef _WAKEUPSTATS_H_
#define _WAKEUPSTATS_H_

#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>
using namespace clite::util;

#include "DataManager.h"

/*
 *  WakeupStats: a simple wakeup-listener that occasionally prints statisitcs about wakeups
 */
class WakeupStats : public WakeupHandler::listener {
protected:
  int     _wakeupNumber;
  TimeVal _lastWakeupTV;
  int     _maxDeltaUSec; // the maximal delta between wakeups, in microsecs
  factory<debug_stream>::pointer _logPrinter;

  void update( const WakeUpdate& wu );
  void flushStats( TimeVal curtv ); /// print stats and initialize member variables if needed
  void printFieldNames() const;

public:  
  WakeupStats();
};

# endif // _WAKEUPSTATS_H_
