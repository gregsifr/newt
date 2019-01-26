/*
  Widgets for keeping track of invisible orders, and also for
    keeping track of the subset of invisible orders that are 
    "followable" (safe to follow).
  This concept ("followable" invisible orders) is used in step-up/step-down
    logic to identify places where it is "safe" to step-up/price-improve,
    without accidentally hiting hidden orders and thus ending up paying
    take-liquidity fees.

  Here, a "followable" invisible trade is defined:
  - conceptually: as one that is more aggressive than the CBBO a short time epsilon 
    before the invis trd occurs.
  - concretely:  as an invisible trade (in stock x ecn x side x px) for which the
    preceeding action (for the same stock x ecn x side x px) is not a PULL or VISTRD
    that occured within some relatively short time period (e.g. 5 seconds).
  - changed 20090420:  a followable invisiable trade newly redefined as one that is
    strictly more aggressive than the last visible trade in the same stock x ecn x side.
    If no such visible trade is found, the invis trade is deemed not followable.
  - logic added July 2009:  Invisible trades now need to occur while trading for 
    the specific stock is "open", aka after the (potentially stock specific) open, 
    and before the (global) close.
  As such, the FollowableInvisOrderComponent answers questions related to the current
    wakeup, but also needs to keep some state info from past wakeups.
  The FollowableInvisOrderComponent object is responsible for limiting the size of
    its internal state (keeping it from growing without bound), without an 
    external limit/flush function (beyond that provided by periodic calling of
    wakeup function).  

  Extension to Multiple ECNs (July/August 2009):
  - The same basic idea behind followable invisible orders should be usable for 
    any ECNs that support reporting of invisible trades, and for which quotes and
    trades come from the same feed (or can otherwise be directly interleaved in
    real-time).
  - The logic for followable invisible trades probably does not need to
    change much here on expension from ISLD -> {ISLD, BATS, ARCA}.  As long
    as we can rely on the new exchanges reporting invisible trade data in the same
    manner and with the same timing as on ISLD, previously ISLD-specific logic
    in this class should be pretty directly applicable to these new feeds.
*/

#include "InvisOrderTracker.h"

using std::pair;

InvisOrderTracker::InvisOrderTracker() 
  : _invisibleTradesSincePreviousWakeup(),
    _clearMapNextUpdate(false) 
{
  // add this as a listener to the coordinator (data-manager)
  factory<DataManager>::pointer dm = factory<DataManager>::find(only::one);
  if( !dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in InvisOrderTracker::InvisOrderTracker)" );
  dm->add_listener_front( this );
}

void InvisOrderTracker::update( const DataUpdate& du ) {
  if( du.isInvisibleTrade() ) {
    // clear map if there's a need
    if( _clearMapNextUpdate ) {
      clear();
      _clearMapNextUpdate = false;
    }
    pair<int, DataUpdate> pt(du.cid,du);
    _invisibleTradesSincePreviousWakeup.insert(pt);
  }
}

void InvisOrderTracker::update( const WakeUpdate& wu ) {
  if( _clearMapNextUpdate ) {// this means we got no invisible-trades-updates since the last wakeup
    clear();
    _clearMapNextUpdate = false; // there are no DataUpdates to clear
  }
  else // we did get invisible-trade-updates, and these should be removed in the next DataUpdate/Wakeup, whatever comes first
    _clearMapNextUpdate = true;
}

vector<const DataUpdate*> InvisOrderTracker::getInvisibleTradesSinceLastWakeup( unsigned int cid ) const {
  
  vector<const DataUpdate*> ret;
  pair< multimap<int,DataUpdate>::const_iterator, multimap<int,DataUpdate>::const_iterator > rng;
  rng = _invisibleTradesSincePreviousWakeup.equal_range( cid );
  multimap<int, DataUpdate>::const_iterator it;
  
  for( it = rng.first; it != rng.second; ++it )
    ret.push_back( &(it->second) );
  return ret;
}
