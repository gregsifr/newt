#include "ExplanatoryModelTester.h"
// DataManager
#include "DataManager.h"
// AlphaSignal.
#include "AlphaSignal.h"
// ExplanatoryReturnModel
#include "ExplanatoryModel.h"
// SyntheticIndex
#include "SyntheticIndex.h"

#include <cl-util/float_cmp.h>

#include "HFUtils.h"

/*****************************************************
  ExplanatoryReturnModelTester code.
*****************************************************/
ExplanatoryReturnModelTester::ExplanatoryReturnModelTester(ExplanatoryReturnModel *erm, 
							   AlphaSignal *fvSignal, 
							   tael::Logger &dlog,
							   int firstSampleIndex, 
							   int sampleMilliSeconds) 
  :
  _erm(erm),
  _fvSignal(fvSignal),
  _dlog(dlog),
  _startTV(0),
  _sampleNum(0),
  _marketOpen(false),
  _firstSampleIndex(firstSampleIndex),
  _sampleMilliSeconds(sampleMilliSeconds)
{
  _dm = factory<DataManager>::find(only::one);
  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in ExplanatoryReturnModelTester::ExplanatoryReturnModelTester)" );

  _lastSampleMidV.assign(_dm->cidsize(), -1.0);
  _haveLastSampleMidV.assign(_dm->cidsize(), false);
  _lastSampleERMV.assign(_dm->cidsize(), -1.0);
  _haveLastSampleERMV.assign(_dm->cidsize(), false);

  _dm->add_listener(this);
}

ExplanatoryReturnModelTester::~ExplanatoryReturnModelTester() {

}

void ExplanatoryReturnModelTester::update(const TimeUpdate &au) {
  if (au.timer() == _dm->marketOpen()) {
    _marketOpen = true;
    _startTV = au.tv();
    sampleAllStocks();
  } else if (au.timer() == _dm->marketClose()) {
    _marketOpen = false;
  }
  
  processTimerUpdate(au);
  
}

void ExplanatoryReturnModelTester::processTimerUpdate(const TimeUpdate &au) {
  // If sufficient time has not elapsed since last sample + association,
  //   return.
  TimeVal curtv = _dm->curtv();
  double msDiff = HFUtils::milliSecondsBetween(_startTV, curtv);
  if (msDiff >= (_sampleNum * _sampleMilliSeconds)) {
    // Re-sample mid prices + signal values for each stock.
    sampleAllStocks();    
  }
}

/*
  Sample all stocks:
  - Populate a vector currentMidV with current (fvSignal adjusted) fair-values.
  - If this is not the 1st sample of the day:
    - Walk through all stocks in population set, printing both actual and
      ERM-predicted returns since last sample.
  - Copy currentMidV --> _lastSampleMidV.
*/
void ExplanatoryReturnModelTester::sampleAllStocks() {
  if (!_marketOpen) return;

  // Get current market fair-values, in currentMidV.
  vector<double> currentMidV;
  vector<char> haveCurrentMidV;
  SyntheticIndex::constituentFairValues(_dm.get(), _fvSignal, currentMidV, haveCurrentMidV);

  // Get current explanatory-model fair-values, in currentERMV.
  vector<double> currentERMV;
  vector<char> haveCurrentERMV;
  if (_sampleNum == 0) {
    ExplanatoryReturnModel::getInitialFairValues(_dm.get(), _erm, currentERMV, haveCurrentERMV);
  } else{
    ExplanatoryReturnModel::getFairValues(_dm.get(), _erm, currentERMV, haveCurrentERMV);
  }
  
  // Not 1st sample of day:  Print ERM-expected and fv-signal-adjusted returns.
  if (_sampleNum > 0) {
    printReturns(_lastSampleMidV, _haveLastSampleMidV, currentMidV, haveCurrentMidV, 
		 _lastSampleERMV, _haveLastSampleERMV, currentERMV, haveCurrentERMV);
  }
  
  // Copy new --> last sample mid and ERM vectors.
  _lastSampleMidV = currentMidV;
  _haveLastSampleMidV = haveCurrentMidV;
  _lastSampleERMV = currentERMV;
  _haveLastSampleERMV = haveCurrentERMV; 

  // 1st sample - print header info....
  if (_sampleNum == 0) {
    printHeader();
  }

  // Increment sample number.
  _sampleNum++;
}

/*
  cid  symbol  trailing-actual-return  current-mid  trailing-erm-return
*/
void ExplanatoryReturnModelTester::printHeader() {
  char buf[128];
  snprintf(buf, 128, "cid\tsymbol\tret-actual\tmidpx\tret-erm\ttime\n");
  TAEL_PRINTF(&_dlog, TAEL_WARN, "%s", buf);
}

void ExplanatoryReturnModelTester::printObservation(int cid, const char*symbol, double mid, 
						    double tretActual, double tretERM, const TimeVal &tv) {
  char buf[128];
  DateTime dt(tv);
  snprintf(buf, 128, "%i\t%s\t%f\t%f\t%f\t(HH)%i(MM)%i(SS)%i\n", cid, symbol, 
	   tretActual, mid, tretERM, dt.hh(), dt.mm(), dt.ss());
  TAEL_PRINTF(&_dlog, TAEL_WARN, "%s", buf);
}

void ExplanatoryReturnModelTester::printObservationError(int cid, const char*symbol, double mid, 
							 const TimeVal &tv) {
  char buf[128];
  DateTime dt(tv);
  snprintf(buf, 128, "%i\t%s\t%s\t%f\t%s\t(HH)%i(MM)%i(SS)%i\n", cid, symbol, "NA", mid, "NA", 
	   dt.hh(), dt.mm(), dt.ss());
  TAEL_PRINTF(&_dlog, TAEL_WARN, "%s", buf);
}

void ExplanatoryReturnModelTester::printReturns(vector<double> &lastSampleMidV, vector<char> &haveLastSampleMidV,
					  vector<double> &currentMidV,    vector<char> &haveCurrentMidV,
					  vector<double> &lastSampleERMV, vector<char> &haveLastSampleERMV,
					  vector<double> &currentERMV,    vector<char> &haveCurrentERMV) {
  double midReturn, ermReturn;
  int sz = lastSampleMidV.size();
  for (int i=0;i<sz;i++) {
    midReturn = -1.0;
    ermReturn = -1.0;
    if (haveLastSampleMidV[i] && haveCurrentMidV[i]) {
      midReturn = (currentMidV[i] - lastSampleMidV[i])/lastSampleMidV[i];
    }
    if (haveLastSampleERMV[i] && haveCurrentERMV[i]) {
      ermReturn = (currentERMV[i] - lastSampleERMV[i])/lastSampleERMV[i];
    }
    printObservation(i, _dm->symbol(i), currentMidV[i], midReturn, ermReturn, _dm->curtv()); 
  }
}
