#include "TradeRequest.h"
#include <cl-util/float_cmp.h>
using namespace clite::util;

using trc::compat::util::DateTime;

int TradeRequest::snprint(char *s, int n) const {
    DateTime dt(_recvTime);
    int i = snprintf(s, n, "TR-REQ %02d:%02d:%02d:%06d  BPX%.2f  APX%.2f %i -> %i  P%.6f",
		     dt.hh(), dt.mm(), dt.ss(), dt.usec(), _cbid, _cask, 
		  _initPos, _targetPos, _priority);
    return i;
}

int TradeRequest::tsign() const {
    if (_targetPos <  _initPos) return -1;
    if (_targetPos == _initPos) return 0;
    return 1;
}

int TradeRequest::signedSize() const {
    return _targetPos - _initPos;
}

int TradeRequest::unsignedSize() const {
    return abs(signedSize());
}

bool TradeRequest::isStop() const {
    return (signedSize() == 0);
}

/*
  Used in attempting to distinguish client-side trade-requests that really represent
      new/different requests, vs those that are simply extensions or slight modifications
      of old requests.
  Necessary in cases where execution behavior depends e.g. on time/results/behavior
      since a trade-reqeust was submitted, and where clients submit frequent requests that
      are basically just re-statements of older request.
  Notes:
      - Advanced swim only.  May imply that execution servers need to be statefull with
      respect to previous trade-requests, which was not designed for e.g. in architecture
      of client - execution server communciation protocols.
*/
void TradeRequest::applyNew(const TradeRequest &tr) {
  //
  if (_cid < 0) {
    (*this) = tr;
    return;
  } 
  
  // stop request:  ignore new TR, but dont overwrite old in case get later continue.
  if (tr.isStop()) {
    return;
  }
  
  // Request to continue trading:
  // - in same direction.
  // - with potentially different target or priority.
  // ==> Overwrite target & priority position only.
  if (tsign() == tr.tsign()) {
    _targetPos = tr._targetPos;
    _priority = tr._priority;
    return;
  } 
  
  // Otherwise, overwrite old TR entirely.
  (*this) = tr;
}
