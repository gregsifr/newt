#include "WakeupStats.h"
using trc::compat::util::DateTime;

#define MILLION (1000*1000)

WakeupStats::WakeupStats()
  : _wakeupNumber( 0 ),
    _lastWakeupTV( 0 ),
    _maxDeltaUSec( 0 ),
    _logPrinter( factory<debug_stream>::get(std::string("wakeups")) )
{
  factory<DataManager>::pointer dm = factory<DataManager>::find(only::one);
  if( !dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in WakeupStats::WakeupStats)" );
  //  dm -> add_listener_back( this );
  dm -> add_listener( this );
  printFieldNames();
}

void WakeupStats::update( const WakeUpdate& wu ) {
  int deltaUSec = (wu.tv.usec() - _lastWakeupTV.usec()) + MILLION * ( wu.tv.sec() - _lastWakeupTV.sec() );
  if( deltaUSec > _maxDeltaUSec )
    _maxDeltaUSec = deltaUSec;
  _lastWakeupTV = wu.tv;

  // Prints stats every 1000 wakeups
  _wakeupNumber++; 
  if( _wakeupNumber % 1000 == 0 )
    flushStats( wu.tv );
}

void WakeupStats::flushStats( TimeVal curtv ) {
  DateTime curDT( curtv );
  TAEL_PRINTF(_logPrinter.get(), TAEL_INFO, "%02d:%02d:%02d.%06d %12d %12d",
		       curDT.hh(), curDT.mm(), curDT.ss(), curDT.usec(), _wakeupNumber, _maxDeltaUSec ); 
  _maxDeltaUSec = 0;
}

void WakeupStats::printFieldNames() const {
  TAEL_PRINTF(_logPrinter.get(), TAEL_INFO, "%15s %12s %12s", "curTV", "#wakeup", "Max_time_between_wakeups_in_latest_bunch" );
}
