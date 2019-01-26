#include "MOCComponent.h"
#include "Markets.h"
#include <iostream>
#include <fstream>
#include <string>

const int DEFAULT_TIMEOUT = 3600*8;
using trc::compat::util::TimeVal;
// Cutoff time before clsoe for sending MOC orders
const TimeVal NYSE_CUTOFF = TimeVal(15*60,0); // 15 minutes
const TimeVal ISLD_CUTOFF = TimeVal(10*60,0); // 10 minutes

MOCComponent::MOCComponent() :
  TradeLogicComponent(),
  _stocksState( factory<StocksState>::get(only::one)),
  exchanges_filename ("exchanges"),
  tic2exch(exchanges_filename) {
	if(tic2exch.empty()) {
		throw std::runtime_error("No mappings found in the exchanges file, for mapping tickers to exchanges");
	}
	dbg = clite::util::factory<clite::util::debug_stream>::get(std::string("dataman"));
//	TAEL_TPRINTF(dbg.get(), &NYSE_CUTOFF.tv, TAEL_INFO, "Priting NYSE_CUTOFF time");
//	TAEL_TPRINTF(dbg.get(), &ISLD_CUTOFF.tv, TAEL_INFO, "Priting ISLD_CUTOFF time");
	TAEL_TPRINTF(dbg.get(), &(_dm->timeValMktClose()).tv, TAEL_INFO, "Priting dm->MarketClose time");
}

bool MOCComponent::isPastCutoff(ECN::ECN ecn) {
//	TAEL_TPRINTF(dbg.get(), &(TimeVal::now + NYSE_CUTOFF).tv, TAEL_INFO, "Priting now + NYSE_CUTOFF time");
//	TAEL_TPRINTF(dbg.get(), &(_dm->timeValMktClose()).tv, TAEL_INFO, "Priting dm->MarketClose time");
	switch (ecn) {
	    case ECN::NYSE:
	    	if ((TimeVal::now + NYSE_CUTOFF) < _dm->timeValMktClose()) return false;
	    	break;
	    case ECN::ISLD:
	    	if ((TimeVal::now + ISLD_CUTOFF) < _dm->timeValMktClose()) return false;
	    	break;
	    default:
	    	break;
	}
	return true;
}

void MOCComponent::suggestOrderPlacements( int cid, Mkt::Side side, int tradeLogicId,
				       int numShares, double priority,
				       vector<OrderPlacementSuggestion> &suggestions ) {
	suggestions.clear();
    SingleStockState *ss = _stocksState->getState(cid);
	if (numShares <= 0) return;

	int sharesUsed = _centralOrderRepo->totalOutstandingSize(cid, side, this->componentId(), tradeLogicId);
	int sharesLeft = numShares - sharesUsed;
	if (sharesLeft <= 0) return;
	std::cout << "In MOCComponent::suggestOrderPlacements. About to generate order suggestions for "
			  << _dm->symbol(cid) << " " << sharesLeft << " " << side << std::endl;
    double price = 0.0;
    file_table<ParamReader>::iterator it;
    it = tic2exch.find(_dm->symbol(cid));

    if( it != tic2exch.end()) {
      ECN::ECN ecn = ECN::ECN_parse(it->second._fields[1].c_str());
      switch (ecn) {
        case ECN::NYSE:
        case ECN::ISLD:
        	break;
        case ECN::AMEX:
        case ECN::ARCA:
        	ecn = ECN::NYSE;
        	break;
        default:
        	std::cout << "ERROR: invalid exchange " << it->second._fields[1].c_str()
        	          << " for symbol " << _dm->symbol(cid) << std::endl;
        	return;
      }

      if (isPastCutoff(ecn)) {
    	  std::cout << "ERROR: trying to place order after " << ECN::desc(ecn) << " cutoff time" << std::endl;
    	  return;
      }

      OrderPlacementSuggestion ops(cid, ecn, side, sharesLeft, price, DEFAULT_TIMEOUT,
    		 			    tradeLogicId, _componentId, allocateSeqNum(), OrderPlacementSuggestion::MKT_ON_CLOSE,
    		 			    _dm->curtv(), ss->bestPrice(Mkt::BID), ss->bestPrice(Mkt::ASK), priority);
      suggestions.push_back(ops);
    } else {
    	std::cout << "ERROR: could not find the exchange for symbol " << _dm->symbol(cid) << std::endl;
    	return;
    }

 }
