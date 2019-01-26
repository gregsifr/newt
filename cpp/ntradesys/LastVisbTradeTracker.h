#ifndef __LASTTRADETRACKER_H__
#define __LASTTRADETRACKER_H__

#include <map>
using std::map;

#include <cl-util/factory.h>
#include <clite/message.h>
#include "DataManager.h"

#include "Markets.h"
#include "DataUpdates.h"

/*
  Simple component.  Keeps track of last *trade* for each stock x ecn x side, with some 
    some simple filter options for which types of Trades to keep/discard, which
    e.g. allow configuration of keeping track of visible vs invisible trades.
  Should be kept synchronized with the StocksState and TradeLogic state, and thus designed to 
    be populated on wakeups, as opposed to on every incoming packet.
  To Do:
    Initially only configured to record data for 1 ECN (ISLD).
    Extended July 2009 to include data for multiple ECNs.
*/
typedef bool (*LVT_FF)(const DataUpdate &du);

typedef vector<DataUpdate> DUVector;

class LastVisbTradeTracker : public MarketHandler::listener {
 protected:
  LVT_FF _filterFunc;
  
  // Vectors for bid & ask side.
  // Each should hold ECN::ECN_size vectors of DataUpdates.
  // Each of those vectors should be of length cidsize.
  vector<DUVector> _bidVS;
  vector<DUVector> _askVS;

  // Get a pointer to the DUVector corresponding to specified 
  //   ECN, SIDE.  Can return NULL iff no such vector present.
  // As these vectors are stored inside _bidVS, and _askVS, such pointers
  //   should not be preserced across any operation that may cause
  //   reallocation of _bidVS or _askVS.
  DUVector *getVector(ECN::ECN ecn, Mkt::Side side);

  virtual void initialize(DUVector &duv);
  virtual bool isDummyTrade(const DataUpdate &du);
  // Should we include/keep this data update (true), or discard it (false).
  virtual bool include(const DataUpdate &du);

  virtual void update( const DataUpdate& du );
 public:
  LastVisbTradeTracker();
  LastVisbTradeTracker(int cidsize, LVT_FF filterFunc);

  virtual ~LastVisbTradeTracker() {};

  // Get the last corresponding update, or null if no such update is found.
  // Note that this object pointer is only guaranteed to be good until the
  //   next potential insert/delete into _map, aka until the next data-update
  virtual DataUpdate *lastTrade(int cid, ECN::ECN ecn, Mkt::Side side);
};

// Default filter function for LastVisbTradeTracker
// Checks that du is a visible trade on ISLD.
bool defaultLVTFilter(const DataUpdate &du);

#endif   // __LASTTRADETRACKER_H__
