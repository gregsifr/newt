/*
  Simple widget that prints trade ticks for multiple stock, over some time horizon.
  Format:
  - 1 line for evey trade.
  - TRADE cid  symbol  ecn  side  size  px  time(in hh.mm.dd.ss.usec).

  Notes:
  - Prints using stdio, not asynch logger.
  - Not for use in live trading.
*/
#ifndef __TRADETICKPRINTER_H__
#define __TRADETICKPRINTER_H__

#include <cl-util/factory.h>
using namespace clite::util;

#include "DataManager.h"

#include "c_util/Time.h"
using trc::compat::util::TimeVal;

class TradeTickPrinter : public TimeHandler::listener, public MarketHandler::listener {
 protected:
  factory<DataManager>::pointer       _dm;
  bool _marketOpen;
  bool populateCBidAsk(int cid, double &bid, double &ask);
 public:
  TradeTickPrinter();
  virtual ~TradeTickPrinter();

  virtual void update(const DataUpdate &du); 
  virtual void update(const TimeUpdate &au);

  static void printTradeTick(DataManager *dm, const DataUpdate &du, double cbid, double cask, bool addEndl);
  static void printTradeTick(DataManager *dm, int cid, ECN::ECN ecn, Mkt::Side side, int size, double price,
			     const TimeVal &tv, double cbid, double cask, bool addEndl);
};

#endif  //  __TRADETICKPRINTER_H__
