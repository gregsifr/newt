#ifndef __SPREADDTRACKER_H__
#define __SPREADDTRACKER_H__

#include "CircBuffer.h"
#include "DataManager.h"
#include "c_util/Time.h"
using trc::compat::util::TimeVal;

#include "StocksState.h"

#include <cl-util/table.h>
#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>
#include <cl-util/float_cmp.h>

using namespace clite::util;

class SpreadTracker : public TimeHandler::listener {
  
 protected:
  /*
    Internal state:
  */
  factory<DataManager>::pointer _dm;
  factory<debug_stream>::pointer _ddebug;
  factory<StocksState>::pointer       _stocksState;
  vector< CircBuffer<double> *> _spdbuffer;
  Timer _sample;
 public:
  SpreadTracker();
  virtual ~SpreadTracker();
  virtual void update(const TimeUpdate &t);
  bool getAvgspd(int cid,double *aspd);
  
};

#endif
