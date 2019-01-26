#include "Suggestions.h"

const char * OrderPlacementSuggestion::PlacementReasonDesc[] = 
    { "JOIN_QUEUE", "FLW_LEADER", "STEP_UP_L1", "CROSS     ", "TAKE_INVSB", 
      "FLW_SOB   ", "UNKNOWN   ", "MKTONCLOSE" };
const char * OrderCancelSuggestion::CancelReasonDesc[] = 
    { "NO_VALID_MKT", "NOT_AT_CBBO ", "Q_POS_UNFVR ", "LEADER_PULL ", "LSS_AG_CBBO ", 
      "NO_FOLLOWRS ", "INSIDE_SPRD ", "ST_SGL_ERR  ", "WRONG_SIDE  ", "NO_CAPCITY  ", 
      "ID_NOT_FD   ", "PRTY_TOO_LOW", "MRE_AG_TMID ", "STOP_REQUEST", "DISCONNECT  ", 
      "DISASSOCIATE", "ECN_DISABLED", "UNKWN_RSN   ", "NONE        "};

/******************************************************************************
  OrderPlacementSuggestion code
******************************************************************************/
OrderPlacementSuggestion::OrderPlacementSuggestion( int cid, ECN::ECN ecn, Mkt::Side side, int size, double price, int timeout, 
						    int tradeLogicId, int componentId, int componentSeqNum,
						    OrderPlacementSuggestion::PlacementReason reason,
						    const TimeVal &tv, double cbbid, double cbask, 
						    double priority) 
  :  _cid(cid),
     _ecn(ecn),
     _side(side),
     _size(size),
     _price(price),
     _timeout(timeout),
     _tradeLogicId(tradeLogicId),
     _componentId(componentId),
     _componentSeqNum(componentSeqNum),
     _reason(reason), 
     _cbbid(cbbid),
     _cbask(cbask),
     _tv(tv),
     _priority(priority),
     _marketImpact(0.0),
     _orderId(-1)
{

}

/******************************************************************************
  OrderCancelSuggestion code
******************************************************************************/
OrderCancelSuggestion::OrderCancelSuggestion( int orderId, int componentId, OrderCancelSuggestion::CancelReason reason,
					      double cbid, double cask, const TimeVal &tv ) 
  : _orderId( orderId ),
    _componentId( componentId ),
    _reason( reason ), 
    _cbid(cbid),
    _cask(cask),
    _tv(tv)
{

}

