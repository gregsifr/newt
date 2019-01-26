#include "PullTracker.h"
#include "Suggestions.h"
#include "Markets.h"
#include "OrderRecord.h"
#include <cl-util/table.h>
#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>
#include <UCS1.h>

#define BUF_SIZE 2048


using namespace clite::util;

typedef std::list<PullRecord *> OrderList;
const int PullRecord::intervals[] = {1,2,5,10,20,30,60,120,300};

PullTracker::PullTracker():
  _dm(factory<DataManager>::get(only::one)),
  _centralOrderRepo(factory<CentralOrderRepo>::get(only::one)),
  _lastTick(0)
{
  // Tracker assumes timer updates every second, so add a timer for it
  _dm->addTimer(Timer(TimeVal(1,0),TimeVal(0,0)));
  _dm->add_listener_front(this);
}


void PullTracker::update( const OrderCancelSuggestion& cxlMsg ) {
  int orderId = cxlMsg._orderId;
  const Order* order = _dm->getOrder( orderId );
  char orddesc[BUF_SIZE];
  const OrderRecord* cord = _centralOrderRepo->getOrderRecord( orderId );
  if (!cord) // Should not happen, CentralRepo will print error
    return;
  if ((!order) || (order->timeout()==0)) // Should not happen, we dont cancel IOC orders 
    return;
  
  factory<UCS1>::pointer ucsSignal = factory<UCS1>::get(only::one);
  double alpha=0.0;
  snprintf(orddesc,BUF_SIZE,"%-5s %i %s %s %s %d %.2f %.3f %.4f %.2f",_dm->symbol(order->cid()),orderId,ECN::desc(order->ecn()),OrderPlacementSuggestion::PlacementReasonDesc[cord->placementReason()], OrderCancelSuggestion::CancelReasonDesc[cxlMsg._reason], order->size(),order->price(),cord->_mid,cord->_vol,alpha);
   addOrder(orderId,std::string(orddesc));
}


void PullTracker::addOrder(int id,string desc){
  const Order* o = _dm->getOrder( id );
  PullRecord* orec = new PullRecord(o->cid(),o->side(),o->price(),desc);
  _orders.push_back(orec);
}


void PullTracker::update(const TimeUpdate& tu){
  if (tu.tv().sec()!=_lastTick){
    _lastTick = tu.tv().sec();
    for (OrderList::iterator it = _orders.end(); it!=_orders.begin();){
      it--;
      if ((*it)->tick()){
	(*it)->fprint(tu.tv());
	delete *it;
	it = _orders.erase(it);
      }
    }
    
  }
}

