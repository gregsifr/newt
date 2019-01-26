/*
  Simple code for collecting high-frequency interval bar data.
*/

#ifndef __INTERVALBAR_H__
#define __INTERVALBAR_H__


#include <vector>
using std::vector;

#include <cl-util/factory.h>
using namespace clite::util;

#include "DataManager.h"
#include "AlphaSignal.h"
#include "ExchangeTracker.h"

/*
  Holds open, high, low, close, and vwap data for some interval.
  vwap is defined as being computed off of trades.
  open, high, low, close, can be computed off of trades, quoted mid prices, etc..
*/
class IntervalBar {
 protected:
  int _cid;
  int _intervalNumber;
  double _open;
  double _close;
  double _high;
  double _low;
  int _numSamples;
  double _sumVol;     // Sum of (unsigned volume).
  double _sumVolPx;   // Sum of (unsigned volume * price).
  double _sumBidVol;  // Sum of volume that occurred with bid side providing liquidity.
  double _sumAskVol;  // Sum of volume that occurred with ask side providing liquidity.

 public:
  IntervalBar();
  IntervalBar(int cid, int intervalNumber);
  // Initialize with single sample point with zero volume.
  IntervalBar(int cid, int intervalNumber, double bid, double ask);
  virtual ~IntervalBar();

  int cid() const {return _cid;}
  int intervalNumber() const {return _intervalNumber;}

  bool open(double &fv) const;
  bool close(double &fv) const;
  bool high(double &fv) const;
  bool low(double &fv) const;
  bool vwap(double &fv) const;
  double volume() const {return _sumVol;}
  double bidVolume() const {return _sumBidVol;}
  double askVolume() const {return _sumAskVol;}
  double signedVolume() const {return _sumAskVol - _sumBidVol;}

  /*
    Use vol == 0 to represent e.g. a quote, and vol > 0 to represent a trade.
  */
  void addSample(Mkt::Side side, double px, double vol);

  int snprint(char *s, int n) const;
};


/*
  Interface for IntervalBarTracker.
  - Tracks IntervalBara data for population of stocks.
  - Allows querying for whether a new bar of data has just been populated, and
    is ready to be extracted.
*/
class IntervalBarTracker {
 public:
  IntervalBarTracker();
  virtual ~IntervalBarTracker();
  /*
    Has a bar worth of data just been completed.  
    Should return true and populate fv exactly once per bar worth of data.
    Should cache that bar worth of state until someone reads it, or until
      the next bar is ready.
    Reads of newly finished bars do not need to occur directly after completion.
    Aka, the following ordering of events should be supported:
    - data packets....
    - end of bar N (timer update).
    - 0....N additional data packets....
    - external read of bar N (should get data written as of end of bar N) 
    
  */
  virtual bool barFinished(int cid, IntervalBar &fv) = 0;
};

/*
  IntervalBarTracker which calculates data off of *trades*, aka:
  - open, high, low, close should hold 1st, high, low, and last *trading prices*
    in the specified interval.
*/
class TradeIntervalBarTracker : public IntervalBarTracker, public TimeHandler::listener, public MarketHandler::listener {
  factory<DataManager>::pointer _dm;

  // Sequence number of current interval.  Numbered 0.....
  int _intervalNumber;

  // Length of each interval, in *milliseconds*. 
  int _intervalMSec; 

  // TimeVal at which 1st interval started.
  TimeVal _startTV;

  // Is market currently open for trading?
  bool _marketOpen;

  vector<IntervalBar> _currentBars;
  vector<IntervalBar> _mostRecentBars;
  vector<int> _mrbStatus;   // 0 = not populated.  1 = populated but not read.  2 = already read.

  void onMarketOpen(const TimeUpdate &au);
  void onMarketClose(const TimeUpdate &au);
  void onTimerUpdate(const TimeUpdate &au);
  void makeNewIntervalSet();
 public:
  TradeIntervalBarTracker(int intervalMSec);
  virtual ~TradeIntervalBarTracker();

  /*  
      IntervalBarTracker public interface functions.
  */
  virtual bool barFinished(int cid, IntervalBar &fv);

  /*
    UpdateListener functions.
  */
  virtual void update(const DataUpdate &du);
  virtual void update(const TimeUpdate &au);
};


#endif  // #define __INTERVALBAR_H__
