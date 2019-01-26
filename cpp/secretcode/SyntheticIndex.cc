/*
  Code for on-the-fly calculation of "synthetic" indeces, aka those whose value
    is observed from traded constituents (plus weightings on those constituents),
    but which are themselves not traded.
  Examples include:
  - Barra risk factor indeces.
  - S&P index FV using current prices only.
*/

#include "SyntheticIndex.h"

#include "AlphaSignal.h"

/*************************************************************************
  SyntheticIndex code.
*************************************************************************/
SyntheticIndex::SyntheticIndex() {

}

SyntheticIndex::~SyntheticIndex() {

}

bool SyntheticIndex::constituentStatedMid(DataManager *dm, int cid, double &fv) {
  double bidPx, askPx, midPx;
  size_t bidSize, askSize;
  if (!getMarket(dm->masterBook(), cid, Mkt::BID, 0, &bidPx, &bidSize) ||
      !getMarket(dm->masterBook(), cid, Mkt::BID, 0, &askPx, &askSize)) {
    fv = 0.0;
    return false;
  }
  midPx = (bidPx + askPx);
  fv = midPx;
  return true;
}

/*
  Stated mid, adjusted for alpha.
*/
bool SyntheticIndex::constituentFairValue(DataManager *dm, AlphaSignal *asignal, int cid, double &fv) {
  double alphaEst;
  // Unable to get stated mid from book data.  Return false.
  bool rv = constituentStatedMid(dm, cid, fv);
  if (rv == false) {
    return false;
  }
  // No signal.  Do not adjust fv away from stated-mid, but return true.
  if (asignal == NULL || !asignal->getAlpha(cid, alphaEst)) {
    return true;
  }
  // Adjust stated-mid for signal.  Alpha assumed to apply vs stated-mid.
  fv = fv * (1.0 + alphaEst);
  return true;
}

int SyntheticIndex::constituentStatedMids(DataManager *dm, vector<double> &fvMid, vector<char> &fvFilled) {
  bool valid;
  double mid;
  int ret = 0;
  int sz = dm->cidsize();
  fvMid.resize(sz);
  fvFilled.resize(sz);
  for (int i=0;i<sz;i++) {
    valid = constituentStatedMid(dm, i, mid);
    fvMid[i] = mid;
    fvFilled[i] = (char)valid;
    if (valid) ret++;
  }
  return ret;
}

int SyntheticIndex::constituentFairValues(DataManager *dm, AlphaSignal *asignal, vector<double> &fvMid, vector<char> &fvFilled) {
  bool valid;
  double mid;
  int sz = dm->cidsize();
  int ret = 0;
  fvMid.resize(sz);
  fvFilled.resize(sz);
  for (int i=0;i<sz;i++) {
    valid = constituentFairValue(dm, asignal, i, mid);
    fvMid[i] = mid;
    fvFilled[i] = (char)valid;
    if (valid) ret++;
  }
  return ret;
}

/*************************************************************************
  SyntheticIndexHF code.
*************************************************************************/
SyntheticIndexHF::SyntheticIndexHF(AlphaSignal *asignal, vector<double> &weights) :
  SyntheticIndex(),
  _asignal(asignal),
  _weights(weights),
  _midPrices(weights.size(), 0.0),
  _validPrices(weights.size(), (char)false),
  _validFV(false),
  _fv(0.0),
  _marketOpen(false),
  _initialValidFV(false),
  _initialFV(0.0)
{
  _dm = factory<DataManager>::find(only::one);
  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in SyntheticIndex::SyntheticIndex)" );   

  _dm->add_listener(this);
}

SyntheticIndexHF::~SyntheticIndexHF() {

}

/*
  Process TimeUpdate - just checks for market open/close messages.
*/
void SyntheticIndexHF::update(const TimeUpdate &au) {
  if (au.timer() == _dm->marketOpen()) {
    _marketOpen = true;
    initializePrices();
    aggregateFV();
  } else if (au.timer() == _dm->marketClose()) {
    _marketOpen = false;
  }
}

/*
  Process market data update.
*/
void SyntheticIndexHF::update(const DataUpdate &du) {
  if (!_marketOpen) {
    return;
  }
  getPrice(du.cid);
}

/*
  Query MasterBook for mid price for specified stock.
  Sets _midPrices[i] and _validPrices[i].
  If _validPrices[i] flips from false --> true, may trigger call to
    aggregateFV.
*/
void SyntheticIndexHF::getPrice(int cid) {
  double midPx;
  bool oldValid;
  if (!constituentFairValue(_dm.get(), _asignal, cid, midPx)) {
    _validPrices[cid] = (char)false;
    setFV(false, 0.0);
    return;
  }
  oldValid = _validPrices[cid];
  _validPrices[cid] = true;
  if (oldValid == (char)false) {
    aggregateFV();
  } 
  if (_validFV == true) {
    adjustFV(cid, midPx);
  }
}

/*
  Adjust fair-value based on new price for specified stock.
  Should *only* be called when:
  _validFV is true.
  _validPrices[cid] is true.
  _midPrices[cid] holds an accurate value representing the last known
    mid prices for stock cid (and that value does not represent some 
    error condition).
  Effects:
  - Replaces _midPrices[cid] with new value.
  - Adjusts _fv by delta(mid px) * weight[cid].
*/
void SyntheticIndexHF::adjustFV(int cid, double mid) {
  assert(_validFV == true);
  assert(_validPrices[cid] == (char)true);
  double oldMid = _midPrices[cid];
  double weight = _weights[cid];
  _midPrices[cid] = mid;
  _fv = _fv + weight * (mid - oldMid);
}


/*
  Initialize prices for all known constituents from MasterBook.
*/
void SyntheticIndexHF::initializePrices() {
  int nidx;
  double midPx;

  nidx = _midPrices.size();
  for (int i=0;i<nidx;i++) {
    // cidx of index constituent i.
    if (!constituentFairValue(_dm.get(), _asignal, i, midPx)) {
      _validPrices[i] = (char)false;
    } else {
      _validPrices[i] = (char)true;
      _midPrices[i] = midPx;
    }
  }
}

/*
  Attempt to aggregate prices * weights for all constituents.
*/
bool SyntheticIndexHF::aggregateFV() {
  int nidx;

  nidx = _midPrices.size();
  double tfv = 0.0;
  for (int i=0;i<nidx;i++) {
    if (_validPrices[i] == (char)false) {
      _validFV = false;
      _fv = 0.0;
      return false;
    }
    tfv += _weights[i] * _midPrices[i];
  }
  setFV(true, tfv);
  return true;
}

void SyntheticIndexHF::setFV(bool valid, double fv) {
  if (_initialValidFV == false && valid == true) {
    _initialValidFV = true;
    _initialFV = fv;
  }

  _validFV = valid;
  _fv = fv;
}

bool SyntheticIndexHF::getFairValue(double &fv) {
  if (_validFV == false) {
    fv = 0.0;
    return false;
  }
  fv = _fv;
  return true;
}

bool SyntheticIndexHF::getInitialFairValue(double &fv) {
  if (_initialValidFV == false) {
    fv = 0.0;
    return false;
  }
  fv = _initialFV;
  return true;
}
