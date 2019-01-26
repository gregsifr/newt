/*
  KFRTSignal.h

  Revision History:
  - April 2010:  
    - Added functionality for scenario analysis - estimating market impact of hypothetical orders.
    - Added functionality for tracking/guesstimating the average market impact of other participants''
      trades.
*/

#ifndef __KFRTSIGNAL_H__
#define __KFRTSIGNAL_H__

#include <cl-util/factory.h>
using namespace clite::util;

#include "DataManager.h"
#include "AlphaSignal.h"
#include "ExchangeTracker.h"
#include "SpdTracker.h"
#include "FeeCalc.h"
#include "OpenTracker.h"

#include "VolatilityTracker.h"
#include "IBVolatilityTracker.h"

#include "MarketImpactEstimate.h"
#include "MarketImpactModel.h"

#include <vector>
using std::vector;

class VolatilityTracker;
class AverageTradingImpactTracker;

/*
   Alpha factor/model.
  - Uses filtering approach approach to infer "real" mid price from recent trades
    (RT).
  - Basic version lacks explanatory model.  Subclasses can add.
  - Current version adjusts trade prices for take liquidty fees, and also adjusts measurement
    error calculation for those fees.  This seems correct on 1st estimation.  Is it?
  - Current version attempts to use volume information as follows:
    - Treat each whole round lot as a separate data point / separate estimate of value.
    - Ignore odd lots and odd ends.
*/
class BasicKFRTSignal : public AlphaSignal, public MarketImpactModel, public TimeHandler::listener, public MarketHandler::listener {
 protected:
  factory<DataManager>::pointer _dm;         // Exists externally.  Accessed via factory system.
  factory<ExchangeTracker>::pointer _exchangeT; // Exists externally.  Accessed via factory system. 
  factory<debug_stream>::pointer _ddebug;    // For debug/error logging.
  factory<FeeCalc>::pointer      _feeCalc;   // For fee calculations.
  factory<OpenTracker>::pointer  _openT;     // Tracks per-stock opening times.
  factory<AverageTradingImpactTracker>::pointer  _atiTracker;  // Keeps track of market-impact per trade data points.
  factory<SpdTracker>::pointer   _spdT;      // Shifted to accessed via factory system 04272010.

  IBVolatilityTracker           *_volT;      // Created internally.
  
  vector<double> _priceEstV;                 // Price estimates, one per stock.
  vector<double> _priceVarV;                 // Price variance estimates, one per stock.
  vector<TimeVal> _priceTimeV;               // Time of last observed trade price, one per stock. 

  bool _useVolume;                           // Should we weight all (> = round-lot size) trades equally 
                                             //   (useVolume == false), or treat each round-lot as a separate
                                             //   trade (useVolume == true);

  bool _divideByEstSD;                       // Should returned alpha forecast be in raw bps, or divided by
                                             //   estimate standard deviation.

  bool _printTicks;                          // Should we print (debugging/diagnostic info) on each tick?
  bool _marketOpen;                          // Is the market currently open?
  TimeVal _marketOpenTV;                     // Time-val as of market-open message.

  const static double AVG_TL_FEE = 0.0025;   // Represeentative take-liquidity fee:  0.25 cents.
  const static double DEFAULT_MINUTE_VOL = 0.0020;  // Default vol: 20 bps/minute.  

  // Minimum amount of time past open before signal is assumed to have seen enough trades
  //   to generate a reliable signal.
  // Probably should be changed to not use const value for all stocks.
  // Default value: 8 minutes.
  const static int MIN_WARMUP_MILLISECONDS = 8 * 60 * 1000;

  // Adjust last estimate to current time.
  // Adjusts for market/internal/world state changes since last estimate was made.
  // Also updates last-estimate variance, according to (square of) same (linear)
  //   transformation applied to lastEstimateValue.
  // Notes:
  // - Kalman filter model assumes that state-transition function is linear.
  //   Therefore, in this approach, the state-transition model applies to
  //   lastEstimate (and lastEstimateVariance) should be linear model.
  // - Should return boolean value indicating whether the state transition was 
  //   successfully applied.
  //   If return value is false, lastEstimateValue and lastEstimateVariance 
  //   should not be modified.
  //   If return value is true, they should be updated accordingly.
  virtual bool adjustLastEstimate(int cid, const TimeVal &lastEstimateTime, 
				  const TimeVal &curtv,
				  double &lastEstimateValue, double &lastEstimateVariance);
  
  // Mark most-recent estimate of price, px-variance, with associated time.
  virtual void markEstimate(int cid, double priceEst, double priceEstVar, 
			    const TimeVal &curtv);

  // Apply a single (round-lot or greater) trade to value/signal estimate 
  //   for applicable stock.
  virtual void applyTrade(const DataUpdate &du);

  // Extension of KFUtils::simpleScalarKalmanFilter, that attempts to infer
  //   some extra information from trading volume in some theoretically-justified
  //   looking way.
  bool simpleScalarKalmanFilterWVolume(double lastEst, double lastEstVar, 
				       double thisObs, double thisME, double thisPI,
				       double &thisEst, double &thisEstVar,
				       double &lastEstWeight, double &thisObsWeight,
				       int volume);

  // Initialize _priceEstV to current stated mid price, across all stocks.
  virtual int initializePrices();

  // Get volatility estimate to use for specified stock.
  // Volatility estimate should be:
  // - 1 SD (in sd units, not var units).
  // - of per second (not per minute).
  // - return volatility (not price volatility).
  // - Try to use a volatility estimate that reflects true volatility of the 
  //   underlying stock process, with measurement error stripped out.
  virtual bool getVolatility(int cid, bool useDefault, double &fv);


  // Non-public functions related to market impact model estimation:
  void estimateImpact(int cid, int size, double price, Mkt::Side side, 
		      double lastEst, double thisEst, MarketImpactEstimate &fvImpact);
  bool applyTrade(int cid, ECN::ECN ecn, int size, double price,
		  Mkt::Side side, const TimeVal &tv, 
		  bool hypothetical, bool estimateImpact, 
		  MarketImpactEstimate &fvImpact);

  /*
    Some functions for better edge-case detection around early morning trading.
  */
  // Do we have sufficient internal state to update fv point & variance estimates
  //   on an incoming (real market) trade - for specified stock?
  virtual bool sufficientStateApplyTrade(int cid);

  // Do we have sufficient internal state to estimate market impact of a proposed
  //   trade in specified stock?
  virtual bool sufficientStateMarketImpact(int cid);

  // Do we have sufficient internal state to estimate fv/alpha for specified stock?
  virtual bool sufficientStateAlpha(int cid);
 public:
  BasicKFRTSignal();
  BasicKFRTSignal(bool printTicks);
  virtual ~BasicKFRTSignal();

  /*  To avoid annoyances of constructor w/ default value for this parameter.  */
  void setPrintTicks(bool printTicks) {_printTicks = printTicks;}
 
  /*
    UpdateListener functions.
  */
  // In no explanatory-model version, can apply KF estimation technique directly....
  virtual void update(const DataUpdate &du);
  // In current base class implementation, can just look for MARKET_OPEN
  //   messages and call initializePrices.
  virtual void update(const TimeUpdate &au);

  /*
    AlphSignal functions.
  */
  // Get expected alpha from signal.
  virtual bool getAlpha(int cid, double &fv);


  /*
    MarketImpactModel functions.
  */
  // Used for hypotheticals/scenario-analysis on potential trades.
  // What would the market impact of a hypothetical order be, conditional on it being filled,
  //   with the current set of market conditions.
  virtual bool marketImpactFill(int cid, ECN::ECN ecn, int size, double price,
				Mkt::Side side, int timeout, bool invisible, MarketImpactEstimate &fv);
 
  /*
    Public constants.
  */
  // Arbitrary.  Max assumed volume/size per trade.  Tries to damp down on effect of block trades.
  const static int TRADE_SIZE_CIELING;
};


/*
  Alpha factor/model.
  - Uses filter approach to infer "real" mid price from recent trades
    (RT).
  - Uses ETFs as proxies for market/industry/risk-factor returns.  Uses ETF returns
    as basis of simple explanatory model for stock returns.
  - Assumes:
    - 1...N ETFs, with real-time market-observable prices.
    - Simple single-factor explanatory model for each stock, with:
      E(Stock Return) = 0 + ETF-Beta * (ETF-Return), 
      where ETF-Beta and ETF-Return are both for the single ETF that best
      explains returns for each stock, 
      aka:
      - Each stock gets a single factor model based on a single ETF, but
      - The ETF chosen may be different for different stocks.
    - The choice of ETF, plus estimated beta is assumed to be externally specified.
*/
class ETFKFRTSignal : public BasicKFRTSignal {
 protected:
  vector<string> _expNameV;                 // Name of ETF with which element is associated.  For debugging.
  vector<int>    _expCidV;                  // Cid of ETF with which element is associated.  -1 for no-mapping.
  vector<double> _expBetaV;                 // Beta wrt specified ETF.                        0 for no mapping.
  vector<double> _expPrcV;                  // Price of specified ETF, as of last call to markEstimate.

  virtual bool adjustLastEstimate(int cid, const TimeVal &lastEstimateTime, 
				  const TimeVal &curtv, double &lastEstimateValue, double &lastEstimateVar);

  virtual void markEstimate(int cid, double priceEstV, double priceVarV, 
			    const TimeVal &curtv);

  // Old version, uses PerStockParams.
  bool parseParamsFileOld(string &paramsFile);
  // New version, uses file-table.
  bool parseParamsFile();  

  int associateStocksWithETFS();

  virtual int initializePrices();
 public:
  ETFKFRTSignal();
  virtual ~ETFKFRTSignal();
};

/*
  clite::util::file_table plug-in for reading KFRT ETF params from file.
  Assumes:
  - Each symbol maps --> ETF-NAME  + ETF-BETA.
    - e.g. BAC SPY 1.2
*/
class ETFKFRTFileTableElem {
 public:
  typedef std::string key_type;  // the key-word "key_type" is used in clite::util::file_table
  /// returns the symbol this line refers to
  std::string const &get_key() const { return _symbol; }

    
  ETFKFRTFileTableElem( const vector<string>& fieldNames, const vector<string>& fields );
  string getETFName() const {return _etfName;}
  double getETFBeta() const {return _etfBeta;}
protected:

  std::string    _symbol;
  std::string    _etfName;
  double _etfBeta; 
};


/*
  Variant of KFRTSignal that uses a generic externally specified explanatory model.
*/
class ExplanatoryReturnModel;
class ExplanatoryModelKFRTSignal : public BasicKFRTSignal {
 protected:
  ExplanatoryReturnModel *_expModel;           // Explanatory Model to use - specified externally.
  vector<double>    _expPrcV;                  // Price specified by explanatory model, as of last
                                               //   call to markEstimate.
                                               // Notes: - 1 per stock.
                                               // - ExplanatoryModel generates prices (in some space), not returns,
                                               //   but those prices are converted into returns.

  virtual bool adjustLastEstimate(int cid, const TimeVal &lastEstimateTime, 
				  const TimeVal &curtv, double &lastEstimateValue, double &lastEstimateVar);

  virtual void markEstimate(int cid, double priceEstV, double priceVarV, 
			    const TimeVal &curtv);

  virtual int initializePrices();
 public:
  ExplanatoryModelKFRTSignal(ExplanatoryReturnModel *expModel);
  virtual ~ExplanatoryModelKFRTSignal();

};


#endif  // __KFRTSIGNAL_H__
