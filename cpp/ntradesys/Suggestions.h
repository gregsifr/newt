#ifndef __SUGGESTIONS_H__
#define __SUGGESTIONS_H__

#include "Markets.h"
#include "c_util/Time.h"
using trc::compat::util::TimeVal;
using trc::compat::util::DateTime;

/**
 * Used by trade logic components to suggest order placements to trade logics.
 * TradeLogics are free to ignore/modify the suggested order placements.
 */
class OrderPlacementSuggestion {

 public:
  enum PlacementReason {
    JOIN_QUEUE, FOLLOW_LEADER, FOLLOW_INVTRD, CROSS, TAKE_INVISIBLE, FOLLOW_LEADER_SOB, UNKNOWN, MKT_ON_CLOSE};

  const static char * PlacementReasonDesc[];

  OrderPlacementSuggestion( int cid, ECN::ECN ecn, Mkt::Side side, int size, double price, int timeout, 
			    int tradeLogicId, int componentId, int componentSeqNum, PlacementReason reason,
			    const TimeVal &tv, double cbbix, double cbask, double priority);
  virtual ~OrderPlacementSuggestion() {};

  void setOrderId( int orderId ) { _orderId = orderId; }
  int  getOrderId() const { return _orderId; }
  void setMarketImpact(double mi) {_marketImpact = mi;}
  double getMarketImpact() const { return _marketImpact;}

  int _cid;         
  ECN::ECN _ecn;    /// Can use ECN::UNKNWN to specify that the TradeLogic needs to determine this itself.
  Mkt::Side _side;  /// BID/ASK.  This should really be Market::Trade dir, with the DataManager free to remark
                    ///    SELL --> SELL_SHORT, except that DataManager::placeOrder expects Mkt::Side, not
                    ///    Mkt::Trade.  Given the current DataManager interface, this should always be set
                    ///    to BID for BUY trades and ASK for SELL | SELL_SHORT trades, regardless of which
                    ///    side of the market the trade is actually being placed on.
  int _size;        /// Unsigned (always positive). TradeLogic should feel free to decrease this, e.g. to avoid overfills.
  double _price;    
  int _timeout;     
  int _tradeLogicId;    /// Id of the tradeLogic *trade logic* that actually does the order placement.
  int _componentId;     /// Id of the *order placement component* that suggested placing order.
  int _componentSeqNum; /// a sequence number set by the component, to enable tracking

  PlacementReason  _reason; /// Type (enumeration) of the order placement component that suggested placing order,
  double _cbbid;        /// Composite best bid & ask at time of placement, ignoring odd-lots.
  double _cbask;
  TimeVal _tv;          /// Wall/sim time when order-placement suggestion is generated.

  double _priority;     /// leaf-node priority.  Aka priority passed to leaf OPC that actually suggested order.
  double _marketImpact; /// Estimated market-impact=, at time of placement.  Populated for liquidity-taking orders only.

  int snprint(char *s, int n) const {
    DateTime dt(_tv);
    int i = snprintf(s, n, "OPS CID%i  ECN%s  SIDE%s  SIZE%i  PX%.2f  TOUT%i  TLID%i  CMPID%i  CSQN%i  RSN%s  %02d:%02d:%02d:%06d  CBID%.2f  CASK%.2f  ORDID%i  PRI%.6f  MKTIMP%.6f",
		     _cid, ECN::desc(_ecn), Mkt::SideDesc[_side], _size,
		     _price, _timeout, _tradeLogicId, _componentId, _componentSeqNum,
		     PlacementReasonDesc[_reason], dt.hh(), dt.mm(), dt.ss(), dt.usec(),
		     _cbbid, _cbask, _orderId, _priority, _marketImpact);
    return i;
  }

 protected:
  int _orderId;  /// Filled only if and when we decide to place the order. Until then it's (-1)
};

/**
 * Used by trade-logic-components and trade-logic-styles to keep track of cancel-suggestions,
 * and also as cxl-message once the TradeLogic decides to accept the suggestion and cancel the order.
 * (The second usage doesn't go well with the name "Suggestion"... Any solutions?)
 */
class OrderCancelSuggestion {
public:
  /// NONE: when the enum is used for orders the cancelation of which was not yet suggested
  enum CancelReason {
    NO_VALID_MARKET, NOT_AT_CBBO,      Q_POSITION_UNFAVORABLE,    LEADER_PULL, LESS_AGGRESSIVE_THAN_CBBO, 
    NO_FOLLOWERS,    INSIDE_SPREAD,    ST_SIGNAL_ERR,             WRONG_SIDE,  NO_CAPACITY,  
    ID_NOT_FOUND,    PRIORITY_TOO_LOW, MORE_AGGRESSIVE_THAN_TMID, STOP_REQ,    DISCONNECT, 
    DISASSOCIATE,    ECN_DISABLED,     UNKNWN_REASON,             NONE};
  static char const *CancelReasonDesc[];

  OrderCancelSuggestion( int orderId, int componentId, OrderCancelSuggestion::CancelReason reason,
			double cbid, double cask, const TimeVal &tv );

  int snprint(char *s, int n) const {
    DateTime dt(_tv);
    // Print aggregate info about cancel suggestion
    return snprintf(s, n, "CXLR %02d:%02d:%02d:%06d  CMPID%i  RSN%s  BPX%.2f  APX%.2f  ORDID%i",
		    dt.hh(), dt.mm(), dt.ss(), dt.usec(), _componentId, 
		    CancelReasonDesc[_reason],
		    _cbid, _cask, _orderId);  
  }

  int          _orderId;
  int          _componentId;
  CancelReason _reason;

  double   _cbid;                // Composite bid @ time of cancel.  Round lots only????
  double   _cask;                // Composite ask @ time of cancel.  Round lots onlky???? 
  TimeVal  _tv;                  // Wall/sim time when cancel suggestion is initiated. 
};

#endif // __SUGGESTIONS_H__
