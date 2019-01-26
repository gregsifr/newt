/*
  IntervalVolumeTracker.h

  Simple widget that tracks recent short-term (per stock) trading volume, per time-interval.
*/

#ifndef __INTERVALVOLUMETRACKER_H__
#define __INTERVALVOLUMETRACKER_H__

#include <vector>
using std::vector;

#include <cl-util/factory.h>
using namespace clite::util;

#include "DataManager.h"
#include "CircBuffer.h"
#include "ExchangeTracker.h"
#include "IntervalBar.h"

#include "c_util/Time.h"
using trc::compat::util::TimeVal;
/*
  Widget that tracker recent short-term trading volume, per-time interval, per stock.
  Notes:
  - Base class represents partial implementation:
    - Does not specify exactly where the volume data comes from or how it is collected.
  - Base class does not specify whether should report signed or unsigned volume.
*/
typedef CircBuffer<double> CBD;
class IntervalVolumeTracker : public TimeHandler::listener {
 protected:
  factory<DataManager>::pointer _dm;
  factory<debug_stream>::pointer _ddebug;             // For debug/error logging.
  int _sampleMilliSeconds;           // How freqently to sample mid-prices, in milli-seconds.
  int _numSamplePoints;              // Max number of sample points to keep (per stock).
  int _minSamplePoints;              // Min number of sample points to use for volatility estimation (per stock).
  int _sampleNumber;                 // Current sampling point (starts at 0).
  vector<CBD*> _volumeBufV;          // Holds actual underlying volume series.
  vector<double> _lastVolumeV;       // Holds trading volume, per stock, as of last sampling point.
  TimeVal _sampleStartTime;          // Time at which mid-price sampling started.
  bool _marketOpen;                  // Is market currently open for normal trading session.

  // Add a single time point sample (cross sectionally, aka for all stocks).
  virtual void addSample();
  // Populate _lastVolumeV with most recent interval volumes.
  virtual void populateVolumes() = 0;
  // Push values from _lastVolumeV into _volumeBufV;
  virtual void addVolumeSamples();
  // Flush volume samples from bhffer.
  virtual void flushVolumeSamples();

  virtual void onMarketOpen(const TimeUpdate &au);
  virtual void onMarketClose(const TimeUpdate &au);
  virtual void onTimerUpdate(const TimeUpdate &au);
 public:
  // 
  // Default parameters:
  // - sampleMilliSeconds 5000 : sample every 5 seconds.
  // - numSamplePoints     360 : keep trailing 30 minutes worth o data.
  // - minSamplePoints     120 : require at least 10 minutes of trailing data.
  //
  IntervalVolumeTracker();
  IntervalVolumeTracker(int sampleMilliSeconds, int numSamplePoints, int minSamplePoints);
  virtual ~IntervalVolumeTracker();

  virtual bool getVolumeAverage(int cid, double &fv);
  virtual bool getVolumeStdev(int cid, double &fv);
  inline int sampleMilliSeconds() {return _sampleMilliSeconds;}

  // Allows access to underlying buffer holding volume series.
  // Advanced swim only. 
  virtual const CBD *volumeBuffer(int cid) {return _volumeBufV[cid];}  

  virtual void update(const TimeUpdate &au);
};

/*
  Specific implementation of an IntervalVolumeTracker.
  This version:
  - Collects volume data off of directly reported (ECN) trades only.
  - It does not include SIAC/UTDF reported trades.
  - It thus does not capture non-displayed trades, e.g. block trades, but
    should be better at assigning trades to the time-bucket during which
    they actually took place than a volume tracker that works off of 
    TapeUpdates.
  - Should return *unsigned* volume, nopt signed volume.
*/
class IBVolumeTracker : public IntervalVolumeTracker {
 protected:
  IntervalBarTracker *_bart;      // Internal.  Uses for populating interval bar data.

  virtual void populateVolumes();
 public:
  IBVolumeTracker();
  IBVolumeTracker(int sampleMilliSeconds, int numSamplePoints, int minSamplePoints);
  virtual ~IBVolumeTracker();
};

class SignedIBVolumeTracker : public IBVolumeTracker {
 protected:
  virtual void populateVolumes();  
 public:
  SignedIBVolumeTracker();
  SignedIBVolumeTracker(int sampleMilliSeconds, int numSamplePoints, int minSamplePoints);   
  virtual ~SignedIBVolumeTracker();
};


#endif   // __INTERVALVOLUMETRACKER_H__
