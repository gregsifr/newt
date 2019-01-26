/*
  Simple DataManager/UpdateListener based widgets + program for testing 
    *unconditional* alpha signals.
  Operates on a single fixed (pre-specified) predictive horizion.
  Operates on 1...N alpha signals.
  Generates big log holding:
  - Log time.
  - Ticker.
  - Obs Time.
  - 1...N signals values.
  - Actual return over time horizon.  
*/

#include "AlphaSignalDumper.h"

/****************************************************************************
  AlphaDumpObservation
****************************************************************************/
AlphaDumpObservation::AlphaDumpObservation(int cid, const char *ticker, TimeVal &startTV, int periodMS, 
					   int nsignals, double bidPx, double askPx) 
  :
  _cid(cid),
     _ticker(ticker),
     _startTV(startTV),
     _periodMS(periodMS),
     _signalValues(nsignals, 0.0),
     _ret(0,0),
     _bidPx(bidPx),
     _askPx(askPx)
{

}

AlphaDumpObservation::~AlphaDumpObservation() {

}

void AlphaDumpObservation::associateSignals(vector<double> &signals) {
  _signalValues = signals;
}

void AlphaDumpObservation::associateReturn(double ret) {
  _ret = ret;
}

bool AlphaDumpObservation::calculateReturn(int cid, DataManager *dm, double &fv) {
  double cmid, omid;
  if (!HFUtils::bestMid(dm, cid, cmid)) {
    return false;
  }
  omid = (_bidPx + _askPx) / 2.0;
  fv = ((cmid - omid)/omid);
  return true;
}

int AlphaDumpObservation::snprint_desc(char *buf, int n) {
  int left = n;
  int ret = 0;
  int tn;
  
  tn += snprintf(buf, left, "cid\tsymbol\ttbidpx\ttaskpx\t");
  ret += tn;
  left -= tn;
  for (int i=0;i<_signalValues.size();i++, left<0) {
    tn = snprint(buf, n, "signal%i\t");
    ret += tn;
    left -= tn;
  }
  if (left >= 1) {
    tn = snprint(buf, n, "\n");
    ret += tn;
    left -= tn;
  }
  return ret;
}

int AlphaDumpObservation::snprint(char *buf, int n) {
  int left = n;
  int ret = 0;
  int tn;
  
  tn += snprintf(buf, left, "%i\%s\t%f\t%f\t", _cid, _ticker.c_str(), _bidPx, _askPx);
  ret += tn;
  left -= tn;
  for (int i=0;i<_signalValues.size();i++, left<0) {
    tn = snprint(buf, n, "%f\t", _signalValues[i]);
    ret += tn;
    left -= tn;
  }
  if (left >= 1) {
    tn = snprint(buf, n, "\n");
    ret += tn;
    left -= tn;
  }
  return ret;  
}
