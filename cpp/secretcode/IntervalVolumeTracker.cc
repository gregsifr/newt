
#include "IntervalVolumeTracker.h"
#include "HFUtils.h"

const string RELEVANT_LOG_FILE = "misc";

/*****************************************************
  IntervalVolumeTracker code
*****************************************************/
IntervalVolumeTracker::IntervalVolumeTracker() :
  _sampleMilliSeconds(5000),
  _numSamplePoints(360),
  _minSamplePoints(120),
  _sampleNumber(0),
  _volumeBufV(0),
  _lastVolumeV(0),
  _sampleStartTime(0),
  _marketOpen(false)
{
  _dm = factory<DataManager>::find(only::one);
  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in IntervalVolumeTracker::IntervalVolumeTracker)" ); 
  _ddebug = factory<debug_stream>::get( RELEVANT_LOG_FILE );
  if( !_ddebug )
    throw std::runtime_error( "Failed to get DebugStream from factory (in IntervalVolumeTracker::IntervalVolumeTracker)" ); 

  _volumeBufV.resize(_dm->cidsize());
  _lastVolumeV.assign(_dm->cidsize(), 0.0);  

  for (int i=0;i<_dm->cidsize();i++) {
    _volumeBufV[i] = new CircBuffer<double>(_numSamplePoints);
  }  
  _dm->add_listener(this);
}

IntervalVolumeTracker::IntervalVolumeTracker(int sampleMilliSeconds, int numSamplePoints, int minSamplePoints) :
  _sampleMilliSeconds(sampleMilliSeconds),
  _numSamplePoints(numSamplePoints),
  _minSamplePoints(minSamplePoints),
  _sampleNumber(0),
  _volumeBufV(0),
  _lastVolumeV(0),
  _sampleStartTime(0),
  _marketOpen(false)
{
  _dm = factory<DataManager>::find(only::one);
  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in IntervalVolumeTracker::IntervalVolumeTracker)" ); 
  _ddebug = factory<debug_stream>::get( RELEVANT_LOG_FILE );
  if( !_ddebug )
    throw std::runtime_error( "Failed to get DebugStream from factory (in IntervalVolumeTracker::IntervalVolumeTracker)" ); 

  _volumeBufV.resize(_dm->cidsize());
  _lastVolumeV.assign(_dm->cidsize(), 0.0);  

  for (int i=0;i<_dm->cidsize();i++) {
    _volumeBufV[i] = new CircBuffer<double>(numSamplePoints);
  }  
  _dm->add_listener(this);
}

IntervalVolumeTracker::~IntervalVolumeTracker() {
  for (int i=0;i<_dm->cidsize();i++) {
    delete _volumeBufV[i];
  }
}

void IntervalVolumeTracker::addSample() {
  if (_sampleNumber++ > 0) {
    populateVolumes();
    addVolumeSamples();
  }
}

void IntervalVolumeTracker::addVolumeSamples() {
  unsigned int vbs = _volumeBufV.size();
  for (unsigned int i = 0 ; i < vbs ; i++) {
    double tsample = _lastVolumeV[i];
    CBD *tbuf = _volumeBufV[i];
    tbuf->add(tsample);    
  }
}

void IntervalVolumeTracker::flushVolumeSamples() {
  unsigned int vbs = _volumeBufV.size();
  for (unsigned int i = 0 ; i < vbs ; i++) {
    CBD *tbuf = _volumeBufV[i];
    tbuf->clear();    
  }
}

void IntervalVolumeTracker::update(const TimeUpdate &au) {
  if (au.timer() == _dm->marketOpen()) {
    onMarketOpen(au);
  } else if (au.timer() == _dm->marketClose()) {
    onMarketClose(au);
  } 
  onTimerUpdate(au);
}

void IntervalVolumeTracker::onMarketOpen(const TimeUpdate &au) {
  _marketOpen = true;
  _sampleStartTime = au.tv();
  _sampleNumber = 0;
  flushVolumeSamples();
}

void IntervalVolumeTracker::onMarketClose(const TimeUpdate &au) {
  _marketOpen = false;
}

void IntervalVolumeTracker::onTimerUpdate(const TimeUpdate &au) {
  if (!_marketOpen) {
    return;
  }
  double msdiff = HFUtils::milliSecondsBetween(_sampleStartTime, au.tv());
  if (msdiff >= (_sampleNumber * _sampleMilliSeconds)) {
    addSample();
  }
}

bool IntervalVolumeTracker::getVolumeAverage(int cid, double &fv) {
  double tmp;
  bool ret = _volumeBufV[cid]->getAvg(_minSamplePoints, &tmp);
  if (ret == false) {
    fv = 0.0;
    return false;
  }
  fv = tmp;
  return true;
}

bool IntervalVolumeTracker::getVolumeStdev(int cid, double &fv) {
  double tmp;
  bool ret = _volumeBufV[cid]->getStdev(_minSamplePoints, &tmp);
  if (ret == false) {
    fv = 0.0;
    return false;
  }
  fv = tmp;
  return true;
}


/*****************************************************
  IBVolumeTracker code
*****************************************************/
/*
  Notes:
  - Current versions of IBVolatilityTracker & IBVolumeTracker make own TradeIntervalBarTrackers.
  - For production use, should probably be changed to try to share a TradeIntervalBarTracker,
    accessed through the factory system. 
*/
IBVolumeTracker::IBVolumeTracker() :
  IntervalVolumeTracker()
{
  _bart = new TradeIntervalBarTracker(_sampleMilliSeconds); 
}

IBVolumeTracker::IBVolumeTracker(int sampleMilliSeconds, int numSamplePoints, int minSamplePoints) :
  IntervalVolumeTracker(sampleMilliSeconds, numSamplePoints, minSamplePoints)
{
  _bart = new TradeIntervalBarTracker(sampleMilliSeconds);
  // Should be taken care of in base class....
  //_dm->add_listener(this);
}


IBVolumeTracker::~IBVolumeTracker() {
  delete _bart;
}

void IBVolumeTracker::populateVolumes() {
  IntervalBar bar;
  bool barf;
  unsigned int sz = _dm->cidsize();
  for (unsigned int i = 0 ; i < sz ; i++) {
    barf = _bart->barFinished(i, bar);
    // Check that have new data bar.
    if (!barf) {
      // Should be changed to using asynch logger.
      //sprintf(buf, "IBVolatilityTracker::populateCurrentPrices - _sampleNumber %i, stock %i - unable to get bar data, using signal-adjusted mid px",
      //      _sampleNumber, i);
      //string bufStr(buf);
      //std::cout << bufStr << std::endl;
      TAEL_PRINTF(_ddebug.get(), TAEL_ERROR, "IBVolumeTracker::populateVolumes - _sampleNumber %i, stock %i - unable to get bar data, assuming 0 volume",
	      _sampleNumber, i);     
    }  
    _lastVolumeV[i] = bar.volume();
  }
}


/*****************************************************
  SignedIBVolumeTracker code
*****************************************************/
SignedIBVolumeTracker::SignedIBVolumeTracker() :
  IBVolumeTracker()
{

}

SignedIBVolumeTracker::SignedIBVolumeTracker(int sampleMilliSeconds, int numSamplePoints, int minSamplePoints) :
  IBVolumeTracker(sampleMilliSeconds, numSamplePoints, minSamplePoints)
{

}

SignedIBVolumeTracker::~SignedIBVolumeTracker() {

}

void SignedIBVolumeTracker::populateVolumes() {
  IntervalBar bar;
  bool barf;
  unsigned int sz = _dm->cidsize();
  for (unsigned int i = 0 ; i < sz ; i++) {
    barf = _bart->barFinished(i, bar);
    // Check that have new data bar.
    if (!barf) {
      // Should be changed to using asynch logger.
      //sprintf(buf, "IBVolatilityTracker::populateCurrentPrices - _sampleNumber %i, stock %i - unable to get bar data, using signal-adjusted mid px",
      //      _sampleNumber, i);
      //string bufStr(buf);
      //std::cout << bufStr << std::endl;
      TAEL_PRINTF(_ddebug.get(), TAEL_ERROR, "IBVolumeTracker::populateVolumes - _sampleNumber %i, stock %i - unable to get bar data, assuming 0 volume",
	      _sampleNumber, i);     
    }  
    _lastVolumeV[i] = bar.signedVolume();
  }
}
