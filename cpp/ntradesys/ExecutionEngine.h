#ifndef __EXECUTIONENGINE_H__
#define __EXECUTIONENGINE_H__

#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>
using namespace clite::util;
#include "Markets.h"
#include "TradeRequest.h"
#include "StocksState.h"
#include "TradeLogic.h"

/**
 *  Get status of trading in a particular stock.
 */
class TradeStatus {
 public:
  enum TradingState {NO_ASSOCIATION, TRADING, NOT_TRADING};
  TradingState tState;
  int currentPosition;
  int targetPosition;
  int shsTraded;
  double priority;    // Priority of most recent trade request.

  TradeStatus( TradingState tradingState, int currPos, int targetPos, int shsTraded_, double priority_ )
    : tState(tradingState), currentPosition(currPos), targetPosition(targetPos), shsTraded(shsTraded_), priority(priority_) {}
};

class ExecutionEngine {

  factory<DataManager>::pointer          _dm;
  factory<StocksState>::pointer          _stocksState;
  factory<TradeRequestsHandler>::pointer _tradeRequestHandler;
  factory<debug_stream>::pointer         _logPrinter;

  /// Maps stocks <-> TradeLogics. Every stock is associated with a particular TradeLogic, or with no TradeLogic
  vector<TradeLogic*>                    _cidToTradeLogic;
  TradeLogic*                            _singleTradeLogic; /// used only, possibly, when a single TL is common to all stocks

  // stuff that normally belongs to the constructor (here because there're two constructors)
  void init();  

public:
  ExecutionEngine();
  ExecutionEngine( TradeLogicComponent* component ); // all stocks are associated to the same trade-logic
  virtual ~ExecutionEngine();

  /// problematic: we might be in the process of changing our position when getting this message, and this creates ambiguities
  /// size: a signed number
  //NSARKAS, marking no longer has a default value. I found it error prone when trying to add additional parameters
  virtual bool trade( int cid, int size, double priority, long orderID, int clientId, Mkt::Marking marking);
  virtual bool tradeTo( int cid, int target, double priority, long orderID, int clientId, Mkt::Marking marking);
  virtual bool stop( int cid, int clientId );

  virtual bool stop(int clientId); // call stop on all stocks
  //virtual bool exitAllPositions(); // set all targets to 0, keeping current priorities
  virtual void cancelAllEcn( ECN::ECN ecn ); // cancel all open orders on a particular ECN and send matching messages

  /// Get current status of trading( trading/not-trading, target, position, priority )
  TradeStatus status( int cid ) const;  
  int         getTargetPosition( int cid ) const { return _stocksState->getState(cid)->getTargetPosition(); }
  double      getPriority( int cid ) const { return _stocksState->getState(cid)->getPriority(); }
  long getOrderID( int cid ) const { return _stocksState->getState(cid)->getOrderID(); }
  Mkt::Marking  getShortMarking( int cid ) const { return _stocksState->getState(cid)->getMarking(); }
  bool associateAllStocks( TradeLogic *tl );
  bool associate( int cid, TradeLogic *tl );
  /// Do not need to call disassociate between subsequent calls to associate.
  bool disassociate( int cid );
  /// Cleanly disconnect from HF infrastructure.  Typically called on program shutdown
  /// Likely set of actions: Send all TradeLogic(s) the disconnect signal.
  void disconnect();
  void setOrderProb(double prob) { _singleTradeLogic->setOrderProb(prob);}  // Passthrough function
};

#endif    //  __EXECUTIONENGINE_H__
