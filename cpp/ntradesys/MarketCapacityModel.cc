#include "MarketCapacityModel.h"
#include "HFUtils.h"

#include "DataManager.h"

/*******************************************************
  TAGlobalMarketCapacityModel
*******************************************************/
TAGlobalMarketCapacityModel::TAGlobalMarketCapacityModel( double pRate, int minShs, int maxShs ) 
  : GlobalMarketCapacityModel(),
    _pRate(pRate),
    _minShs(minShs),
    _maxShs(maxShs),
    _qst(factory<QSzTracker>::get(only::one)) {}

bool TAGlobalMarketCapacityModel::globalCapacity( int cid, Mkt::Side side, double px, int &fv ) {
  double qsz;
  if (!_qst->qSz(cid, qsz)) {
    // Default value: 200 shares at a time.
    fv = 200;
    return true;
  } 
  // Convert queue size --> capacity.  As a rough heuristic, we try:
  // - 10% of trailing avg queue size.
  // - rounded to nearest round lot.
  // - floored at 200 shares.
  // - cielinged at 2000 shares.
  qsz = qsz * _pRate;                              // 10% of queue size.
  double nlots = qsz / 100.0;
  int ret = HFUtils::roundDtoI(nlots) * 100;

  ret = std::min(ret, _maxShs);   
  ret = std::max(ret, _minShs);

  fv = ret;
  return true;
}
