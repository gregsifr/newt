#include "MarketImpactModel.h"
#include "KFRTSignal.h"

/********************************************************************
  MarketImpactModel code.
********************************************************************/
MarketImpactModel::MarketImpactModel() {

}

MarketImpactModel::~MarketImpactModel() {

}

bool MarketImpactModel::marketImpactOutstanding(int cid, ECN::ECN ecn, int size, double price,
						Mkt::Side side, int timeout, bool invisible, MarketImpactEstimate &fv) {
  fv.setImpact(cid, size, Mkt::BUY, false, 0.0, 0.0);
  return false;
}

bool MarketImpactModel::marketImpactFill(int cid, ECN::ECN ecn, int size, double price,
						Mkt::Side side, int timeout, bool invisible, MarketImpactEstimate &fv) {
  fv.setImpact(cid, size, Mkt::BUY, false, 0.0, 0.0);
  return false;
}

/********************************************************************
  TakeLiquidityMarketImpactModel code.
********************************************************************/
TakeLiquidityMarketImpactModel::TakeLiquidityMarketImpactModel() :
  MarketImpactModel()
{
  _sig = factory<ETFKFRTSignal>::get(only::one);
  if( !_sig )
    throw std::runtime_error( "Failed to get ETFKFRTSignal from factory (in TakeLiquidityMarketImpactModel::TakeLiquidityMarketImpactModel)" ); 
 
}
TakeLiquidityMarketImpactModel::~TakeLiquidityMarketImpactModel() {

}

bool TakeLiquidityMarketImpactModel::marketImpactOutstanding(int cid, ECN::ECN ecn, int size, double price,
    Mkt::Side side, int timeout, bool invisible, MarketImpactEstimate &fv) {
  fv.setImpact(cid, size, Mkt::BUY, false, 0.0, 0.0);
  return false;
}

/*
  Notes:
  - Current version of KFRTSignal truncates trade sizes down to nearest round-lot.
    This results in it ignoring odd lot trades.
    This seems ok for fv estimation, but can produce strange looking answers for
      mi estimation.
    Here we attempt to correct for that, assuming that market impact *per trade* grows with
      sqrt of volume.
    See e.g. "Single Curve Collpase of the Price Impact Function for the New York Stock Exchange"
      by Lillo, Farmer, and Mantegna for empirical semi-justification for this assumption.
*/
bool TakeLiquidityMarketImpactModel::marketImpactFill(int cid, ECN::ECN ecn, int size, double price,
    Mkt::Side side, int timeout, bool invisible, MarketImpactEstimate &fv) {
  if (size < 0) {
    fv.setImpact(cid, 0, Mkt::BUY, false, 0.0, 0.0);
    return false;
  }
  int usize = std::max(size, 100);
  if (!_sig->marketImpactFill(cid, ecn, usize, price, side, timeout, invisible, fv)) {
    fv.setImpact(cid, 0, Mkt::BUY, false, 0.0, 0.0);
    return false;
  }
  if (size == usize) {
    return true;
  }
  double scale = (double)size/(double)usize;
  fv.scaleSize(scale, 1.0, pow(scale, 0.5));    // size, temporary impact, permanent impact
  return true;
}
