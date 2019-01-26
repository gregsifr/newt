#include "ExplanatoryModel.h"

#include <cl-util/float_cmp.h>

// PerStockParams, plus some associated string-conversion functions.
#include "PerStockParams.h"
#include "StringConversion.h"

/*********************************************************************
  ExplanatoryModel code
*********************************************************************/
ExplanatoryReturnModel::ExplanatoryReturnModel() {

}

ExplanatoryReturnModel::~ExplanatoryReturnModel() {

}

int ExplanatoryReturnModel::getFairValues(DataManager *dm, ExplanatoryReturnModel *erm, 
					  vector<double> &fillValues, vector<char> &fillFounds) {
  bool valid;
  double fv;
  int ret = 0;
  int sz = dm->cidsize();
  fillValues.resize(sz);
  fillFounds.resize(sz);
  for (int i=0;i<sz;i++) {
    valid = erm->getFairValue(i, fv);
    fillValues[i] = fv;
    fillFounds[i] = valid;
    if (valid) ret++;
  }
  return ret;
}

int ExplanatoryReturnModel::getInitialFairValues(DataManager *dm, ExplanatoryReturnModel *erm, 
						 vector<double> &fillValues, vector<char> &fillFounds) {
  bool valid;
  double fv;
  int ret = 0;
  int sz = dm->cidsize();
  fillValues.resize(sz);
  fillFounds.resize(sz);
  for (int i=0;i<sz;i++) {
    valid = erm->getInitialFairValue(i, fv);
    fillValues[i] = fv;
    fillFounds[i] = valid;
    if (valid) ret++;
  }
  return ret;
}


/*********************************************************************
  ERMETFMid code
*********************************************************************/
ERMETFMid::ERMETFMid(string &betaFile) :
  ExplanatoryReturnModel(),
  _expNameV(0),
  _expCidV(0),
  _expBetaV(0),
  _initialPriceV(0),
  _marketOpen(false)
{
  _dm = factory<DataManager>::find(only::one);
  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in ERMETFMid::ERMETFMid)" );   

  _expNameV.assign(_dm->cidsize(), "");
  _expCidV.assign(_dm->cidsize(), -1);
  _expBetaV.assign(_dm->cidsize(), 0.0);
  _initialPriceV.assign(_dm->cidsize(), -1.0);


  // Parse config file holding stock --> ETF + beta mapping,
  if (!parseParamFile(betaFile)) {
    char buf[128];
    snprintf(buf, 128, "ERMETFMid::ERMETFMid - unable to parse config file %s", betaFile.c_str());
    throw std::runtime_error(buf);  
  }  

  _dm->add_listener(this);
}

ERMETFMid::~ERMETFMid() {

}

void ERMETFMid::update(const TimeUpdate &au) {
  if (au.timer() == _dm->marketOpen()) {
    _marketOpen = true;
    initializePrices();
  } else if (au.timer() == _dm->marketClose()) {
    _marketOpen = false;
  }
}

void ERMETFMid::update(const DataUpdate &du) {
  double midPx;
  if (!_marketOpen) {
    return;
  }
  if (cmp<6>::EQ(_initialPriceV[du.cid], -1.0)) {
    if (getMidPrice(du.cid, midPx)) {
      _initialPriceV[du.cid] = midPx;
    }
  }
}

// Get current mid market price for specified stock.
bool ERMETFMid::getMidPrice(int cid, double &fv) {
  double bidPx, askPx;
  size_t bidSize, askSize;
  if (!getMarket(_dm->masterBook(), cid, Mkt::BID, 0, &bidPx, &bidSize) ||
      !getMarket(_dm->masterBook(), cid, Mkt::ASK, 0, &askPx, &askSize)) {
    fv = 0.0;
    return false;
  } 
  fv = (bidPx + askPx)/2.0;
  return true;
}

void ERMETFMid::initializePrices() {
  double midPx;
  int sz = _dm->cidsize();
  for (int i=0;i<sz;i++) {
    // cidx of index constituent i.
    if (!getMidPrice(i, midPx)) {
      _initialPriceV[i] = -1.0;    // used as NA value.
    } else {
      _initialPriceV[i] = midPx;
    }    
  }
}

bool ERMETFMid::parseParamFile(string &paramsFile) {
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
    std::cerr << "ETFKFRTSignal::parseParamsFile - warning parsing params file " << paramsFile << std::endl;
    std::cerr << " stocks in population set " << _dm->cidsize() << " : stocks found in params file " << nr << std::endl;    
  }

  associateStocksWithETFS();
  return true;
}

/*
  Given _expNameV, map _expCidV to symbol, or -1 for no-such symbol.
  
*/
int ERMETFMid::associateStocksWithETFS() {
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
  Get current fair-value.  Again, in the context of the 
    ExplanatoryReturnModel class, fair value is defined to not have any
    basis, and returns in fair-value space, not fair values directly, should
    be used.
  Given this, it is natural to define the "fair value" of any stock as:
    - If the stock is not mapped to any ETF, or has a beta of 0.0 vs ETF
      - fair-value is always 1.0.
    - If the stock is mapped to an ETF:  Then stock fv =
      1.0 + (etf return from initial mid --> current mid) * etf-beta.
*/
bool ERMETFMid::getFairValue(int cid, double &fv) {
  int expCid = _expCidV[cid];
  // Null explanatory model.  Expected return always 0%.  Aka fv constant.
  if (expCid == -1) {
    fv = 1.0;
    return true;
  }
  // Unknown initial price for ETF.  Unable to calculate current "fair value"
  //   relative to known initial price.
  double initialPrice = _initialPriceV[expCid];
  if (cmp<6>::EQ(initialPrice, -1.0)) {
    fv = -1.0;
    return false;
  }
  // Unknown current mid price for ETF.  Unable to calculate current "fair value"
  //   relative to known initial price.
  double midPrice;
  if (!getMidPrice(expCid, midPrice)) {
    fv = -1.0;
    return false;
  }

  // Known current & initial ETF prices.  Use those + bets(stock, etf) to calculate
  //   expected return
  double etfBeta = _expBetaV[cid];
  double etfReturn = ((midPrice - initialPrice)/initialPrice);
  fv = 1.0 + (etfReturn * etfBeta);
  return true;  
}

bool ERMETFMid::getInitialFairValue(int cid, double &fv) {
  fv = 1.0;
  return true;
}


/*********************************************************************
  ERMLinearMultiFactor code
*********************************************************************/
ERMLinearMultiFactor::ERMLinearMultiFactor(AlphaSignal *idxCFVSignal, 
					   bmatrixd &factorWeights, bmatrixd &factorBetas) :
  _S(factorWeights.size1()),
  _F(factorWeights.size2()),
  _factorBetas(factorBetas),
  _indeces(_F, NULL)
{
  vector<double> tweights;
  for (int f = 0;f < _F; f++) {
    copyWeights(factorWeights, _S, f, tweights);
    _indeces[f] = new SyntheticIndexHF(idxCFVSignal, tweights);
  }
}

ERMLinearMultiFactor::~ERMLinearMultiFactor() {
  for (int f = 0; f < _F; f++) {
    delete _indeces[f];
  }
}

// Copy weights for all stocks * 1 factor into weights.
// Aka, copy factorWeights[1:S, f] into weights.. 
void ERMLinearMultiFactor::copyWeights(bmatrixd &factorWeights, int S, int f, vector<double> &weights) {
  weights.resize(S);
  for (int s=0;s<S;s++) {
    weights[s] = factorWeights(s, f);
  }
}

bool ERMLinearMultiFactor::getFairValue(int cid, double &fv) {
  double factorIV, factorCV, factorRet, factorBeta;
  double sigma = 0.0;
  // Walk through each factor, extracting:
  // - current factor fv.
  // - initial (market open) factor fv.
  // ==> Factor return since open.
  // - Factor beta.
  // And summing factor-return * factor-beta. 
  for (int f = 0; f < _F; f++) {
    if (!_indeces[f]->getFairValue(factorCV) ||
	!_indeces[f]->getInitialFairValue(factorIV)) {
      fv = 1.0;
      return false;
    }
    factorRet = ((factorCV - factorIV)/factorIV);          // e.g. 0.02 for 2% return.
    if (isnan(factorRet)) {
      factorRet = 0.0;
    }
    factorBeta = _factorBetas(cid, f);	                   // e.g. 1.5 for beta of 1.5
    sigma = sigma + factorRet * factorBeta;
  }
  // Uses initial "fair value" of 1.0, assuming no movement in factor indeces.
  fv = 1.0 + sigma;
  return true;
}

bool ERMLinearMultiFactor::getInitialFairValue(int cid, double &fv) {
  fv = 1.0;
  return true;
}
