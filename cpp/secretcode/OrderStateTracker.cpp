#include "OrderStateTracker.h"
#include "HFUtils.h"
#include "DataManager.h"

#include <Markets.h>
#include <Client/lib3/bookmanagement/common/Book.h>
#include <Client/lib3/bookmanagement/common/BookOrder.h>
#include <Client/lib3/bookmanagement/common/BookLevel.h>

#define BUF_SIZE 1024

using std::pair;
typedef lib3::BookOrder OT;

/***
 * This assumes we are using it for ISLD
 * We rely on the ordering of trade and book updates
 ***/


SOBOrderRepo::SOBOrderRepo(int tradeLogicId)
  :   _tradeLogicId(tradeLogicId),
      _dm( factory<DataManager>::find(only::one)),
      _centralOrderRepo( factory<CentralOrderRepo>::get(only::one) ),
      _imbt( factory<ImbTracker>::get(only::one) ),
      _spdt( factory<SpreadTracker>::get(only::one ) ),
      _stocksState( factory<StocksState>::get(only::one) ),
      _logPrinter( factory<debug_stream>::get(std::string("trader")) ),
      _fwdTracker(factory<ForwardTracker>::get(only::one)),
      _modelTracker( factory<ModelTracker>::get(only::one))
{
  _mapTable.resize( _dm->cidsize() );
  _dm->add_listener( this );
  _priceCache[0].resize( _dm->cidsize());
  _priceCache[1].resize( _dm->cidsize());
  _lastTrade.resize( _dm->cidsize(), (uint64_t) 0);
  
}


void SOBOrderRepo::update( const OrderUpdate& ou ) {
  /* Delete the order if its done and we are tracking it */
  /* Add the order if we get a confirmed */
  
  if ((ou.action()==Mkt::CONFIRMED) && (ou.ecn()==ECN::ISLD)) {
    const OrderRecord* cord = _centralOrderRepo->getOrderRecord( ou.id() );
    if (cord==NULL)
      return;
    if (cord->componentId() == _tradeLogicId){
      addOrderRecord( ou );
    }
    return;
  }
  if( ou.state()!=Mkt::DONE )
    return;
  //TAEL_PRINTF(_logPrinter, TAEL_ERROR, "Done Order %s %s %d",_dm->symbol(ou.cid()),Mkt::OrderActionDesc[ou.action()],ou.id());
  //char buffer[BUF_SIZE];
  map<int,OrderStateRecord>::iterator it = _mapTable[ ou.cid() ].find( ou.id() );
  if( it == _mapTable[ou.cid()].end() ) {
    // Not our order 
    return;
  }
  const OrderStateRecord* rec = &(it->second);
  factory<debug_stream>::pointer _sobPrinter =  factory<debug_stream>::get(std::string("sobmodel"));
  char desc[1024];
  sprintf(desc,"%s %s %s %d %f %f %f",_dm->symbol(ou.cid()),Mkt::OrderActionDesc[ou.action()], OrderCancelSuggestion::CancelReasonDesc[rec->_cancel],rec->_npts,rec->_max,rec->_min,rec->_sum);
  TAEL_PRINTF(_sobPrinter.get(), TAEL_ERROR,"%s %s %d %f %f %f %s %f",_dm->symbol(ou.cid()),Mkt::OrderActionDesc[ou.action()],rec->_npts,rec->_max,rec->_min,rec->_sum,Mkt::SideDesc[rec->side], rec->price);
  //_fwdTracker->addOrder(ou.id(),std::string(desc));
  _mapTable[ ou.cid() ].erase( it );
}


void SOBOrderRepo::update ( const DataUpdate& du){
  /* See if the dataupdate concerns us, if so update the records
   * of all orders that we have with that signature
   */
  if (du.ecn!=ECN::ISLD)
    return;
  
  if (du.isBook() && (du.id == _lastTrade[du.cid]))
    return;
  
  if (du.isVisibleTrade())
    _lastTrade[du.cid]=du.id;

  if (cmp<3>::EQ(_priceCache[du.side][du.cid], du.price)){
    //vector<OrderStateRecord*> orders = getOrderRecords(du.cid,du.side,du.ecn,du.price);
    map<int,OrderStateRecord>::iterator it;
    for( it=_mapTable[du.cid].begin(); it!=_mapTable[du.cid].end(); it++ ) {
      OrderStateRecord* ordRec = &(it->second);
      if (( ordRec->side==du.side) && (ordRec->ecn == du.ecn) && (cmp<3>::EQ(ordRec->price,du.price)))
	ordRec->applyUpdate(du);
    }
  }
}


void SOBOrderRepo::update ( const OrderCancelSuggestion &cms){
  OrderStateRecord* rec= getOrderRecord( cms._orderId );
  if (rec==NULL)
    return;
  rec->_cancel = cms._reason;
}



vector< OrderStateRecord*> SOBOrderRepo::getOrderRecords( int cid, Mkt::Side s, ECN::ECN e,double px){
  vector< OrderStateRecord*> ret;
  map<int,OrderStateRecord>::iterator it;
  for( it=_mapTable[cid].begin(); it!=_mapTable[cid].end(); it++ ) {
    OrderStateRecord* ordRec = &(it->second);
    if (( ordRec->side==s) && (ordRec->ecn == e) && (cmp<3>::EQ(ordRec->price,px)))
      ret.push_back( ordRec );
  }
  return ret;
}


OrderStateRecord* SOBOrderRepo::getOrderRecord( int orderId )  {
  const Order* order = _dm->getOrder( orderId );
  if( order == NULL ) {
    TAEL_PRINTF(_logPrinter.get(), TAEL_WARN, "WARNING: Couldn't find an order with id=%d in SOBOrderRepo::getOrderRecord", orderId );
    return NULL;
  }
  int cid = order->cid();
  map<int,OrderStateRecord>::iterator it = _mapTable[cid].find( orderId );
  if( it == _mapTable[cid].end() ) {
    return NULL;
  }
  return &(it->second);
}

/// Get the OrderRecords of the specified componentId and tradeLogicId

bool SOBOrderRepo::addOrderRecord( const OrderUpdate& ou ) {
  int orderId = ou.id();
  OrderStateRecord ordRec(ou);
  pair<int,OrderStateRecord> p(orderId, ordRec); // This creates a copy of ordRec inside p, right?
  bool ret = _mapTable[ou.cid()].insert(p).second; 
  _priceCache[ou.side()][ou.cid()] = ou.price();
  char buffer[BUF_SIZE];
  const Order* order = _dm->getOrder( ou.id() );
  if (order)
    order->snprint( buffer, BUF_SIZE );
  return ret;
}


bool SOBOrderRepo::queuePositionUnfavorable( const Order* order, double *normFV){
  
  int cid = order->cid();
  OrderStateRecord *rec = getOrderRecord( order->id() );
  if (rec==NULL){
    return false;
  }
  if (rec->_refnum == (uint64_t) -1) {
    // Try to reinitialize the order
    rec->initRecord( order->confirmed());
  }
  *normFV = -100.0;

  double imbAlpha,excessImb;
  if (!_imbt->getCDImb(cid,imbAlpha,excessImb)){
    return false;
  }
  
  imbAlpha*=1e4;
  excessImb*=1e4;
  double  aspd, dspd;
  if (!_spdt->getAvgspd(cid,&aspd)){
    // Couldnt compute average spread, dont pull ?
    // This only happens in the first 2 minutes of the day
    return false;
  }
  _stocksState->getState(cid)->lastNormalSpread(&dspd);
  dspd -= aspd;
  
  int p1 = rec->_q - rec->_p1fills - rec->_p1pulls;
  int p1p = rec->_p1pulls;
  int p1f = rec->_p1fills;
  int p2a = rec->_p2adds;
  int p2p = rec->_p2pulls;
  int q = rec->_q;
  
  if (p1<0)
    p1=0;
  // Calculate derived factors.
  double dpq = log((p1+1.0)/(p1+p2a-p2p+1.0)); 
  double h1 = (p1p-p1f)/(p1+p1p+p1f+1.0);
  double h2= p2p/(p2a+1.0);
  double dpull=(p1p-p2p)/(p1p+p2p+1.0);
  int p2=p2a-p2p;
  double tm = HFUtils::milliSecondsBetween(order->confirmed().tv(), _dm->curtv());
  double ltm = log(1.0 + tm);
  
  // imbAlpha is calculated with absolute sign, not trade relative sign.
  
  if (order->side() == Mkt::ASK) {
    imbAlpha*=-1; 
    excessImb*=-1;
  }
  *normFV=_modelTracker->getModel(cid)->linearBlend( (double)q, (double)p2, h1, h2, dpq, dpull, dspd, aspd,ltm, imbAlpha,excessImb);
  rec->addEval(*normFV);
  if (*normFV < _modelTracker->getModel(cid)->cutoff ) 
    return true;
  return false;
}
