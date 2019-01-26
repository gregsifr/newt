/*
  ExplanatoryModelTester.h
  
  Simple DataManager/UpdateListener based widget for testing 
    explanatory return models against high-frequency data.
  This widget:
  - Given:
    - An ExplanatoryReturnModel R.
    - A population set of stocks S1...Sn
    - A sampling frequency F.
  - Walks through trading time, periodically, for stocks in S1...Sn:
    - Printing actual mid -> mid FV return over last interval.
    - Printing explanatory model expected return over same interval.
  - Actual analysis of ERM explanatory model is left for post-analysis from
    generated text output, e.g. using R.
*/

#ifndef __EXPLANATORYMODELTESTER__H__
#define __EXPLANATORYMODELTESTER__H__


#include <vector>
using namespace std;

#include <cl-util/factory.h>
using namespace clite::util;

#include "DataManager.h"
#include "HFUtils.h"

#include <tael/Log.h>
using namespace trc;

#include "c_util/Time.h"
using trc::compat::util::TimeVal;

class DataManager;
class AlphaSignal;
class ExplanatoryReturnModel;

/*
  Inputs:
  - _fvSignal:  Optional alpha signal used to calculate stock "fair values" used for actual 
    return calculation.  May be specified as null/identity-signal/dummy-signal to just
    use stated-mids for ex-post return calculation.
    - Need not be same signal as that used for e.g. SymtheticIndex calculation in explanatory-return
      model.
*/
class ExplanatoryReturnModelTester : public TimeHandler::listener {
 protected:
  /*
    Internal State.
  */
  factory<DataManager>::pointer       _dm;
  ExplanatoryReturnModel              *_erm;
  AlphaSignal                         *_fvSignal;  // Optional signal (can be specified as null/identity signal).
  tael::Logger                     &_dlog;      // Logger for output.
  TimeVal                             _startTV;    // TimeVal of 1st sample point.
  int                                 _sampleNum;
  bool                                _marketOpen;
  int                                 _firstSampleIndex;   // Index of 1st stock in population set to start sampling.  Default = 0
  int                                 _sampleMilliSeconds; // How frequently to sample, in milliseconds. 
  vector<double>                      _lastSampleMidV;     // Market mid-prices as of last sampling-point.
  vector<char>                        _haveLastSampleMidV;
  vector<double>                      _lastSampleERMV;  // Mid prices from ERM, as of last sampling point.
  vector<char>                        _haveLastSampleERMV;

  /*
    Non-public Member Functions.
  */
  // Write header/format-info to dlog.
  void printHeader();  
  void printObservation(int cid, const char*symbol, double mid, double tretActual, double tretERM, const TimeVal &tv);
  void printObservationError(int cid, const char*symbol, double mid, const TimeVal &tv);

  // Write returns (actual and ERM-predicted) since last sampling interval
  //   for all stocks.
  void printReturns(vector<double> &lastSampleMidV, vector<char> & haveLastSampleMidV,
		    vector<double> &currentMidV,    vector<char> &haveCurrentMidV,
		    vector<double> &lastSampleERMV, vector<char> &haveLastSampleERMV,
		    vector<double> &currentERMV,    vector<char> &haveCurrentERMV);

  void processTimerUpdate(const TimeUpdate &au);
  void sampleAllStocks();

 public:
  ExplanatoryReturnModelTester(ExplanatoryReturnModel *erm, AlphaSignal *fvSignal, tael::Logger &dlog, int firstSampleIndex,
			 int sampleMilliSeconds);
  virtual ~ExplanatoryReturnModelTester();
 
  /*
    GroupUpdate functions.
  */
  virtual void update(const TimeUpdate &au);
};

#endif     //  __EXPLANATORYMODELTESTER__H__
