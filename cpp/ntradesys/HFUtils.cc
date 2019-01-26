#include "HFUtils.h"
#include <cl-util/float_cmp.h>

using namespace clite::util;

#include "AlphaSignal.h"


bool HFUtils::moreAggressive(Mkt::Side side, double px1, double px2) {
  if( side == Mkt::BID )        return cmp<4>::GT(px1, px2);
  else /*( side == Mkt::ASK )*/ return cmp<4>::LT(px1, px2);

}

bool HFUtils::geAggressive(Mkt::Side side, double px1, double px2) {
  if( side == Mkt::BID )        return cmp<4>::GE(px1, px2);
  else /*( side == Mkt::ASK )*/ return cmp<4>::LE(px1, px2);
}

bool HFUtils::lessAggressive(Mkt::Side side, double px1, double px2) {
  if( side == Mkt::BID )        return cmp<4>::LT(px1, px2);
  else /*( side == Mkt::ASK )*/ return cmp<4>::GT(px1, px2);
}

bool HFUtils::leAggressive(Mkt::Side side, double px1, double px2) {
  if( side == Mkt::BID )       return cmp<4>::LE(px1, px2);
  else/*( side == Mkt::ASK )*/ return cmp<4>::GE(px1, px2);
}

double HFUtils::makeMoreAggressive(Mkt::Side side, double price, double increment) {
  return( side == Mkt::BID ? price + increment : price - increment );
}

double HFUtils::makeLessAggressive(Mkt::Side side, double price, double increment) {
  return( side == Mkt::BID ? price - increment : price + increment );
}

// Given a price px, find the closest even increment of mpv that is not more aggresive
//  than px.
double HFUtils::roundLEAggressive(Mkt::Side side, double px, double mpv) {
  double ret;
  double numMPVS = px / mpv;
  if (side == Mkt::BID) {
    ret = floor(numMPVS) * mpv;
  } else {
    ret = ceil(numMPVS) * mpv;
  }
  return ret;
}

// Given a price px, find the closest even increment of mpv that is not less aggresive
//  than px.
double HFUtils::roundGEAggressive(Mkt::Side side, double px, double mpv) {
  double ret;
  double numMPVS = px / mpv;
  if (side == Mkt::ASK) {
    ret = floor(numMPVS) * mpv;
  } else {
    ret = ceil(numMPVS) * mpv;
  }
  return ret;
}

double HFUtils::roundClosest(double px, double mpv) {
  double ret;
  double numMPVS = px / mpv;
  ret = round(numMPVS) * mpv;
  return ret;
}

// Given a price px, return the closest even increment of mpv that is 
//   strictly more aggressive than limit.
double HFUtils::pushToLessAggressive(Mkt::Side side, double price, double limit, double mpv) {
  // Already less aggressive.  Just round to closest multiple of mpv.
  if (lessAggressive(side, price, limit)) {
    return roundClosest(price, mpv);
  }
  // Not less aggressive.  Use limit less 1 MPV.
  return makeLessAggressive(side, limit, mpv);
}

// Given a price px, return the closest even increment of mpv that is 
//   strictly more aggressive than limit.
double HFUtils::pushToMoreAggressive(Mkt::Side side, double price, double limit, double mpv) {
  // Already more aggressive.  Just round to closest multiple of mpv.
  if (moreAggressive(side, price, limit)) {
    return roundClosest(price, mpv);
  }
  // Not more aggressive.  Use limit plus 1 MPV.
  return makeMoreAggressive(side, limit, mpv);
}

// Given a price px, return the closest even increment of mpv that is 
//   not more aggressive than limit.
double HFUtils::pushToLEAggressive(Mkt::Side side, double price, double limit, double mpv) {
  // Already not more aggressive.  Just round to closest multiple of mpv.
  if (leAggressive(side, price, limit)) {
    return roundClosest(price, mpv);
  }
  return limit;
}

// Given a price px, return the closest even increment of mpv that is 
//   not less aggressive than limit.
double HFUtils::pushToGEAggressive(Mkt::Side side, double price, double limit, double mpv) {
  // Already not less aggressive.  Just round to closest multiple of mpv.
  if (geAggressive(side, price, limit)) {
    return roundClosest(price, mpv);
  }
  return limit;
}


// is tv1 >= (tv2 - epsilon).
bool HFUtils::tsFuzzyGE(const TimeVal &tv1, const TimeVal &tv2) {
  int tv1ms = (tv1.sec() * 1000) + tv1.msec();
  int tv2ms = (tv2.sec() * 1000) + tv2.msec();
  return (tv1ms >= (tv2ms - 1));
}

// is tv1 <= (tv2 + epsilon).
bool HFUtils::tsFuzzyLE(const TimeVal &tv1, const TimeVal &tv2) {
  int tv1ms = (tv1.sec() * 1000) + tv1.msec();
  int tv2ms = (tv2.sec() * 1000) + tv2.msec();
  return (tv1ms <= (tv2ms + 1));
}

bool HFUtils::tsFuzzyEQ(const TimeVal &tv1, const TimeVal &tv2) {
  bool r1 = tsFuzzyGE(tv1, tv2);
  bool r2 = tsFuzzyLE(tv1, tv2);
  bool ret = (r1 && r2);
  return ret;
}

// tv1 - tv2, in milliseconds.
double HFUtils::milliSecondsBetween(const TimeVal &tv1, const TimeVal &tv2) {
  long secsDiff = (tv2.sec() - tv1.sec());
  long microSecsDiff = (tv2.usec() - tv1.usec());
  return (secsDiff * 1000) + (microSecsDiff / 1000.0);
}

bool HFUtils::getTradeableMarket(DataManager*dm, int cid, Mkt::Side side, int level,
				 size_t minSize, double *price, size_t *size) {
  return getTradableMarket(dm->masterBook(), cid, side, level, minSize, price, size);
}

bool HFUtils::bestPrice(DataManager *dm, int cid, Mkt::Side side, double &px) {
  double tmpPx;
  size_t tmpSz;
  bool ret = getTradableMarket(dm->masterBook(), cid, side, (int)0, (size_t)100, &tmpPx, &tmpSz);
  if (ret == true) {
    px = tmpPx;
  }
  return ret;
}

bool HFUtils::bestSize(DataManager *dm, int cid, Mkt::Side side, size_t &sz) {
  double tmpPx;
  size_t tmpSz;
  bool ret = getTradableMarket(dm->masterBook(), cid, side, (int)0, (size_t)100, &tmpPx, &tmpSz);
  if (ret == true) {
    sz = tmpSz;
  }
  return ret;
}

bool HFUtils::bestMid(DataManager *dm, int cid, double &px, AlphaSignal *fvSignal) {
  double bidPx, askPx, alpha;
  if (!bestPrice(dm, cid, Mkt::BID, bidPx) || !bestPrice(dm, cid, Mkt::ASK, askPx)) {
    return false;
  }
  px = (bidPx + askPx)/2.0; 
  if (fvSignal != NULL && fvSignal->getAlpha(cid, alpha)) {
    px = px * (1.0 + alpha);
  }
  return true;
}

void HFUtils::bestMids(DataManager *dm, double defaultValue, vector<double> &fv) {
  double px;
  fv.resize(dm->cidsize());
  for (unsigned int i = 0; i < fv.size(); i++) {
    if (bestMid(dm, i, px)) {
      fv[i] = px;
    } else {
      fv[i] = defaultValue;
    }
  }
}

void HFUtils::bestPrices(DataManager *dm, Mkt::Side side, double defaultValue, vector<double> &fv) {
  double px;
  fv.resize(dm->cidsize());
  for (unsigned int i = 0; i < fv.size(); i++) {
    if (bestPrice(dm, i, side, px)) {
      fv[i] = px;
    } else {
      fv[i] = defaultValue;
    }
  }
}

void HFUtils::bestMids(DataManager *dm, vector<double> &defaultValues, vector<double> &fv, AlphaSignal *fvSignal) {
  double px;
  fv.resize(dm->cidsize());
  for (unsigned int i = 0; i < fv.size(); i++) {
    if (bestMid(dm, i, px, fvSignal)) {
      fv[i] = px;
    } else {
      fv[i] = defaultValues[i];
    }
  }
}

void HFUtils::bestPrices(DataManager *dm, Mkt::Side side, vector<double> &defaultValues, vector<double> &fv) {
  double px;
  fv.resize(dm->cidsize());
  for (unsigned int i = 0; i < fv.size(); i++) {
    if (bestPrice(dm, i, side, px)) {
      fv[i] = px;
    } else {
      fv[i] = defaultValues[i];
    }
  }
}

void HFUtils::setupLogger(tael::Logger *dbg, tael::LoggerDestination *dest) {
  dbg->addDestination(dest);
}


/// Divide an order of total size <numShares> into N orders of size chunkSize, plus, possibly, one remainder
///  Should populate chunks with vector of chosen sizes.
void HFUtils::chunkOrderShares(int numShares, int chunkSize, vector<int> &chunks) {
  chunks.clear();
  while(numShares > 0) {
    int sz = std::min(numShares, chunkSize);
    chunks.push_back(sz);
    numShares -= sz;
  }
}
