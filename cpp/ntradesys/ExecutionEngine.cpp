#include "ExecutionEngine.h"

/*********************************************************************
  ExecutionEngine functions.
*********************************************************************/
ExecutionEngine::ExecutionEngine() 
  : _stocksState( factory<StocksState>::get(only::one) ),
    _tradeRequestHandler( factory<TradeRequestsHandler>::get(only::one) ),
    _logPrinter( factory<debug_stream>::get(std::string("trader")) )
{
  init();
}

ExecutionEngine::ExecutionEngine( TradeLogicComponent* component ) 
  : _stocksState( factory<StocksState>::get(only::one) ),
    _tradeRequestHandler( factory<TradeRequestsHandler>::get(only::one) ),
    _logPrinter( factory<debug_stream>::get(std::string("trader")) )
{
  init();
  _singleTradeLogic = new TradeLogic( component );
  associateAllStocks( _singleTradeLogic );
}

ExecutionEngine::~ExecutionEngine() {
  if( _singleTradeLogic != NULL )
    delete _singleTradeLogic;
}

void ExecutionEngine::init() {
  _dm = factory<DataManager>::find(only::one);
  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in ExecutionEngine::ExecutionEngine)" );

  _cidToTradeLogic.resize( _dm->cidsize(), NULL );

  _dm -> add_dispatch( _tradeRequestHandler.get() );
}

bool ExecutionEngine::trade(int cid, int size, double priority, long orderID, int clientId, Mkt::Marking marking) {
  int cp = _dm -> position(cid);
  return tradeTo(cid, cp + size, priority, orderID, clientId, marking);
}

/// broadcast a request-update through the trade-request-queue
bool ExecutionEngine::tradeTo( int cid, int target, double priority, long orderID, int clientId, Mkt::Marking marking ) {
  if( _cidToTradeLogic[cid] == NULL ) {
    TAEL_PRINTF(_logPrinter.get(), TAEL_WARN, "%-5s WARNING: on ExecutionEngine::tradeTo, but this symbol is associated with no TradeLogic. ",
			 _dm->symbol(cid) );
    return false;
  }

  SingleStockState *ss = _stocksState->getState(cid);
  int previousTarget = ss->getTargetPosition();
  int currentPosition = ss->getCurrentPosition();
  TAEL_PRINTF(_logPrinter.get(), TAEL_INFO, "%-5s ExecutionEngine::tradeTo: currPos = %4d, target changed %4d ==> %4d, "
		       "priority= %7.4f bps, cbbo=(%.2f,%.2f) mktStt=%s marking=%s",
		       _dm->symbol(cid), _dm->position(cid), previousTarget, target, priority*10000, 
		       ss->bestPrice(Mkt::BID), ss->bestPrice(Mkt::ASK), ss->getMktStatusDesc(), Mkt::MarkingDesc[marking] );

  // populate the trade-request into the Trade-Requests-Queue
  TradeRequest tr( cid, previousTarget, currentPosition, target, priority, 
		   ss->bestPrice(Mkt::BID), ss->bestPrice(Mkt::ASK), _dm->curtv(), orderID, clientId, marking );
  ss->onTradeRequest( tr );
  _tradeRequestHandler->send( tr );

  return true;
}

/**
 *  Request to stop trading. Handle like a trade request to trade to current position with the current priority.
 */
bool ExecutionEngine::stop( int cid, int clientId ) {
  SingleStockState *ss = _stocksState->getState(cid);
  TradeLogic       *tl = _cidToTradeLogic[cid];
  if( ss == NULL || tl==NULL ) return false; // should never happen

  int    previousTarget = ss->getTargetPosition();
  int    target = _dm->position( cid );
  double priority = ss->getPriority();
  
  TAEL_PRINTF(_logPrinter.get(), TAEL_INFO, "%-5s ExecutionEngine::stop: currPos = %6d, target changed %6d ==> %6d, "
		       "priority= %5.2f bps, cbbo=(%.2f,%.2f) mktStt=%s",
		       _dm->symbol(cid), _dm->position(cid), previousTarget, target, priority*10000, 
		       ss->bestPrice(Mkt::BID), ss->bestPrice(Mkt::ASK), ss->getMktStatusDesc() );

  // populate the trade-request into the Trade-Requests-Queue


  //NSARKAS note that orderId = -1
  TradeRequest tr( cid, previousTarget, target, target, priority, 
		   ss->bestPrice(Mkt::BID), ss->bestPrice(Mkt::ASK), _dm->curtv(), -1, clientId, Mkt::UNKWN );
  ss->onTradeRequest( tr );
  _tradeRequestHandler->send( tr );

  return true;
}

bool ExecutionEngine::stop(int clientId) {
  bool ret = true;
  for (int cid=0; cid<_dm->cidsize(); cid++ ) 
    ret &= stop( cid, clientId );
  return ret;
}

/// set target to 0 for all stocks, keeping current priorities
/*bool ExecutionEngine::exitAllPositions() {
  bool ret = true;
  for( int cid=0; cid<_dm->cidsize(); cid++ )
    ret &= tradeTo( cid, 0, getPriority(cid), getOrderID(cid) );
  return ret;
}*/

/// cancel all open orders on a particular ECN and send matching messages
void ExecutionEngine::cancelAllEcn( ECN::ECN ecn ) {
   for( int cid=0; cid<_dm->cidsize(); cid++ )
     _cidToTradeLogic[cid] -> cancelAllOrders( cid, ecn, OrderCancelSuggestion::ECN_DISABLED );
}  

/*
  Get status of trading for specified stock.  Should include:
  - whether trying to trade or not.
  - how many shs done.
  - current position.
*/
TradeStatus ExecutionEngine::status( int cid ) const{
  SingleStockState *ss = _stocksState->getState( cid );
  bool currentlyTrading = ss->getTargetPosition() != _dm->position(cid) || getMarketOrders(_dm->orderBook(), cid) != 0;

  TradeStatus::TradingState  state;
  if( _cidToTradeLogic[cid] == NULL )
    state = TradeStatus::NO_ASSOCIATION;
  else if( currentlyTrading )
    state = TradeStatus::TRADING;
  else
    state = TradeStatus::NOT_TRADING;

  TradeStatus ret( state, _dm->position(cid), ss->getTargetPosition(), ss->getSharesTraded(), ss->getPriority() );
  return( ret );
}

bool ExecutionEngine::associateAllStocks( TradeLogic* tl ) {
  bool ret = true;
  for( int cid=0; cid<_dm->cidsize(); cid++ )
    ret &= associate( cid, tl );
  return ret;
}

bool ExecutionEngine::associate( int cid, TradeLogic* tl ) {
  if( cid < 0 || cid >= _dm->cidsize() ) return false;
 _cidToTradeLogic[ cid ] = tl; 
 return tl->associate( cid );
}

bool ExecutionEngine::disassociate( int cid ) {
  TradeLogic *tl = _cidToTradeLogic[ cid ];
  if (tl == NULL) return false;
  _cidToTradeLogic[ cid ] = NULL;
  return tl->disassociate( cid );
}

void ExecutionEngine::disconnect() {
  std::set<TradeLogic*> seen;
  for( int cid=0; cid<_dm->cidsize(); cid++ ) {
    TradeLogic* tl = _cidToTradeLogic[ cid ];
    if( seen.count( tl ) == 0 ) {
      tl -> disconnect();
      seen.insert( tl );
    }
  }
}
