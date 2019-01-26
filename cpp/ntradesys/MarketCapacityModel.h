/*
  Interface definition for a widget that is used to calculate market "capacity", 
    aka the number of non-marketable shares that we think it is safe to place 
    at any point in time.

  Initial version is very simple, and probably not the correct/complete interface 
    specification.  It assumes:
*/

#ifndef __MARKETCAPACITYMODEL_H__
#define __MARKETCAPACITYMODEL_H__

#include <string>
using std::string;

#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>
using namespace clite::util;

#include "QSzTracker.h"

class DataManager;

/**
 * "Global" Market Capacity Model.  In this context, "global" means that the 
 *  desired capacity applies in total, across all ECNs.  
 */
class GlobalMarketCapacityModel {
 public:
  GlobalMarketCapacityModel() {};
  virtual ~GlobalMarketCapacityModel() {};

  // Query model for capacity.
  virtual bool globalCapacity(int cid, Mkt::Side side, double px, int &fv) = 0;
};

/**
 * "Local" market capacity model.  In this context, "local" means that the 
 *  desired capacity applies only to a single ECN.
*/
class LocalMarketCapacityModel {
 public:
  LocalMarketCapacityModel() {};
  virtual ~LocalMarketCapacityModel() {};

  // Query model for capacity.
  virtual bool localCapacity(int cid, ECN::ECN ecn, Mkt::Side side, double px, int &fv) = 0; 
};

/*
  "Static" global market capacity model. Always returns same number.
*/
class StaticGlobalMarketCapacityModel : public GlobalMarketCapacityModel {
  int _sv;
 public:
  StaticGlobalMarketCapacityModel( int sv ) : _sv(sv) {}
  virtual ~StaticGlobalMarketCapacityModel() {};

  // Query model for capacity.
  virtual bool globalCapacity(int cid, Mkt::Side side, double px, int &fv) { fv=_sv; return true; }
};

/*
  Wrapper around a Tracker component that uses a trailing exp decay average of
    top-level queue size to determine global capacity.
  Wrapper servers 2 purposes in code:
  - To floor and cieling market capacity model calculated from trailing average.
  - To provide a known interface while Amrit writes tracker code.
*/
class TAGlobalMarketCapacityModel : public GlobalMarketCapacityModel {
  double _pRate;                  // What % of volume to target.  Use e.g. 0.01 for 10%.
  int _minShs;                    // min order size (in shares).
  int _maxShs;                    // max order size (in shares).
  factory<QSzTracker>::pointer _qst;
 public:
  TAGlobalMarketCapacityModel( double pRate, int minShs, int maxShs );
  virtual ~TAGlobalMarketCapacityModel() {};

  bool globalCapacity(int cid, Mkt::Side side, double px, int &fv);
};


#endif   // __MARKETCAPACITYMODEL_H__
