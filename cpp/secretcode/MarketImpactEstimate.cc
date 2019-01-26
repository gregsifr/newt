#include "MarketImpactEstimate.h"

/********************************************************************
  MarketImpactEstimate code.
********************************************************************/
MarketImpactEstimate::MarketImpactEstimate() 
  :
  _cid(-1),
  _size(0),
  _dir(Mkt::BUY),
  _goodEstimate(false),
  _temporaryImpact(0.0),
  _permanentImpact(0.0)
{

}

MarketImpactEstimate::~MarketImpactEstimate() {

}

void MarketImpactEstimate::setImpact(int cid, int size, Mkt::Trade dir, bool goodEstimate, 
				double ti, double pi) {
  _cid = cid;
  _size = size;
  _dir = dir;
  _goodEstimate = goodEstimate;
  _temporaryImpact = ti;
  _permanentImpact = pi;
}

int MarketImpactEstimate::snprint(char *s, int n) const {
  int tn;
  tn = snprintf(s, n, "CID%i  SIZE%i  DIR%s  GOOD%i  TI%.10f  PI%.10f",
		_cid, _size, Mkt::TradeDesc[_dir], (int)_goodEstimate, _temporaryImpact, _permanentImpact);
  return tn;
}

void MarketImpactEstimate::scaleSize(double sizeScale, double tiScale, double piScale) {
  _size = (int)round((double)_size * sizeScale);
  _temporaryImpact *= tiScale;
  _permanentImpact *= piScale;
}
