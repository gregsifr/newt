/*
  Simple widget that prints trade ticks for multiple stock, over some time horizon.
  Format:
  - 1 line for evey trade.
  - cid  symbool  ecn  side  size  px  time(in hh.mm.dd.ss.usec).

  Notes:
  - Prints using stdio, not asynch logger.
  - Not for use in live trading.
*/

#include "TradeTickPrinter.h"
#include "DataManager.h"
#include "HFUtils.h"

using trc::compat::util::DateTime;

TradeTickPrinter::TradeTickPrinter() :
  _marketOpen(false) 
{
  std::cout << "TradeTickPrinter::TradeTickPrinter called" << std::endl;
  _dm = factory<DataManager>::find(only::one);
  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in JoinQueueComponent::JoinQueueComponent)" );
  _dm->add_listener(this);
}

TradeTickPrinter::~TradeTickPrinter() {
  
}

bool TradeTickPrinter::populateCBidAsk(int cid, double &bid, double &ask) {
  if (!HFUtils::bestPrice(_dm.get(), cid, Mkt::BID, bid) ||
      !HFUtils::bestPrice(_dm.get(), cid, Mkt::ASK, ask)) {
    return false;
  }
  return true;
}

void TradeTickPrinter::update(const DataUpdate &du) {
  double cbid, cask;
  if (!du.isTrade()) {
    return;
  }

  if (!_marketOpen) {
    return;
  }

  if (!populateCBidAsk(du.cid, cbid, cask)) {
    return;
  }

  printTradeTick(_dm.get(), du, cbid, cask, true);
}

void TradeTickPrinter::update(const TimeUpdate &au) {
  if (au.timer() == _dm->marketOpen()) {
    _marketOpen = true;
  } else if (au.timer() == _dm->marketClose()) {
    _marketOpen = false;
  }
}

void TradeTickPrinter::printTradeTick(DataManager *dm, const DataUpdate &du, double cbid, double cask, bool addEndl) {
  DateTime dt(du.tv);
  std::cout << "TRADE " <<  du.cid << " " << dm->symbol(du.cid) << " " << ECN::desc(du.ecn) << " ";
  std::cout << Mkt::SideDesc[du.side] << " " << du.size << " " << du.price <<  " ";
  std::cout << dt.getfulltime() << " ";
  std::cout << cbid << " " << cask << " ";
  if (addEndl) {
    std::cout << std::endl; 
  }
}

void TradeTickPrinter::printTradeTick(DataManager *dm, int cid, ECN::ECN ecn, Mkt::Side side, 
				      int size, double price, const TimeVal &tv,
				      double cbid, double cask, bool addEndl) {
  DateTime dt(tv);
  std::cout << "TRADE " <<  cid << " " << dm->symbol(cid) << " " << ECN::desc(ecn) << " ";
  std::cout << Mkt::SideDesc[side] << " " << size << " " << price <<  " ";
  std::cout << dt.getfulltime() << " ";
  std::cout << cbid << " " << cask << " ";
  if (addEndl) {
    std::cout << std::endl; 
  }
}
