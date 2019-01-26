#include "OrderStateRecord.h"
#include "HFUtils.h"
#include "DataManager.h"

#include <Markets.h>
#include <Client/lib3/bookmanagement/common/Book.h>
#include <Client/lib3/bookmanagement/common/BookOrder.h>
#include <Client/lib3/bookmanagement/common/BookLevel.h>

#define BUF_SIZE 1024

typedef lib3::BookOrder OT;
const std::string RELEVANT_LOG_FILE = "guess";

OrderStateRecord::~OrderStateRecord() {
}

int OrderStateRecord::snprint(char *s, int n) const {
  return snprintf(s, n, "OrderStateRecord  %i  %i  %i  %i  %i  %i",
		  _id, _q, _p1pulls, _p2pulls, _p2adds, _p2pulls);
}


OrderStateRecord::OrderStateRecord( const OrderUpdate &ou):
  _id(ou.id()),
  side(ou.side()),
  price(ou.price()),
  _tv(TimeVal(0)),
  outv(ou.tv()),
  ecn(ou.ecn()),
  cid(ou.cid()),
  size(ou.size()),
  _q(-1),
  _qshs(-1),
  _dqshs(-1),
  _p1pulls(0),
  _p1fills(0),
  _p2adds(0),
  _p2pulls(0),
  _guessed(false),
  _npts(0),
  _max(-5000),
  _min(5000),
  _sum(0),
  _cancel(OrderCancelSuggestion::NONE),
  _dm(factory<DataManager>::find(only::one))

{
  initRecord(ou);
  // Temporary for debugging
  factory<debug_stream>::pointer  _logPrinter = factory<debug_stream>::get( RELEVANT_LOG_FILE );
  TAEL_PRINTF(_logPrinter.get(), TAEL_ERROR, "Guessed: %ld as %ld",ou.exchangeID(),_refnum);
}


bool OrderStateRecord::initRecord ( const OrderUpdate &ou){
  
  if (ecn==ECN::NYSE)
    return false;
  if (ou.exchangeID()==-1){ // Reference number not set ! Must be in limon trading mode 
    _guessed = true;
    return guessInit();
  }

  _refnum = ou.exchangeID();
  using namespace lib3;
  typedef clite::util::cmp<3> cmp;
  lib3::MarketBook *book = _dm->subBook(ou.ecn());
  Mkt::Side s=ou.side();
  Book<OT>::BookHalfIter it = book->side[ou.side()][ou.cid()]->begin();
  int q=0;
  int qs =0;
  while (it != book->side[s][ou.cid()]->end() && cmp::EQ((*it)->px,ou.price())){
    BookLevel<OT>::OrderList::iterator lit = (*it)->orders.begin();
    while (lit != (*it)->orders.end() && ( (int) (*lit)->refnum != ou.exchangeID())){
      q++;
      qs+=(*lit)->size;
      lit++;
    }
    if (lit != (*it)->orders.end() && ((int) (*lit)->refnum == ou.exchangeID())){ // Found the order in the book
      _q = q+1;
      _qshs = qs;
      _tv = (*lit)->add_time;
      return(true);
    }
    it++;
  }
  _q = -1;
  _qshs = -1;
  _dqshs=0;
  return(false);
  
}


bool OrderStateRecord::guessInit(){
  
  // We didnt find the reference number
  using namespace lib3;
  typedef clite::util::cmp<3> cmp;
  lib3::MarketBook *book = _dm->subBook(ecn);
  Book<OT>::BookHalfIter it = book->side[side][cid]->begin();
  int q=0;
  int qs=0;
  // Find the correct level (in most cases it should be the first, general case for layering support)
  while (it != book->side[side][cid]->end() && (!cmp::EQ((*it)->px,price) ) ){
    it++;
  }
  
  if (it==book->side[side][cid]->end())
    return(false);
  
  BookLevel<OT>::OrderList::iterator lit = (*it)->orders.begin();
  
  // Match the order for size and time (price,ecn already matched)
  
  while (lit != (*it)->orders.end() && ((int) (*lit)->size != size) && (! HFUtils::tsFuzzyEQ((*lit)->add_time,outv) ) ){
    q++;
    qs+=(*lit)->size;
    lit++;
  }
  if (lit!=(*it)->orders.end()){
    // Found the order
    _q = q + 1;
    _qshs = qs;
    _dqshs = 0;
    _tv = (*lit)->add_time;
    _refnum = (*lit)->refnum; // cache our guess of reference number reference number
    return(true);
  }
  // No luck
  _refnum = (uint64_t) -1;
  _q = -1;
  _qshs = -1;
  _dqshs = 0;
  return(false);

}



void OrderStateRecord::reguessQueuePosition( ){
  /* We dont've o->refnum in this case, walk through the list of orders and try to match up our 
   * signature and find the first order that matches us
   */
  
  if (!_guessed)
    return;
  if (_cancel!=OrderCancelSuggestion::NONE)
    return;
  
  int offset = (_q - _p1fills - _p1pulls) - 1;
  if (guessInit())
    _q -= offset;
  return;
}


void OrderStateRecord::applyUpdate(  const DataUpdate &du){
  // Assume du.side = ou.side, du.price = ou.price, du.ecn = ou.ecn
  
  if (du.isBook() && du.size>0 ){
	// GVNOTE: Was earlier _q == (uint64_t) -1, which doesn't make sense, since _q is of type int
    if (_q == -1) {
      // We dont know who we are in the book. could we be du ?
      if (_refnum>0){ // We atleast know our ref number
	if (_refnum == du.id){
	  _tv = du.tv;
	  _q = getMarketOrders(_dm->subBook(du.ecn),du.cid,du.side); // Get the q size from booktools
	}
      }
      else{
	// Try to match signature
	const Order *o = _dm->getOrder(_id);
	if (o){
	if ((du.size == o->size()) && ((du.tv >= (o->confirmed()).tv()))){
	  _tv = du.tv;
	  _q = getMarketOrders(_dm->subBook(du.ecn),du.cid,du.side,du.price); // Get the q size from booktools
	  _qshs = getMarketSize (_dm->subBook(du.ecn),du.cid,du.side,du.price);
	  _dqshs = 0;
	  _refnum = du.id;
	}
	}
      }
      return;
    }
    else
      addBack();
  }
  // GVNOTE: (uint64_t) -1 makes sense here, since it checks if _refnum equals UINT_MAX
  if ((_refnum == (uint64_t) -1) || (_q == -1)) {
    return; //we're not in book yet, wait 
  }
  
  if (!du.isTrade() && du.size<0){
    if (du.id == _refnum)
      reguessQueuePosition();
    if (du.addtv < _tv){
      _dqshs+=du.size;
      pullFront();
    }
    else
      pullBack();
  }
  if (du.isVisibleTrade()){
    if (_guessed && (du.id == _refnum)){ // Need to check if the exec was really  us before we go reguessing our position !
      reguessQueuePosition();
      return;
    }
    _dqshs+=du.size;
    lib3::MarketBook *book = _dm->subBook(du.ecn);
    lib3::Book<OT>::BookHalfIter it = book->side[du.side][du.cid]->begin();
    if (it != book->side[du.side][du.cid]->end() && ((*it)->orders.begin()!=(*it)->orders.end())){
      std::_List_iterator<lib3::BookOrder*> b = (*it)->orders.begin();
      if ((*b)->refnum != du.id)
	Fill();
    }
  }
}

void OrderStateRecord::addEval(double ev){
  _npts++;
  _sum+=ev;
  _max=std::max(ev,_max);
  _min=std::min(ev,_min);
}

int OrderStateRecord::p1shs(){
  return (_qshs-_dqshs);
}
