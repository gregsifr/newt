#include "TradingImpactModel.h"
#include "HFUtils.h"
#include "KFRTSignal.h"

#include "VolatilityTracker.h"
#include "IBVolatilityTracker.h"
#include "IntervalVolumeTracker.h"

/********************************************************************
  TradingImpactTracker code.
********************************************************************/
TradingImpactTracker::TradingImpactTracker() {

}

TradingImpactTracker::~TradingImpactTracker() {

}

/********************************************************************
  AverageTradingImpactTracker code.
********************************************************************/
/*
  Notes:
  - Initializes both ti and pi trackers to use 60 second buckets, and look back
    over trailing 20 of them.
  - Also initializes with 8 minute "warm-up" time.
*/
AverageTradingImpactTracker::AverageTradingImpactTracker() :
  TradingImpactTracker(),
  _tiTracker(60, 20, 8),  // trailing 20 minutes worth of data, with 8 minutes of "warm up" time.
  _piTracker(60, 20, 8),  // trailing 20 minutes worth of data, with 8 minutes of "warm up" time.
  _numSamples(0, 0)
{
  _dm = factory<DataManager>::find(only::one);

  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in AverageTradingImpactTracker::AverageTradingImpactTracker)" );

  _numSamples.assign(_dm->cidsize(), 0);   
}

AverageTradingImpactTracker::~AverageTradingImpactTracker() {

}

void AverageTradingImpactTracker::addSample(MarketImpactEstimate &est) {
  double ti, pi;
  int cid, tsize;
  // Ignore place-holder estimates.
  if (!est.goodEstimate()) {
    return; 
  }
  cid = est.cid();
  // Ignore the 1st trade in each stock - generally very noisy data point.
  if (_numSamples[cid] == 0) {
    _numSamples[cid]++;
    return;
  }
  // Somewhat arbitrary - tries to damp down on the effect of block trades or big mis-reported trades.
  tsize = std::min(BasicKFRTSignal::TRADE_SIZE_CIELING, est.size());
  ti = est.temporaryImpact();
  pi = est.permanentImpact();
  ti = fabs(ti);                    // temporary impact stored per trade.
  pi = fabs(pi) / (double)tsize;    // permanent impact stored per share.
  // Add in 1 sample per round-lot.  Tbis should (approximately) weight trades in relation
  //   to their size, which is useful if trade impact per share is not constant in trade size.
  for (  ;  tsize>0  ;  tsize-=100  ) { 
    _tiTracker.addSample(cid, ti);
    _piTracker.addSample(cid, pi);
  }
  _numSamples[cid]++;
}

bool AverageTradingImpactTracker::estimateTemporaryImpact(int cid, int numShares, Mkt::Trade dir, 
							  double pRate, double &fv) {
  double ti;
  double tsign = (dir == Mkt::BUY ? 1.0: -1.0);
  if (!_tiTracker.mean(cid, ti)) {
    return false;
  }
  fv = ti * tsign;  // temporary impact estimate stored per trade.
  return true;
}

bool AverageTradingImpactTracker::estimatePermanentImpact(int cid, int numShares, Mkt::Trade dir, 
							  double pRate, double &fv) {
  double pi;  // permanent impact estimate stored per share.
  double tsign = (dir == Mkt::BUY ? 1.0: -1.0);
  if (!_piTracker.mean(cid, pi)) {
    return false;
  }
  fv = pi * numShares * tsign;
  return true;
}


bool AverageTradingImpactTracker::estimateImpact(int cid, int numShares, Mkt::Trade dir, 
						 double pRate, MarketImpactEstimate &fv) {
  double ti, pi;
  if (!estimateTemporaryImpact(cid, numShares, dir, pRate, ti) ||
	!estimatePermanentImpact(cid, numShares, dir, pRate, pi)) {
    fv.setImpact(cid, numShares, dir, false, 0.0, 0.0);
    return false;
  } 
  fv.setImpact(cid, numShares, dir, true, ti, pi);
  return false;
}

/********************************************************************
  TradingImpactModel code.
********************************************************************/
TradingImpactModel::TradingImpactModel() {

}

TradingImpactModel::~TradingImpactModel() {

}

/********************************************************************
  AverageTradingImpactModel code.
********************************************************************/
const string AVG_TRADING_IMPACT_MODEL_LOG_FILE = "avgtradingimpactmodel";
AverageTradingImpactModel::AverageTradingImpactModel() :
  TradingImpactModel(),
  _marketOpen(false),
  _lastPrintTV(0)
{
  _dm = factory<DataManager>::find(only::one);
  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in AverageTradingImpactModel::AverageTradimgImpactModel)" ); 

  _ddebug = factory<debug_stream>::get( AVG_TRADING_IMPACT_MODEL_LOG_FILE );
  if( !_ddebug )
    throw std::runtime_error( "Failed to get DebugStream from factory (in AverageTradingImpactModel::AverageTradingImpactModel)" );

  // ALERT ALERT - this code is a bit brittle.
  // It relies on KFRT signal to also find same AverageTradingImpactTracker vis factory system, and
  //   populate in when trading/market impact data points.
  // Thus, if no KFRTSignal object is created, _atiTracker will exist, but not be populated with any 
  //   data samples.  
  // AverageTradingImpactModel code should *at least* be smart enough to return false
  //   on calls to marketImpactTrading in this case - but its still a brittle solution.
  _atiTracker = factory<AverageTradingImpactTracker>::get(only::one);
  if( !_atiTracker )
    throw std::runtime_error( "Failed to get AverageTradingImpactTracler from factory (in BasicKFRTSignal::BasicKFRTSignal)" ); 

  _dm->add_listener(this);  // for debugging only....
}

AverageTradingImpactModel::~AverageTradingImpactModel() {

}

bool AverageTradingImpactModel::marketImpactTrading(int cid, int previousShares, int numShares, 
						    Mkt::Trade dir, double pRate, MarketImpactEstimate &fv) { 
  return _atiTracker->estimateImpact(cid, numShares, dir, pRate, fv);
}

void AverageTradingImpactModel::update(const TimeUpdate &au) {
  if (au.timer() == _dm->marketOpen()) {
    _marketOpen = true;
  } else if (au.timer() == _dm->marketClose()) {
    _marketOpen = false;
  } 
  
  if (_marketOpen) {
    double msdiff = HFUtils::milliSecondsBetween(_lastPrintTV, au.tv());
    if (msdiff >= 60000) {
      printState();
      _lastPrintTV = au.tv();
    }
  }
}

void AverageTradingImpactModel::printState() {
  MarketImpactEstimate est;
  char buf[256];
  DateTime dt(_dm->curtv());
  TAEL_PRINTF(_ddebug.get(), TAEL_WARN, "AverageTradingImpactModel::printState called  curtv = %s", dt.getfulltime());
  for (int i=0 ; i < _dm->cidsize() ; i++) {
    _atiTracker->estimateImpact(i, 100, Mkt::BUY, 0.025, est);
    est.snprint(buf, 256);
    TAEL_PRINTF(_ddebug.get(), TAEL_WARN, "%s  %s", _dm->symbol(i), buf);
  }
}


/********************************************************************
  ImbTradingImpactModel code.
********************************************************************/
const string IMB_TRADING_IMPACT_MODEL_LOG_FILE = "imbtradingimpactmodel";
ImbTradingImpactModel::ImbTradingImpactModel() :
  _sampleMilliSeconds(1000),     // sample every second.
  _numSamplePoints(1200),        // trailing 20 minutes worth of data.
  _minSamplePoints(480),         // min of 8 minutes to "warm up".
  _sampleNumber(0),
  _startTV(0),
  _marketOpen(false),
  _lotImpactV(0),
  _queueSizeV(0)
{
  _dm = factory<DataManager>::find(only::one);
  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in ImbTradingImpactModel::ImbTradingImpactModel)" );   
  _ddebug = factory<debug_stream>::get( IMB_TRADING_IMPACT_MODEL_LOG_FILE );
  if( !_ddebug )
    throw std::runtime_error( "Failed to get DebugStream from factory (in ImbTradingImpactModel::ImbTradingImpactModel)" );
  _imbSignal = factory<ImbTracker>::get(only::one);
  if (!_imbSignal) 
    throw std::runtime_error( "Failed to get ImbSignal from factory (in ImbTradingImpactModel::ImbTradingImpactModel)" );
  
  _lotImpactV.assign(_dm->cidsize(), NULL);
  _queueSizeV.assign(_dm->cidsize(), NULL);

  for (int i=0;i<_dm->cidsize();i++) {
    _lotImpactV[i] = new CircBuffer<double>(_numSamplePoints);
    _queueSizeV[i] = new CircBuffer<double>(_numSamplePoints);
  }

  _dm->add_listener(this);
}

ImbTradingImpactModel::ImbTradingImpactModel(ImbTracker *imbSignal) :
  _imbSignal(imbSignal),
  _sampleMilliSeconds(1000),     // sample every second.
  _numSamplePoints(1200),        // trailing 20 minutes worth of data.
  _minSamplePoints(480),         // min of 8 minutes to "warm up".
  _sampleNumber(0),
  _startTV(0),
  _marketOpen(false),
  _lotImpactV(0),
  _queueSizeV(0)
{
  _dm = factory<DataManager>::find(only::one);
  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in ImbTradingImpactModel::ImbTradingImpactModel)" );   
  _ddebug = factory<debug_stream>::get( IMB_TRADING_IMPACT_MODEL_LOG_FILE );
  if( !_ddebug )
    throw std::runtime_error( "Failed to get DebugStream from factory (in ImbTradingImpactModel::ImbTradingImpactModel)" );
  
  _lotImpactV.assign(_dm->cidsize(), NULL);
  _queueSizeV.assign(_dm->cidsize(), NULL);

  for (int i=0;i<_dm->cidsize();i++) {
    _lotImpactV[i] = new CircBuffer<double>(_numSamplePoints);
    _queueSizeV[i] = new CircBuffer<double>(_numSamplePoints);
  }

  _dm->add_listener(this);
}

ImbTradingImpactModel::~ImbTradingImpactModel() {
  for (int i=0;i<_dm->cidsize();i++) {
    delete _lotImpactV[i];
    delete _queueSizeV[i];
  }  
}

void ImbTradingImpactModel::sampleAll() {
  if (_sampleNumber++ > 0) {
    populateImpacts();
  }
}

void ImbTradingImpactModel::flushImpactSamples() {
  int vbs = _dm->cidsize();
  for (int i = 0 ; i < vbs ; i++) {
    CBD *tbuf = _lotImpactV[i];
    tbuf->clear();  
    tbuf = _queueSizeV[i];
    tbuf->clear();
  }
}

void ImbTradingImpactModel::update(const TimeUpdate &au) {
  if (au.timer() == _dm->marketOpen()) {
    onMarketOpen(au);
  } else if (au.timer() == _dm->marketClose()) {
    onMarketClose(au);
  } 
  onTimerUpdate(au);
}

void ImbTradingImpactModel::onMarketOpen(const TimeUpdate &au) {
  _marketOpen = true;
  _startTV = au.tv();
  _sampleNumber = 0;
  flushImpactSamples();
}

void ImbTradingImpactModel::onMarketClose(const TimeUpdate &au) {
  _marketOpen = false;
}

void ImbTradingImpactModel::onTimerUpdate(const TimeUpdate &au) {
  if (!_marketOpen) {
    return;
  }
  double msdiff = HFUtils::milliSecondsBetween(_startTV, au.tv());
  if (msdiff >= (_sampleNumber * _sampleMilliSeconds)) {
    sampleAll();
  }
}

/*
  Fill in per-stock market-impact estimates.

  Basic algorithm:
  - Query _imbSignal for current point-estimate of change in fv from posting 1
    additional lot.
  - Also query _dm for current top-level queue size.
  - Add samples to _lotImpactV anmd _queueSizeV.
*/
void ImbTradingImpactModel::populateImpacts() {
  double bidAlpha, bidFV, askAlpha, askFV, tImpact, tShs, avgImpact, avgShs;
  size_t bshs, ashs;
  double bpx, apx;
  bool valid, print;
  MarketImpactEstimate est;
  DateTime dt(_dm->curtv());
  print = ((_sampleNumber % 60) == 0);
  if (print) {
    TAEL_PRINTF(_ddebug.get(), TAEL_INFO, "ImbTradingImpactModel::populateImpacts called - curTV = %s",
		  dt.getfulltime());
  }
  for (int i=0 ; i < _dm->cidsize(); i++) {
    // Initialize variables to default values.
    bidAlpha = bidFV = askAlpha = askFV = tImpact = tShs = avgImpact = avgShs = 0.0;
    bshs = ashs = 0;

    // Populate current top-level queue size on bid & ask sides.
    valid = getMarket(_dm->masterBook(), i, Mkt::BID, 0, &bpx, &bshs) &&
      getMarket(_dm->masterBook(), i, Mkt::ASK, 0, &apx, &ashs);

    // Populate change in fv from placing 1 additional round-lot on top-level BID
    //   side, then on top-level ASK side.
    if (valid) {
      valid = _imbSignal->calcImb(i, 0, 100, 0, 0, bidAlpha, bidFV) &&
	_imbSignal->calcImb(i, 0, 0, 0, 100, askAlpha, askFV);
    }
    // Estimate FV change from posting additional round-lot, both on BID &
    //   ASK side, and average both sides.
    // Add sample points to _lotImpact & _queueSize series.
    if (valid) {
      tImpact = (bidAlpha - askAlpha)/2.0;
      tShs = ((double)(bshs + ashs))/2.0;
      _lotImpactV[i]->add(tImpact);
      _queueSizeV[i]->add(tShs);
    } else {
      TAEL_PRINTF(_ddebug.get(), TAEL_WARN, "%-5s  stock %i : no valid estimate",
		      _dm->symbol(i), i);      
    }

    if (print) {
      _lotImpactV[i]->getAvg(_minSamplePoints, &avgImpact);
      _queueSizeV[i]->getAvg(_minSamplePoints, &avgShs);
      TAEL_PRINTF(_ddebug.get(), TAEL_INFO, "%-5s  stock %i : SIZE %i  BSHS %i  ASHS %i  BPX %.2f  APX %.2f  VALID %i  THIS-IMPACT-EST %.10f  THIS-Q-SIZE %.2f  AVG-IMPACT-EST %.10f  AVG-Q-SIZE %.2f",
		      _dm->symbol(i), i, 100, (int)bshs, (int)ashs, bpx, apx, (int)valid, tImpact, tShs, avgImpact, avgShs); 
    }
  }
}

bool ImbTradingImpactModel::marketImpactTrading(int cid, int previousShares, int numShares, 
						Mkt::Trade dir, double pRate, MarketImpactEstimate &fv) {
  double lotImpact, queueSize;
  CBD *lotImpactBuf = _lotImpactV[cid];
  CBD *queueSizeBuf = _queueSizeV[cid];
  //
  // Edge cases - impact of trading negative or zero shares.
  //
  // Trading negative size - not valid.
  if (numShares < 0) {
    fv.setImpact(cid, numShares, dir, false, 0.0, 0.0);
    return false;
  }
  // Trading 0 shares - valid, with 0 impact.
  if (numShares == 0) {
    fv.setImpact(cid, numShares, dir, true, 0.0, 0.0);
    return true;
  }
  //
  // Real numShares - estimate impact.
  //
  // Estimate impact of trading 100 shares.
  if (!lotImpactBuf->getAvg(_minSamplePoints, &lotImpact) || !queueSizeBuf->getAvg(_minSamplePoints, &queueSize)) {
    fv.setImpact(cid, numShares, dir, false, 0.0, 0.0);
    return false;
  }  
  int tsign;
  if (dir == Mkt::BUY) {
    tsign = 1;
  } else {
    tsign = -1;
  }
  // Estimate impact of trading N shares.
  // lotImpact should hold estimated impact of trading 1 lot.
  // Impact of trading each additional lot should decrease with power-law function as
  //   queue-size increases.
  double thisImpact = 0.0, totalImpact = 0.0, k;
  vector<int> chunkSizes;
  k = _imbSignal->getK(cid);
  HFUtils::chunkOrderShares(numShares, 100, chunkSizes);
  for (unsigned int i = 0 ; i < chunkSizes.size(); i++) {
    int chunkSz = chunkSizes[i];
    thisImpact = lotImpact * pow((i * 100) + queueSize + previousShares, k)/pow(queueSize + previousShares, k) * ((double)chunkSz)/100.0;
    totalImpact += thisImpact;
  }
  fv.setImpact(cid, numShares, dir, true, 0.0, tsign * totalImpact);
  return true;    
}


bool ImbTradingImpactModel::marketImpactTrading(int cid, int previousShares, int numShares, 
						Mkt::Trade dir, double pRate, vector<MarketImpactEstimate> &fv) {
  double lotImpact, queueSize;
  CBD *lotImpactBuf = _lotImpactV[cid];
  CBD *queueSizeBuf = _queueSizeV[cid];
  //
  // Edge cases - impact of trading negative or zero shares.
  //
  // Trading negative size - not valid.
  if (numShares < 0) {
    fv.resize(0);
    return false;
  }
  // Trading 0 shares - valid, with 0 impact.
  if (numShares == 0) {
    fv.resize(1);
    fv[0].setImpact(cid, numShares, dir, true, 0.0, 0.0);
  }
  //
  // Real numShares - estimate impact.
  //
  // Estimate impact of trading 100 shares.
  if (!lotImpactBuf->getAvg(_minSamplePoints, &lotImpact) || !queueSizeBuf->getAvg(_minSamplePoints, &queueSize)) {
    fv.resize(0);
    return false;
  }  
  int tsign;
  if (dir == Mkt::BUY) {
    tsign = 1;
  } else {
    tsign = -1;
  }
  // Estimate impact of trading N shares.
  // lotImpact should hold estimated impact of trading 1 lot.
  // Impact of trading each additional lot should decrease with power-law function as
  //   queue-size increases.
  double thisImpact = 0.0, totalImpact = 0.0, k;
  vector<int> chunkSizes;
  k = _imbSignal->getK(cid);
  HFUtils::chunkOrderShares(numShares, 100, chunkSizes);
  int sz = chunkSizes.size(), totalShares = 0;
  fv.resize(sz);
  for (unsigned int i = 0 ; i < chunkSizes.size(); i++) {
    int chunkSz = chunkSizes[i];
    totalShares += chunkSz;
    thisImpact = lotImpact * pow((i * 100) + queueSize + previousShares, k)/pow(queueSize + previousShares, k) * ((double)chunkSz)/100.0;
    totalImpact += thisImpact;
    fv[i].setImpact(cid, totalShares, dir, true, 0.0, tsign * totalImpact);
  }
  return true;    
}
