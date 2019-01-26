
#ifndef __PARTICIPATIONRATEMODEL_H__
#define __PARTICIPATIONRATEMODEL_H__


#include <cl-util/factory.h>
using namespace clite::util;

#include "DataManager.h"
#include "AlphaSignal.h"
#include "ExchangeTracker.h"
#include "SpdTracker.h"
#include "FeeCalc.h"

#include <vector>
using std::vector;

/*
  Base-class / interface for a model used to guesstimate (percentage of volume)
    participation for different order placement strategies or combinations
    thereof.
  Participation rate calculations may end up requiring detailed knowledge of 
    order placement strategies, and thus end up being closely coupled with
    order placement component code.
  However, for now, we start with some simple participation rate models, and 
    the participation rate / scheduling framework is initially experimental,
    so we keep participation rate code separate from order placement 
*/
class ParticipationRateModel {
 public:
  ParticipationRateModel();
  virtual ~ParticipationRateModel();
  /*
    Estimate expected % of volume participation rate for trading:
    - specified stock.
    - in specified direction.
    - over specified time interval.
    Notes:
    - Ignores rounding/granularity effects. Aka, the participation rate
      for providing liquidity on 1 share is lower than that for providing
      for liquidity on 100 shares, assuming that both involve 1 order that
      has the same expected fill time.  This effects is ignored.
  */
  virtual bool estimateParticipationRate(int cid, Mkt::Trade dir, double priority, 
					 const TimeVal &startTV, const TimeVal &endTV,
					 double &fv) = 0;
};

/*
  Default participation rate model for liquidity providing components only,
    as of April 2010.
  Initial implementation is very simple:
  - Uses flat guess for all stocks.
  - Does not differentiate based on e.g. past experience, volatility, stock price, etc.
  Should be updated based on realized results from live trading as order placement
    strategies are added/modified.
*/
class DefaultProvideLiquidityPRM : public ParticipationRateModel {
 public:
  DefaultProvideLiquidityPRM();
  virtual ~DefaultProvideLiquidityPRM();
  
  virtual bool estimateParticipationRate(int cid, Mkt::Trade dir, double priority, 
					 const TimeVal &startTV, const TimeVal &endTV,
					 double &fv);
};

#endif   // __PARTICIPATIONRATEMODEL_H__
