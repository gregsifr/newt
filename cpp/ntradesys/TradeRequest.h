#ifndef __TRADEREQUEST_H__
#define __TRADEREQUEST_H__

#include "c_util/Time.h"
using trc::compat::util::TimeVal;

#include "Markets.h"
#include <clite/message.h>

/*
  Message broadcast to indicate receipt of a new externally derived execution request.

  Currently includes state specified by the request:
  - stock.
  - target position (trade to this target # of shares).
  - request priority.
  
  Also includes some internally flagged information:
  - bid & ask at time of request.
  - wall/sim time at which request was received.
*/
// GVNOTE: Should change this function to take in a trade, instead of a target.
// Although, it might be easier to set targetPos = initPos + trade, instead of
// changing this class to take in a trade instead of a target.
class TradeRequest {
 public:
  int     _cid;           /// stock.
  int     _previousTarget;/// old target position.
  int     _initPos;       /// actual position at time of request.
  int     _targetPos;     /// new target position.
  double  _priority;      /// request priority.

  double _cbid;           // Composite bid @ time of request.  Ignoring odd lots.
  double _cask;           // Composite ask @ time of request.  Ignoring odd lots.
  TimeVal _recvTime;      // Time at which request was received.
  Mkt::Marking _marking ; // User specified short marking
  long _orderID;          // User specified orderID
  int _clientId; //The client that requested the trade

  TradeRequest( int cid, int previousTarget, int initPos, int targetPos, double priority, 
		double cbid, double cask, TimeVal recvTime, long orderID, int clientId, Mkt::Marking mark = Mkt::UNKWN )
    : _cid(cid), _previousTarget(previousTarget), _initPos(initPos), 
      _targetPos(targetPos), _priority(priority), 
      _cbid(cbid), _cask(cask), _recvTime(recvTime), _orderID(orderID), _clientId(clientId), _marking(mark) {}

  int snprint(char *s, int n) const;

  // -1 for decreasing position, 0 for no change, +1 for increasing position
  int tsign() const;

  int signedSize() const;
  int unsignedSize() const;
  bool isStop() const;

  void applyNew(const TradeRequest &tr);

};

/// The following queue is added to DataManager (which is the global coordinator) by ExecutionEngine
typedef clite::message::dispatch<TradeRequest>   TradeRequestsHandler; 

#endif   //  __TRADEREQUEST_H__
