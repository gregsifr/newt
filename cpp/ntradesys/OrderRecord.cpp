#include "OrderRecord.h"
#include "IBVolatilityTracker.h"

using trc::compat::util::DateTime;

const uint64_t UNKNOWN_REF_NUM = (uint64_t) -1;
const int UNKNOWN_Q_POS_IN_SHS = -1;
const static double DEFAULT_MINUTE_VOL = 0.0020;

OrderRecord::OrderRecord( const Order* order, const OrderPlacementSuggestion& placementMsg)
  : _orderId( order->id() ),    
    _tradeLogicId( placementMsg._tradeLogicId ),
    _componentId( placementMsg._componentId ),
    _componentSeqNum( placementMsg._componentSeqNum ),
    _placementReason( placementMsg._reason ),
    _cancelReason( OrderCancelSuggestion::NONE ),
    _cid( order->cid() ),
    _side( order->side() ),
    _size( order->size() ),
    _price( order->price() ),
    _mid ( 0.5*(placementMsg._cbbid+placementMsg._cbask) ),
    _ecn( order->ecn() ),
    _placingTV( order->placing().tv() ),
    _priority(placementMsg._priority),
    _marketImpact(placementMsg._marketImpact),
    _placeDuTV( TimeVal(0) ),
    _confirmationTV( TimeVal(0) ),
    _refnum( UNKNOWN_REF_NUM ),
    _refGuessed( false ),
    _initialShsPos( UNKNOWN_Q_POS_IN_SHS ),
    _nShsReducedFront( 0 ) 
{
  factory<VolatilityTracker>::pointer _volT  = factory<VolatilityTracker>::find(only::one);
  
  if (!_volT || (!_volT->getVolatility(order->cid(),_vol)))
    _vol=DEFAULT_MINUTE_VOL;
}


int OrderRecord::snprint(char *buf, int n) const {
  DateTime dt(_placingTV); 
   // Print aggregate info about order.
  return snprintf(buf, n, "OP %02d:%02d:%02d:%06d  CID%i  SIDE%s  SIZE%i  PX%f  ECN%s  RSN%s  ORDID%i  TLID%i  CMPID%i  CMPSN%i  PRI%.6f  MI%.6f",
		  dt.hh(), dt.mm(), dt.ss(), dt.usec(), 
		  _cid, Mkt::SideDesc[_side], _size, _price, ECN::desc(_ecn),
      		  OrderPlacementSuggestion::PlacementReasonDesc[_placementReason],
		  _orderId, _tradeLogicId, _componentId, _componentSeqNum,
		  _priority, _marketImpact);   
}

