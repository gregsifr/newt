/*
  TimeAverage.h

  Widget for keeping track of moments of a distribution, across a fixed
    trailing time period, when the individual samples are spaced irregularly in time.
*/


#ifndef __TIMEAVERAGE_H__
#define __TIMEAVERAGE_H__


#include <cl-util/factory.h>
using namespace clite::util;

#include "DataManager.h"

#include <vector>
using std::vector;

/*
  Holds distribution values for 1 time slice.
  - Assumes that samples are added but not subtracted.
  - Initial version contains logic for computing mean and sd only, not for
    computing higher moments of distribution.
*/
class TimeBucket {
  int _minSamples;     // Min # of sample points for valid distribution moment values.
  int _nSamples;
  double _sum;
  double _sumsq;
 public:
  TimeBucket(int minSamples);
  virtual ~TimeBucket();

  int nSamples() const;
  bool mean(double &fv) const;
  bool sd(double &fv) const;

  double sum() const;
  double sumsq() const;
  
  void addSample(double val);
  void clear();
};


/*
  Keeps track of moments of time-series distribution, where data points arrive at
    different iem-increments, and where the desired moments are calculated over 
    all data points occuring in a trailing *time* window,
*/
class TimeBucketSeries {
 protected:
  int _nPeriods;                 // Number of periods to look back.
  int _minPeriods;               // Minimum # of periods for valid sample.
  int _currentPeriod;            // Number of current period.  1st period of data in idx 0.
  vector<TimeBucket> _buckets;  // Set of buckets, 1 per period.
 public:
  TimeBucketSeries(int nPeriods, int minPeriods, int minSamplesPerPeriod);
  virtual ~TimeBucketSeries();

  // Add another time-series bucket.
  // Should be called before any samples are added.
  void addBucket();

  // Add another data element to current bucket.
  void addSample(double val);

  int nSamples() const;
  bool mean(double &fv) const;
  bool sd(double &fv) const;

  double sum() const;
  double sumsq() const;
};

/*
  Holds N TimeBucket series (1 per stock), and allows them to be populated (by an external object).
  Listens to time updates so knows when to add new time buckets.
*/
class TimeBucketSeriesTracker : public TimeHandler::listener {
  int _bucketLength;                                  // Length of time interval, in seconds.
  int _numBuckets;                                    // Number of buckets per series.
  factory<DataManager>::pointer     _dm;              // Connection to underlying HF data.
  vector<TimeBucketSeries>          _payload;         // 1 per stock.
  TimeVal _startTV;                                   // TimeVal at which 1st interval started.
  bool    _marketOpen;                                // Is market currently open for trading?
  int     _intervalNumber;                            // Sample number of current interval.

  virtual void addBucket();
 public:
  // Length of time interval, in seconds, # of intervals to lookback, min # of buckets populated
  //   before a sample can be taken.
  TimeBucketSeriesTracker(int bucketLength, int numBuckets, int minBucketsWithSamples);
  virtual ~TimeBucketSeriesTracker();

  virtual void addSample(int cid, double val);
  virtual bool mean(int cid, double &fv) const;
  virtual bool sd(int cid, double &fv) const;
  virtual int nSamples(int cid);

  virtual void update (const TimeUpdate &au);
  
};

#endif   // __TIMEAVERAGE_H__

