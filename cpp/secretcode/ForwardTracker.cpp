#include "ForwardTracker.h"
#include "Suggestions.h"
#include "Markets.h"
#include "OrderRecord.h"
#include <cl-util/table.h>
#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>
#include <UCS1.h>

#define BUF_SIZE 2048


using namespace clite::util;

typedef std::list<ForwardRecord *> OrderList;
const int ForwardRecord::intervals[] = {1,2,5,10,20,30,60,120,300,600};

ForwardTracker::ForwardTracker():
  _dm(factory<DataManager>::get(only::one)),
  _centralOrderRepo(factory<CentralOrderRepo>::get(only::one)),
  _lastTick(0)
{
  // Tracker assumes timer updates every second, so add a timer for it
  _dm->addTimer(Timer(TimeVal(1,0),TimeVal(0,0)));
  _dm->add_listener_front(this);
}

void ForwardTracker::update(  const OrderUpdate& ou ) {
  // Wrapper for standalone version
   if( ou.state()!=Mkt::DONE )
    return;
   if (ou.sharesFilled()==0)
     return;
   char orddesc[BUF_SIZE];
   const OrderRecord* cord = _centralOrderRepo->getOrderRecord( ou.id() );
   factory<UCS1>::pointer ucsSignal = factory<UCS1>::get(only::one);
   double alpha=0.0;
   
   if (!ucsSignal->getAlpha(ou.cid(),alpha))
     alpha = 0.0;
   alpha*=1e4; //bps
   if (!cord){
     if (ou.timeout()>0)
       snprintf(orddesc,BUF_SIZE,"%-5s %i %s  UN_PROV UNKNWN %d %.2f %.3f %.4f %.2f",_dm->symbol(ou.cid()),ou.id(),ECN::desc(ou.ecn()),ou.sharesFilled(),ou.price(),ou.price(),0.002,alpha);
     else
       snprintf(orddesc,BUF_SIZE,"%-5s %i %s  UN_TAKE UNKNWN %d %.2f %.3f %.4f %.2f",_dm->symbol(ou.cid()),ou.id(),ECN::desc(ou.ecn()),ou.sharesFilled(),ou.price(),ou.price(),0.002,alpha);
   }else
     snprintf(orddesc,BUF_SIZE,"%-5s %i %s %s %s %d %.2f %.3f %.4f %.2f",_dm->symbol(ou.cid()),ou.id(),ECN::desc(ou.ecn()),OrderPlacementSuggestion::PlacementReasonDesc[cord->placementReason()], OrderCancelSuggestion::CancelReasonDesc[cord->cancelReason()], ou.sharesFilled(),ou.price(),cord->_mid,cord->_vol,alpha);
   addOrder(ou.id(),std::string(orddesc));
}

void ForwardTracker::addOrder(int id,string desc){
  const Order* o = _dm->getOrder( id );
  if (!o) 
    return;
  ForwardRecord* orec = new ForwardRecord(o->cid(),o->side(),o->price(),desc);
  _orders.push_back(orec);
}


void ForwardTracker::update(const TimeUpdate& tu){
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

