/**
 *  OpenTracker.h
 * Simple widget that keeps track of whether stocks in population set have "opened" for trading.  
 * - NASDAQ and ARCA listed stocks are assumed to be "open" at any time after the US market open and before the US market close.
 * - NYSE and AMEX listed stocks are assumed to be "open" only after having been specifically opened by the relevant specialist,
 *   (and before the US market close). "opening" by the specialist is proxied by any trading volume appearing on the primary exchange.
 *
 * Mod History:
 */

#ifndef _OPENNYSEAMEXTRACKER_H_
#define _OPENNYSEAMEXTRACKER_H_

#include <cl-util/factory.h>
using namespace clite::util;

#include "DataManager.h"
#include "ExchangeTracker.h"
#include "HFUtils.h"

class OpenTracker : public MarketHandler::listener, public TimeHandler::listener {

    clite::util::factory<DataManager>::pointer     _dm;
    clite::util::factory<ExchangeTracker>::pointer _exchangeT;
  vector<bool>                      _nyse;
  vector<bool>                      _amex;
  bool                              _marketIsOpen;

  vector<TimeVal>                   _nyseOpenTV;
  vector<TimeVal>                   _amexOpenTV;
  TimeVal                           _marketOpenTV;

public:
  OpenTracker();
  
  void update ( const DataUpdate &du ); 
  void update ( const TimeUpdate &t );
  
  bool hasOpened( int cid, ECN::ECN ecn ) const;
  bool hasOpenedOnPrimaryExchange( int cid ) const { return hasOpened( cid, _exchangeT->getExchange(cid) ); }

  // As hasOpened, but also fills in stock-specific open-time estimate.
  bool openTV(int cid, ECN::ECN ecn, TimeVal &fv) const;
  bool openTV(int cid, TimeVal &fv) const { return openTV( cid, _exchangeT->getExchange(cid), fv); }
};

#endif
