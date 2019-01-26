/*
  KFRTSignal.cc

  Alpha factor/model.
  - Filters recent trade prices/VWAP to try to estimate where
    stock should be trading - in hope that differnece between current mid and 
    filtered trading px should predict something about future mid --> mid returns.
  - Initial version does not include any explanatory model.
  - Initial version also makes some specific assumptions about sources of measurement-error
    in VWAP series:
    - VWAP estimation error occurs only from px discreteness, plus effects of bid ask bounce.
  - To do:
    - Better explanatory model.
    - Better estimation of LT measurement error in sampled VWAP series.
*/

#include "KFRTSignal.h"
#include "VolatilityTracker.h"
#include "KFUtils.h"
#include "FeeCalc.h"
#include "PerStockParams.h"
#include "StringConversion.h"
#include "HFUtils.h"
#include "ExplanatoryModel.h"

#include "DataManager.h"
#include "BookTools.h"

#include <cl-util/float_cmp.h>

// Temporary - just for debugging.
#include "TradeTickPrinter.h"

#include "TradingImpactModel.h"

#include <iostream>
#include <ostream>
#include <vector>
#include <string>
#include <algorithm>
using namespace std;

const static string RELEVANT_LOG_FILE = "kfrtsignal";

const string ETF_BETA_FILE = "etfbetafile";
const static string DEFAULT_EXP_ETF_NAME = "SPY";
const static double DEFAULT_EXP_ETF_BETA = 1.0;
/**************************************************************************
  BasicKFRTSignal Code!!!!
**************************************************************************/
const int BasicKFRTSignal::TRADE_SIZE_CIELING = 10000;

/*
  Simple constructor - makes following changes to global factory stsme space:
  - If cant find ImbSignal, adds one.
  - Adds VolatilutyTracekr (of sub-class IBVolatilityTracker).
  - Adds SpdTracker.
*/
BasicKFRTSignal::BasicKFRTSignal() :
  _exchangeT( factory<ExchangeTracker>::get(only::one) ),
  _feeCalc( factory<FeeCalc>::get(only::one) ),
  _openT( factory<OpenTracker>::get(only::one) ),
  _priceEstV(0),
  _priceVarV(0),
  _priceTimeV(0),
  _useVolume(true),
  _divideByEstSD(false),
  _printTicks(false),
  _marketOpen(false),
  _marketOpenTV(0)
{
  _dm = factory<DataManager>::find(only::one);
  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in BasicKFRTSignal::BasicKFRTSignal)" ); 
  _exchangeT = factory<ExchangeTracker>::find(only::one);
  if( !_exchangeT )
    throw std::runtime_error( "Failed to get ExchangeTracker from factory (in BasicKFRTSignal::BasicKFRTSignal)" );  
  _ddebug = factory<debug_stream>::get( RELEVANT_LOG_FILE );
  if( !_ddebug )
    throw std::runtime_error( "Failed to get DebugStream from factory (in BasicKFRTSignal::BasicKFRTSignal)" ); 

  // should create one if not previously initialized.
  _atiTracker = factory<AverageTradingImpactTracker>::get(only::one);
  if( !_atiTracker )
    throw std::runtime_error( "Failed to get AverageTradingImpactTracker from factory (in BasicKFRTSignal::BasicKFRTSignal)" ); 

  // Imbalance signal.  Create new one IFF cant find existing one.
  factory<ImbTracker>::pointer imbSignal = factory<ImbTracker>::get(only::one);
  if (!imbSignal) 
    throw std::runtime_error( "Failed to get ImbTracker from factory (in BasicKFRTSignal::BasicKFRTSignal)" ); 

  // SpdTracker.  Use default parameters from factory system.  
  // Note: default parameters should be something like:
  //_spdT = new SpdTracker(50, 60, 1000); 
  _spdT = factory<SpdTracker>::get(only::one);
  if (!_spdT) {
    throw std::runtime_error( "Failed to get SpdTracker from factory (in BasicKFRTSignal::BasicKFRTSignal)" ); 
  }
  
  /*
    Components created internally....
  */
  // VolatilityTracker. 
  _volT = new IBVolatilityTracker(1000, 3600, 300, imbSignal.get());
  if (!_volT) {
    throw std::runtime_error( "Failed to create new IBVolatilityTracker (in BasicKFRTSignal::BasicKFRTSignal)" ); 
  }

  _priceEstV.assign(_dm->cidsize(), -1.0);
  _priceVarV.assign(_dm->cidsize(), -1.0);
  _priceTimeV.assign(_dm->cidsize(), 0);  // _priceTimeV.assign(_dm->cidsize(), _dm->curtv());

  _dm->add_listener(this);
}

BasicKFRTSignal::~BasicKFRTSignal() {
  delete _volT;
}

/*
  Initialize _priceEstV to current stated mid price, across all stocks.
  Also initializes _priceEstVarV to plausible level.
  Should be called on receipt of market open signal.
*/
int BasicKFRTSignal::initializePrices() {
  double bpx, apx, spd, mid, meVar;
  int ret = 0;
  size_t asz, bsz;
  for (int i = 0 ; i < _dm->cidsize(); i++) {
    if (HFUtils::getTradeableMarket(_dm.get(), i,Mkt::ASK,(int)0,(size_t)100,&apx,&asz) && HFUtils::getTradeableMarket(_dm.get(), i,Mkt::BID,(int)0,(size_t)100,&bpx,&bsz)) {
      mid = (bpx + apx)/2.0;
      spd = (apx - bpx);
      meVar = KFUtils::measurementErrorVariance(bpx - AVG_TL_FEE, apx + AVG_TL_FEE, 0.01);
      markEstimate(i, mid, meVar, _dm->curtv());
      ret++;
    }
  }
  return ret;
}

/*
  Adjust lastEstimateValue for market/internal/world state changes since
    estimate was made.
  BasicKFRTSignal does not include explicit explanatory model.  Implicitly:
  F(t + dt) = F(t).
*/
bool BasicKFRTSignal::adjustLastEstimate(int cid, const TimeVal &lastEstimateTime, 
					 const TimeVal &curtv, 
					 double &lastEstimateValue, double &lastEstimateVariance) {
  // Null state transition.  Can always be applied successfully.
  // Does not change last estimate value or variance.
  return true;
}

void BasicKFRTSignal::markEstimate(int cid, double priceEst, double priceEstVar, 
				   const TimeVal &curtv) {
    _priceEstV[cid] = priceEst;
    _priceVarV[cid] = priceEstVar;
    _priceTimeV[cid] = curtv;  
}

void BasicKFRTSignal::update(const DataUpdate &du) {
  // Apply trade updates only.
  if (!du.isTrade()) {
    return;
  }  
  // Ignore odd-lots.
  if (du.size < 100) {
    return;
  }
  //du.size = std::min(du.size, TRADE_SIZE_CIELING);
  applyTrade(du);
}

void BasicKFRTSignal::update(const TimeUpdate &au) {
  if (au.timer() == _dm->marketOpen()) {
    _marketOpen = true;
    _marketOpenTV = au.tv();
    initializePrices();
  } else if (au.timer() == _dm->marketClose()) {
    _marketOpen = false;
  }
}

bool BasicKFRTSignal::getVolatility(int cid, bool useDefault, double &fv) {
  double vol;
  // Might want to use some plausible default value, e.g. 4%/day = 20 bps per minute.
  // Current implementation does not do that, as filter weights are sensitive to piVar
  // and meVar estimates.
  if (!_volT->getVolatility(cid, vol) || isnan(vol) || isinf(vol)) {
    if (useDefault) {
      fv = DEFAULT_MINUTE_VOL / sqrt(60.0);
      // GVNOTE: Disabling logging of this message for the time being, since this fills up the kfrtsignal.log file
      //TAEL_PRINTF(_ddebug.get(), TAEL_ERROR,  "BasicKFRTSignal::getVolatility : Unable to estimate vol for stock %i (%s), using default value",\
      //            cid, _dm->symbol(cid));
      return true;
    } else {
      fv = 0.0;
      return false;
    }
  }
  // _volt estimates per minute vol.  Translate that to *per second* vol.
  vol /= sqrt(60.0);
  fv = vol;
  return true;
}

/*
  Apply actual trade that occurs in market.
*/
void BasicKFRTSignal::applyTrade(const DataUpdate &du) {

  if (!sufficientStateApplyTrade(du.cid)) {
    return;
  }

  // Apply trade, and also estimate market impact.
  MarketImpactEstimate fvImpact;
  // Somewhat arbitrary - tries to damp down on effect of block trades, which
  //   can be up to 30 seconds (????) stale in U.S. equity markets.
  int size = std::min(du.size, TRADE_SIZE_CIELING);
  if (!applyTrade(du.cid, du.ecn, size, du.price, du.side, du.tv, false, true, fvImpact)) {
    return;
  }
  // Add sample point to tracker distribution of trade impacts.
  _atiTracker->addSample(fvImpact);
}

/*
  Functions that check whether have (stock-specific) sufficient state to perform 
    various operations.
  - To apply a trade update, only need:
    - market open.
*/
bool BasicKFRTSignal::sufficientStateApplyTrade(int cid) {
  // Market closed - no signal.
  if (!_marketOpen) {
    return false;
  }
  return true;
}

// To estimate market impact of trade - need:
// - market open.
// - stock specific open.
// - 10 minutes of warmup time since stock-specific open.
bool BasicKFRTSignal::sufficientStateMarketImpact(int cid) {
  // Market closed - no signal.
  if (!_marketOpen) {
    return false;
  }

  // Stock not opened for trading.
  TimeVal stockOpenTV;
  if (!_openT->openTV(cid, stockOpenTV)) {
    return false;
  }

  // Not sufficient elapsed "warmup" time - no signal.
  // Note:  Used to be general warmup time since market open.  Switched to warmup
  //   time since stock specific open.
  TimeVal curtv = _dm->curtv();
  if (HFUtils::milliSecondsBetween(stockOpenTV, curtv) < MIN_WARMUP_MILLISECONDS) {
    return false;
  }

  // Not ncessary to have valid market quotes to estimate market impact....
  //if (!HFUtils::bestMid(_dm.get(), cid, midPx)) {
  //  return false;
  //}
  return true;
}

// Estimate alpha.  Need:
// - market open.
// - stock-specific open.
// - 10 minutes of warmup time post stock-specific open.
// - valid volatility estimate for stock (otherwise filter may not get right weights).
// Note: Potential edge case here:
// - In theory, should have received sufficently many trades since the point where 1st started
//   getting valid vol estimate that old (potentially incorrectly weighted) trades
//   have minimal weight.
// - This formulation allows potential edge case where:
//   - No valid vol estimate.
//   - Trade updates (incorrectly weighted).
//   - Start getting valid vol estimate.
//   - Query for alpha --> ok.
bool BasicKFRTSignal::sufficientStateAlpha(int cid) {
  double vol;
  if (!sufficientStateMarketImpact(cid)) {
    return false;
  }
  if (!getVolatility(cid, false, vol)) {
    return false;
  }
  return true;
}

/*
  Scenario analysis - guesstimate market impact of proposed trade, conditional
    on it being filled.
  Market impact of a hypothetical order, conditional on its being filled, and
    on market conditions being otherwise identical to current when order is filled.
  Does not do any sanity checking, e.g. that:
  - execution of proposed trade is plausible given current market conditions.
  - trade does not violate any reg-NMS constraints.
  - trade could execute given current market conditions.
*/
bool BasicKFRTSignal::marketImpactFill(int cid, ECN::ECN ecn, int size, double price,
				Mkt::Side side, int timeout, bool invisible, MarketImpactEstimate &fv) {
  TimeVal stockOpenTV, tv = _dm->curtv();
  fv.setImpact(cid, size, Mkt::BUY, false, 0.0, 0.0);

  //
  // Some sanity checking that we have valid data aginst which to generate hypothetical
  //   impact of a trade. Should be same checks applied in get alpha....
  //
  if (!sufficientStateMarketImpact(cid)) {
    return false;
  }

  //
  // Actual work of checking delta in point estimate.
  //
  return applyTrade(cid, ecn, size, price, side, tv, true, true, fv);
}


/*
  Does work of actually updating fv-estimate given that a new trade has occurred.
  Original version:
  - Took DataUpdate du as parameter directly.
  - Did not have code for estimating market impact of trade.
  - Did not have *hypothetical* parameter - always applied efects of trade to fv-estimate.
  Modified April 2010:
  - To allow for scenario analysis:  Aka, estimate impact of proposed trade, without actually
    applying state change from trade.
*/

bool BasicKFRTSignal::applyTrade(int cid, ECN::ECN ecn, int size, double price,
				 Mkt::Side side, const TimeVal &tv, 
				 bool hypothetical, bool estimate, 
				 MarketImpactEstimate &fvImpact) {
  double lastEst, lastEstVar, midPx, bidPx, askPx, currentSpread, trailingSpread, spread, piVar, meVar, msDiff, 
    vol, thisEst = 0.0, thisEstVar = 0.0, lastEstWeight = 0.0, thisObsWeight = 0.0, effectivePrice;

  // Temporary, for debugging.
  //char buf[256];
  //du.snprint(buf, 128);
  //std::cout << "BasicKFRTSignal::applyTrade called - du = " << buf << std::endl;

  // Applies during regular trading session.
  if (!_marketOpen) {
    return false;
  }

  // To round-lots trades.
  if (size < 100) {
    return false;
  }

  // Get current inside bid & ask.
  // Note:  KFRT calculation uses spread to estimate measurement-error in prices.
  // This introdudes the question of which sprread to use - the current measured spread
  //   (more responsive to current conditions), or a trailing average spread
  //   (more stable).  
  // Unfortunately, the current code base does not make it easy to extract exactly
  //   what the spread was when a given trade (now being seen) executed.  In addition,
  //   in live trading, the current code base also seems to end up fairly often with incorrectly
  //   locked/crossed market.  
  // In the current implementation, we use the spread at the time that the trade-message is
  //   received, floored at the trailing avg spread.
  
  // bid, ask, mid, using current mkt prices.
  if (!HFUtils::bestPrice(_dm.get(), cid, Mkt::BID, bidPx) ||
      !HFUtils::bestPrice(_dm.get(), cid, Mkt::ASK, askPx)) {
    //std::cerr << "Unable to get compsite BID and ASK " << cid << " not updating from trade tick" << std::endl;
    TAEL_PRINTF(_ddebug.get(), TAEL_ERROR,  "BasicKFRTSignal::applyTrade Unable to get compsite BID and ASK for stock %i, not updating from trade tick",cid);
    return false;
  }
  midPx = (bidPx + askPx)/2.0;
  currentSpread = askPx - bidPx;

  // Extract trailing average spread, and convert from return space --> px space.
  if (!_spdT->getASpd(cid, trailingSpread)) {
    TAEL_PRINTF(_ddebug.get(), TAEL_ERROR, "BasicKFRTSignal::applyTrade Unable to get trailing avg spd for stock %i, not updating from trade tick",cid);
    return false;
  }
  trailingSpread = trailingSpread * midPx;
  
  // Try to approximate what top-level bid/ask looked like at the time
  //   that the trade occurred.
  // Here, we try the following heuristic:
  // - We know 1 side of the market from the trade message.
  // - Use max of current spread & recent spread as a guess of how wide 
  //   the market was when the trade occurred.
  // - Use that to set the other side of the market.
  spread = std::max(currentSpread, trailingSpread);
  if (side == Mkt::BID) {
    bidPx = price;
    askPx = price + spread;
  } else {
    askPx = price;
    bidPx = price - spread;
  }
  midPx = (bidPx + askPx)/2.0;

  // Where did the trade occur, and what are take-liquidity fees on that venue?
  // Note:  FeeCalc expresses take liquidity fees as *negative* numbers.
  Mkt::Tape loTape = _exchangeT -> getTape( cid );
  double tlFee = _feeCalc->takeLiqFee(ecn, loTape);

  // Calculate effective trade px, including fees.
  // For now, we just include TL fee, assuming that:
  // - brokerage commissions are small.
  // - other fees are small.
  if (side == Mkt::BID) {
    effectivePrice = price - tlFee;
  } else {
    effectivePrice = price + tlFee;
  }

  //
  // Get stock volatility estimate.
  //
  if (!getVolatility(cid, true, vol)) {
    //std::cerr << "Unable to get vol for stock " << cid << " not updating from trade tick" << std::endl;
    TAEL_PRINTF(_ddebug.get(), TAEL_WARN,  "BasicKFRTSignal::applyTrade Unable to calculate volatility for stock %i, not updating from trade tick",cid);
    return false;
  }

  /*
    Estimate PI and ME Var.
  */
  lastEst = _priceEstV[cid];
  lastEstVar = _priceVarV[cid];  

  //
  // Note:  call to adjustLastestimate can return false indicating unable to
  //   adjust.  That function is defined to not change lastEst or lastEstVar
  //   in cases where it returns false.  Assuming that the adjustments are generally
  //   small (as appears to be the case with BKFRT and ETFKFRT, its probably ok
  //   to let "unable to adjust" translate to "apply null explanatroy model",
  //   which is the approach taken in the code below, which ignores the return
  //   value of adjustlastEstimate. 
  //
  adjustLastEstimate(cid, _priceTimeV[cid], tv, lastEst, lastEstVar);

  // Measurement Error variance assumed to be due to discreteness in pricing.
  msDiff = HFUtils::milliSecondsBetween(_priceTimeV[cid], tv);
  meVar = KFUtils::measurementErrorVariance(bidPx - tlFee, askPx + tlFee, 0.01);
  // Process Innovation variance assumed to be due to zero-drift brownian
  //   motion over time interval since last observation.
  piVar = KFUtils::processInnovationVariance(msDiff / 1000.0, vol, midPx);

  /*
    Special case - last estimate was NA.  Use 100% new estimate.
  */
  if (cmp<6>::EQ(lastEstVar, -1.0) || cmp<6>::EQ(lastEst, -1.0)) {
    if (estimate) {
      estimateImpact(cid, size, price, side, lastEst, effectivePrice, fvImpact); 
    }
    if (!hypothetical) {
      markEstimate(cid, effectivePrice, meVar, tv);
    }
    if (_printTicks) {
      TradeTickPrinter::printTradeTick(_dm.get(), cid, ecn, side, size, 
				       price, tv, bidPx, askPx, false);
      //     msDiff, vol, meVar, piVar, lastEstVar, lastEstWeight, thisObsWeight, thisEstVar, lastEst, effectivePrice, thisEst
      std::cout << msDiff << " " << vol << " " << meVar << " "  << piVar << " "  << lastEstVar << " " << lastEstWeight << " ";
      std::cout << thisObsWeight << " " << thisEstVar << " " << lastEst << " " << effectivePrice << " " << thisEst << std::endl; 
    }   
    return true;
  }

  /*
    Apply Kalman Filter.
    Should estimate both thisEst and thisEstVar.
  */
  simpleScalarKalmanFilterWVolume(lastEst, lastEstVar, effectivePrice, meVar, piVar,
				  thisEst, thisEstVar, lastEstWeight, thisObsWeight,
				  size);
  
  //  Temp - for debugging....
  //char buf[256];
  //sprintf(buf, "BasicKFRTSignal::applyTrade - msDiff = %f, size = %i, vol = %f, meVar = %f, piVar = %f, lastEstVar = %f, lastEstWeight = %f, thisObsWeight = %f, thisEstVar = %f, lastEst = %f, effectivePrice = %f, thisEst = %f",
  //	 msDiff, size, vol, meVar, piVar, lastEstVar, lastEstWeight, thisObsWeight, thisEstVar, lastEst, effectivePrice, thisEst);
  //string bufStr(buf);
  //std::cout << bufStr << std::endl;
  if (_printTicks) {
    TradeTickPrinter::printTradeTick(_dm.get(), cid, ecn, side, size, price, tv, bidPx, askPx, false);
    //     msDiff, vol, meVar, piVar, lastEstVar, lastEstWeight, thisObsWeight, thisEstVar, lastEst, effectivePrice, thisEst
    std::cout << msDiff << " " << vol << " " << meVar << " "  << piVar << " "  << lastEstVar << " " << lastEstWeight << " ";
    std::cout << thisObsWeight << " " << thisEstVar << " " << lastEst << " " << effectivePrice << " " << thisEst << std::endl;    
  }


  if (estimate) {
    estimateImpact(cid, size, price, side, lastEst, thisEst, fvImpact); 
  }
  /*
    Override lastEst, lastEstVar, lastEstTime with thisEst, thisEstVar, thisEstTime.
  */
  if (!hypothetical) {
    markEstimate(cid, thisEst, thisEstVar, tv);
  }  

  return true;
}

/*
  Populate MarketImpactEstimate from estimated fv change pre-trade --> post-trade.
*/
void BasicKFRTSignal::estimateImpact(int cid, int size, double price, Mkt::Side side, 
				     double lastEst, double thisEst, MarketImpactEstimate &fvImpact) {
  Mkt::Trade dir = Mkt::BUY;
  if (side == Mkt::BID) {
    dir = Mkt::SELL;
  }

  if (cmp<6>::EQ(lastEst, -1.0) || cmp<6>::EQ(thisEst, -1.0)) {
    fvImpact.setImpact(cid, size, dir, false, 0.0, 0.0);
    return;
  }
  
  // permanent impact:  change in fv estimate, expressed as a return.
  double pi = (thisEst - lastEst)/lastEst;
  // temporary impact:  execution price vs most recent estimate, expressed as a return.
  double ti = (price - thisEst)/lastEst;
  fvImpact.setImpact(cid, size, dir, true, ti, pi);
}

/*
  As KFUtils::simpleScalarKalmanFilter, but attempts to also use trade volume 
    information.
  Volume is used as follows:
  - If _useVolume is set to false, all (100+ share) trades are marked as a single
    new daat point, with specified observation-value, measurement-error, and
    process-innovation.
  - If _useVolume is set to true, each full-lot of 100 shares is treated as a
    separate new observation.  In this context:
    - Each new observation is assumed to have 
      - measurement error thisME.
      - observation value thisObs.
    - Subsequent observations (beyond the 1st) are assumed to occur at the same
      time as the 1st.  Thus, they get process-innovation 0.0.
    - On return, thisObsWeight should reflect the total weight given to all of the new 
      observations combined, as should lastEstWeight.
*/
bool BasicKFRTSignal::simpleScalarKalmanFilterWVolume(double lastEst, double lastEstVar, 
						      double thisObs, double thisME, double thisPI,
						      double &thisEst, double &thisEstVar,
						      double &lastEstWeight, double &thisObsWeight,
						      int volume) {
  double origLastEst = lastEst;
  int nlots = volume / 100;
  // Ignore odd lots.
  if (nlots <= 0) {
    thisEst = lastEst;
    thisEstVar = lastEstVar;
    lastEstWeight = 1.0;
    thisObsWeight = 0.0;
    return false;
  }

  // If treating all (>= round-lot) trades equally:
  // - Apply SSKF once.
  if (_useVolume == false) {
    return KFUtils::simpleScalarKalmanFilter(lastEst, lastEstVar, thisObs, thisME, thisPI,
					     thisEst, thisEstVar, lastEstWeight, thisObsWeight);
  }
  
  // If treating each round-lot of trade as independent data point:
  // - Apply SSKF once, with specified ME & PI.
  bool ret = KFUtils::simpleScalarKalmanFilter(lastEst, lastEstVar, thisObs, thisME, thisPI,
					       thisEst, thisEstVar, lastEstWeight, thisObsWeight);
  lastEst = thisEst;
  lastEstVar = thisEstVar;
  if (ret == false) {
    return ret;
  }

  // - Apply SSKF additional n - 1 times, with specified ME, but zero PI
  //   (as all of the "ticks" of a multiple round-lot trade ooccur at the exact
  //   same point in time - therefore definitionally there is no process innovation 
  //   between them.
  for (int i=1;i<nlots;i++) {
    // Assumed - no process innovation between subsequent "ticks".
    KFUtils::simpleScalarKalmanFilter(lastEst, lastEstVar, thisObs, thisME, 0.0,       
					       thisEst, thisEstVar, lastEstWeight, thisObsWeight);
    lastEst = thisEst;
    lastEstVar = thisEstVar;
  }

  // Calculate lastEstWeight and thisObsWeight so that they reflect the *total* weights
  //    applied across the (potentially multiple) applications of SSKF:
  thisObsWeight = (thisEst - origLastEst)/(thisObs - origLastEst);
  lastEstWeight = 1.0 - thisObsWeight;

  // Done.
  return true;
}

/*
  Get alpha forecast for stock cid.  Here defined as:
  - (_priceEstV[cid] - midpx(cid))/(midpx(cid)).
*/
bool BasicKFRTSignal::getAlpha(int cid, double &fv) {
  double midPx, lastEst, lastEstVar, vol;
  TimeVal lastEstTime, curTime, stockOpenTV;

  if(!sufficientStateAlpha(cid)) {
    return false;
  }

  // No valid market - no signal.
  if (!HFUtils::bestMid(_dm.get(), cid, midPx)) {
    return false;
  }
  lastEst = _priceEstV[cid];
  lastEstVar = _priceVarV[cid];
  lastEstTime = _priceTimeV[cid];
  if (cmp<6>::EQ(lastEstVar, -1.0) || cmp<6>::EQ(lastEst, -1.0)) {
    return false;
  }

  // Adjust lastEst for explanatory model since last time a measure was taken.  
  curTime = _dm->curtv();
  adjustLastEstimate(cid, lastEstTime, curTime, lastEst, lastEstVar);
  double forecast = ((lastEst - midPx)/midPx);

  // If we are forecasting using raw alpha:
  if (_divideByEstSD == false) {
    fv = forecast;
  } else {
    //
    // If we are scaling forecast based on # of standard-deviations (in estimate error)
    //   that we think we are from mid.
    //
    // Get underlying stock volatility.
    if (!getVolatility(cid, false, vol)) {
      return false;
    }
    // Adjust lastEstVar for elapsed time.
    double msDiff = HFUtils::milliSecondsBetween(lastEstTime, curTime);
    double piVar = KFUtils::processInnovationVariance(msDiff / 1000.0, vol, midPx);
    lastEstVar = lastEstVar + piVar;
    // And scale forecast by estimate standard error.
    fv = forecast / sqrt(lastEstVar);
 }
  return true;
}



/**************************************************************************
  ETFKFRTSignal Code!!!!
**************************************************************************/
ETFKFRTSignal::ETFKFRTSignal() :
  BasicKFRTSignal(),
  _expNameV(_dm->cidsize(), ""),
  _expCidV(_dm->cidsize(), -1),
  _expBetaV(_dm->cidsize(), 0.0),
  _expPrcV(_dm->cidsize(), 0.0)
{
  // Parse config file holding stock --> ETF + beta mapping,
  if (!parseParamsFile()) {
    cerr << "ETFKFRTSignal - unable to parse config file " << ETF_BETA_FILE << std::endl;
    cerr << " will use NULL explanatory model for all stocks" << std::endl;
  }
}

/*
  
*/
ETFKFRTSignal::~ETFKFRTSignal() {

}

/*
  Parse params file.  Assumed format:
  - Space-separated file.
  - header line, with:  
    Stock ETF BETA
  - 1 line per stock, with:
    ticker etf-symbol etf-beta.
  Assumes that _expNameV, _expCidV, _expBetaV, _expPrcV are all pre-populated
    with appropriate values that indicate "no stock --> ETF mapping found".
*/
bool ETFKFRTSignal::parseParamsFileOld(string &paramsFile) {
  // Read in mapping from ticker --> etf-name + etf-beta.
  PerStockParams p;
  string fs(" ");
  int nr = PerStockParamsReader::readFile(_dm.get(), paramsFile, fs, 1, p);  
  if (nr <= 0) {
    return false;
  }
  dynamic_bitset<> found;
  nr = p.populateValues(_dm.get(), 1, found, _expNameV);
  nr = p.populateValues(_dm.get(), 2, found, _expBetaV);
  if (nr != (int)_dm->cidsize()) {
    cerr << "ETFKFRTSignal::parseParamsFile - warning parsing params file " << paramsFile << std::endl;
    cerr << " stocks in population set " << _dm->cidsize() << " : stocks found in params file " << nr << std::endl;    
  }

  associateStocksWithETFS();
  return true;
}

/*
  Parse params file.  Assumed format:
  - Space-separated file.
  - header line, with:  
    Stock ETF BETA
  - 1 line per stock, with:
    ticker etf-symbol etf-beta.
  Assumes that _expNameV, _expCidV, _expBetaV, _expPrcV are all pre-populated
    with appropriate values that indicate "no stock --> ETF mapping found".
  New version:
  - Uses file-table.
*/
bool ETFKFRTSignal::parseParamsFile() {
  // Read in the ticker etf-name  etf-beta values
  file_table<ETFKFRTFileTableElem> symToETFTable( ETF_BETA_FILE );

  if ( symToETFTable.empty() ){
    throw std::runtime_error( "No symbols read in by ETFKFRTSignal::parseParamsFile (input_file=" + 
			      ETF_BETA_FILE + ")");    
  }

  ETFKFRTFileTableElem *data;
  int nf = 0;
  for( int cid=0; cid<_dm->cidsize(); cid++ ) {
    const char* symbol = _dm->symbol( cid );
    clite::util::file_table<ETFKFRTFileTableElem>::iterator it = symToETFTable.find(symbol);    //  clite::util::
    if( it != symToETFTable.end() ) {
      data = new ETFKFRTFileTableElem( it->second ); // "it" is a pair of (key,value)
      _expNameV[cid] = data->getETFName();
      _expBetaV[cid] = data->getETFBeta();
      nf++;
      delete data;
    } else {
     // No element found.  Default to etf=SPY with beta of 1.0.
     if (strcmp(symbol, DEFAULT_EXP_ETF_NAME.c_str()) != 0) {
       _expNameV[cid] = DEFAULT_EXP_ETF_NAME;
       _expBetaV[cid] = DEFAULT_EXP_ETF_BETA;          }
    }
  }

  associateStocksWithETFS();

  // Dump some debugging out put to log file to potentially assist with production issues.
  TAEL_PRINTF(_ddebug.get(), TAEL_WARN, "ETFKFRT::parseParamsFile - parsing params file %s", ETF_BETA_FILE.c_str());
  TAEL_PRINTF(_ddebug.get(), TAEL_WARN, "stocks in population set %i : stocks found in params file %i", _dm->cidsize(), nf);
  for ( int i=0;i<_dm->cidsize();i++) {
    TAEL_PRINTF(_ddebug.get(), TAEL_WARN, "%3i (%-5s)  ETFKFRT::parseParamsFile  expName %s  expBeta %.2f  expCid %i",
		    i, _dm->symbol(i),_expNameV[i].c_str(), _expBetaV[i], _expCidV[i]);
  }
  return true;
}


/*
  Given _expNameV, map _expCidV to symbol, or -1 for no-such symbol.
  
*/
int ETFKFRTSignal::associateStocksWithETFS() {
  int ret = 0;
  int i;
  string expName;
  int expCid;
  for (i = 0; i < _dm->cidsize(); i++) {
    expName = _expNameV[i];
    StringConversion::stripQuotes(expName);
    StringConversion::stripSpaces(expName);
    expCid = _dm->cid(expName.c_str());
    if (expCid != -1) {
      _expCidV[i] = expCid;
      ret++;
    }
  }
  return ret;
}

/*
  Mark that stock (cid) has estimated "true" price priceEst, with variance priceVar
*/
void ETFKFRTSignal::markEstimate(int cid, double priceEst, double priceEstVar, 
			    const TimeVal &curtv) {
  BasicKFRTSignal::markEstimate(cid, priceEst, priceEstVar, curtv);
  
  int etfCid = _expCidV[cid];
  // Mark price of explanatory ETF as of time curtv.
  if (etfCid != -1) {
    double etfPrice = _priceEstV[etfCid];
    _expPrcV[cid] = etfPrice;
  } 
  
}

bool ETFKFRTSignal::adjustLastEstimate(int cid, const TimeVal &lastEstimateTime, 
				       const TimeVal &curtv, 
				       double &lastEstimateValue, double &lastEstimateVariance) {
  if (cmp<6>::EQ(lastEstimateValue, -1.0)) {
    // lastEstimateValue of NA.  Linear transform of NA probably also yields NA.
    // Return false, and don't adjust last estimate value & variance.
    return false;
  }

  // Identify explanatory model (in this case, ETF).
  // No known explanatory model --> dont adjust price.
  int etfCid = _expCidV[cid];
  if (etfCid == -1) {
    // No specified explanatory model ==> null explanatory model.
    // Return true and dont adjust last known estimate value or variance.
    return true;
  }
  // Get ETF price as of last estimate.
  // price of -1.0 implies that the ETF price has not yet been sset from valid mark.
  // --> dont adjust for changes in it.
  double oldETFPrice = _expPrcV[cid];
  if (cmp<6>::EQ(oldETFPrice, -1.0)) {
    // Unable to guesstimate benchmark return.
    // Return false and dont adjust last known estimate value or variance.
    return false;
  }

  // Get current ETF price.
  // Note: Current implementation uses "true" mid adjusted for KFRTSignal alpha,
  //   not stated mid....
  // No currently valid estimated price --> don't adust input price.
  double curETFPrice = _priceEstV[etfCid];
  if (cmp<6>::EQ(curETFPrice, -1.0)) {
    // Unable to guesstimate benchmark return.
    // Return false and dont adjust last known estimate value or variance.   
    return false;
  }

  // Compute explanatory model (ETF) return from last px --> current px.
  // e.g. 0.03 for 3% positive return.
  double etfReturn = (curETFPrice - oldETFPrice)/oldETFPrice;

  double etfBeta = _expBetaV[cid];
  double predictedStockReturn = etfReturn * etfBeta;

  // Adjust lastEstimateValue by that explanatory model return.
  double mult = (1.0 + predictedStockReturn);
  lastEstimateValue = lastEstimateValue * mult;
  lastEstimateVariance = lastEstimateVariance * mult * mult;

  // And return true.
  return true; 
}

/*
  As per BasicKFRTSignal::initializePrices, except that it also
    tries to populate _expPrcV with initial prices.
*/
int ETFKFRTSignal::initializePrices() {
  int ret = BasicKFRTSignal::initializePrices();
  
  for (unsigned int i = 0; i < _expPrcV.size(); i++) {
    int etfCid = _expCidV[i];
    if (etfCid != -1) {
      double etfPrc = _priceEstV[etfCid];
      _expPrcV[i] = etfPrc;
    }
  }

  return ret;
}

/**************************************************************************
  ETFKFRTFileTableElem Code!!!!
**************************************************************************/
ETFKFRTFileTableElem::ETFKFRTFileTableElem( const vector<string>& fieldNames, 
					    const vector<string>& fields ) {
  // assert some conditions
  if( fields.size() < 3 )
    throw table_data_error( "at ETFKFRTFileTableElem::ETFKFRTFileTableElem: Too few fields in line" );    
  _symbol = fields[0];
  _etfName = fields[1];
  _etfBeta = atof( fields[2] .c_str());  
}

/**************************************************************************
  ExplanatoryModelKFRTSignal Code!!!!
**************************************************************************/
ExplanatoryModelKFRTSignal::ExplanatoryModelKFRTSignal(ExplanatoryReturnModel *expModel) 
  :
  BasicKFRTSignal(),
  _expModel(expModel),
  _expPrcV(_dm->cidsize(), 0.0)
{
  
}

ExplanatoryModelKFRTSignal::~ExplanatoryModelKFRTSignal() {

}

/*
  Mark that stock (cid) has estimated "true" price priceEst, with variance priceVar
*/
void ExplanatoryModelKFRTSignal::markEstimate(int cid, double priceEst, double priceEstVar, 
			    const TimeVal &curtv) {
  BasicKFRTSignal::markEstimate(cid, priceEst, priceEstVar, curtv);
  
  double fv;
  if (_expModel->getFairValue(cid, fv)) {
    _expPrcV[cid] = fv;
  } else {
    _expPrcV[cid] = -1.0;    
  }  
}


bool ExplanatoryModelKFRTSignal::adjustLastEstimate(int cid, const TimeVal &lastEstimateTime, 
						    const TimeVal &curtv, 
						    double &lastEstimateValue, double &lastEstimateVariance) {
   if (cmp<6>::EQ(lastEstimateValue, -1.0)) {
    // lastEstimateValue of NA.  Linear transform of NA probably also yields NA.
    // Return false, and don't adjust last estimate value & variance.
    return false;
  }

  // Get ExpModel price as of last call to markEstimate.
  // price of -1.0 implies that expModel price was not yet set, or there was
  //   a problem on last attenpt to calculate it.
  // --> dont adjust for changes in expmodel price.
  double oldETFPrice = _expPrcV[cid];
  if (cmp<6>::EQ(oldETFPrice, -1.0)) {
    // Unable to guesstimate benchmark return.
    // Return false and dont adjust last known estimate value or variance.
    return false;
  }

  // Get current ExpModel price.
  // No currently valid estimated price --> don't adust input price.
  double curETFPrice = -1.0;
  if (!_expModel->getFairValue(cid, curETFPrice) || cmp<6>::EQ(curETFPrice, -1.0)) {
    // Unable to guesstimate benchmark return.
    // Return false and dont adjust last known estimate value or variance.   
    return false;
  }

  // Compute explanatory model (ETF) return from last px --> current px.
  // e.g. 0.03 for 3% positive return.
  double etfReturn = (curETFPrice - oldETFPrice)/oldETFPrice;
  double predictedStockReturn = etfReturn;

  // Adjust lastEstimateValue by that explanatory model return.
  double mult = (1.0 + predictedStockReturn);
  lastEstimateValue = lastEstimateValue * mult;
  lastEstimateVariance = lastEstimateVariance * mult * mult;

  // And return true.
  return true;    
}

/*
  As per BasicKFRTSignal::initializePrices, except that it also
    tries to populate _expPrcV with initial prices.
*/
int ExplanatoryModelKFRTSignal::initializePrices() {
  double etfPrice;
  int ret = BasicKFRTSignal::initializePrices();
  
  for (unsigned int i = 0; i < _expPrcV.size(); i++) {
    if (_expModel->getFairValue(i, etfPrice)) {
      _expPrcV[i] = etfPrice;
    } else {
      _expPrcV[i] = etfPrice = -1.0;
    }
  }

  return ret;
}
