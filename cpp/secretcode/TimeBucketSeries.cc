/*
  TimeAverage.cc
  Widget for keeping track of moments of a distribution, across a fixed
    trailing time period, when the individual samples are spaced irregularly in time.
*/

#include "TimeBucketSeries.h"
#include "HFUtils.h"

/********************************************************************
  TimeBucket
********************************************************************/
TimeBucket::TimeBucket(int minSamples) 
  :
  _minSamples(minSamples),
  _nSamples(0),
  _sum(0.0),
  _sumsq(0.0)
{
  
}

TimeBucket::~TimeBucket() {

}

int TimeBucket::nSamples() const {
  return _nSamples;
}

double TimeBucket::sum() const {
  return _sum;
}

double TimeBucket::sumsq() const {
  return _sumsq;
}


bool TimeBucket::mean(double &fv) const {
  if (_nSamples <= _minSamples) {
    fv = 0.0;
    return false;
  }
  fv = _sum/_nSamples;
  return true;
}

bool TimeBucket::sd(double &fv) const {
  if (_nSamples <= _minSamples) {
    fv = 0.0;
    return false;
  }
  fv = pow(_sumsq/_nSamples - pow(_sum/_nSamples,2),0.5);
  return true;
}

void TimeBucket::addSample(double val) {
  _nSamples++;
  _sum += val;
  _sumsq += (val * val);
}

void TimeBucket::clear() {
  _nSamples = 0;
  _sum = 0.0;
  _sumsq = 0.0;
}


/********************************************************************
  TimeBucketSeries code
********************************************************************/
TimeBucketSeries::TimeBucketSeries(int nPeriods, int minPeriods, int minSamplesPerPeriod) :
  _nPeriods(nPeriods),
  _minPeriods(minPeriods),
  _currentPeriod(-1),
  _buckets(nPeriods, minSamplesPerPeriod)
{

}

TimeBucketSeries::~TimeBucketSeries() {

}

void TimeBucketSeries::addBucket() {
  _currentPeriod++;
  int idx = _currentPeriod % _nPeriods;
  _buckets[idx].clear();
}

void TimeBucketSeries::addSample(double val) {
  int idx = _currentPeriod % _nPeriods;
  _buckets[idx].addSample(val);
}

bool TimeBucketSeries::mean(double &fv) const {
  fv = 0.0;
  if (_currentPeriod < _minPeriods) {
    return false;
  }
  fv = sum() / nSamples();
  return true;
}

bool TimeBucketSeries::sd(double &fv) const {
  fv = 0.0;
  if (_currentPeriod < _minPeriods) {
    return false;
  }
  int rnsamples = nSamples();
  double rsum = sum();
  double rsumsq = sumsq();
  fv = pow(rsumsq/rnsamples - pow(rsum/rnsamples,2),0.5);
  return true;
}


int TimeBucketSeries::nSamples() const {
  int ret = 0;
  int idx = std::min(_currentPeriod, _nPeriods-1);
  for (int i=0 ; i <= idx; i++) {
    ret += _buckets[i].nSamples();
  }
  return ret;
}

double TimeBucketSeries::sum() const {
  double ret = 0.0;
  int idx = std::min(_currentPeriod, _nPeriods-1);
  for (int i=0 ; i <= idx; i++) {
    ret += _buckets[i].sum();
  }
  return ret;
}

double TimeBucketSeries::sumsq() const {
  double ret = 0.0;
  int idx = std::min(_currentPeriod, _nPeriods-1);
  for (int i=0 ; i <= idx; i++) {
    ret += _buckets[i].sumsq();
  }
  return ret;
}


/********************************************************************
  TimeBucketSeriesTracker code
********************************************************************/
TimeBucketSeriesTracker::TimeBucketSeriesTracker(int bucketLength, int numBuckets, int minBucketsWithSamples) :
  _bucketLength(bucketLength),
  _numBuckets(numBuckets),
  _startTV(0),
  _marketOpen(false),
  _intervalNumber(0)
{
  _dm = factory<DataManager>::find(only::one);
  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in TimeBucketSeriesTracker::TimeBucketSeriesTracker)" );  

  TimeBucketSeries tbs(_numBuckets, minBucketsWithSamples, 1);
  _payload.assign(_dm->cidsize(), tbs);

  _dm->add_listener(this);
}

TimeBucketSeriesTracker::~TimeBucketSeriesTracker() {

}

void TimeBucketSeriesTracker::addSample(int cid, double val) {
  _payload[cid].addSample(val);
}

int TimeBucketSeriesTracker::nSamples(int cid) {
  return _payload[cid].nSamples();
}

bool TimeBucketSeriesTracker::mean(int cid, double &fv) const {
  return _payload[cid].mean(fv);
}

bool TimeBucketSeriesTracker::sd(int cid, double &fv) const {
  return _payload[cid].sd(fv);
}

void TimeBucketSeriesTracker::addBucket() {
  int sz = _dm->cidsize();
  for (int i = 0 ; i < sz; i++) {
    _payload[i].addBucket();
  }
}

void TimeBucketSeriesTracker::update(const TimeUpdate &au) {
  if ( au.timer() == _dm->marketOpen() ) {
    _marketOpen = true;
    _startTV = au.tv();
    _intervalNumber = 0;
  } else if ( au.timer() == _dm->marketClose() ) {
    _marketOpen = false;
  }

  if (_marketOpen == true) {
    double msDiff = HFUtils::milliSecondsBetween(_startTV, au.tv());
    if (msDiff >= (_bucketLength * _intervalNumber * 1000)) {
      addBucket();
      _intervalNumber++;
    }     
  }
}
