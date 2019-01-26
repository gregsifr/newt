#ifndef __TRADELOGIC_H__
#define __TRADELOGIC_H__

#include <string>
using std::string;

#include <cl-util/factory.h>
using namespace clite::util;

#include "TradeLogicComponent.h"
#include "TradeRequest.h"
#include "Suggestions.h"
#include "StocksState.h"
#include "TradeConstraints.h"
#include "RiskLimits.h"
#include "MarketImpactModel.h"

/**
 * TradeLogic holds a main TradeLogicComponent. For each symbol this trade-logic is associated with, it gets suggestions to 
 * place/cancel orders from this component, and actually sends these placements/cancels on to DataManager.
 */
class TradeLogic : 
  public WakeupHandler::listener, 
  public MarketHandler::listener, 
  public OrderHandler::listener,
  public TradeRequestsHandler::listener
{

protected:
  factory<DataManager>::pointer      _dm;
  factory<StocksState>::pointer      _stocksState;
  factory<CentralOrderRepo>::pointer _centralRepo;
  factory<TradeConstraints>::pointer _tradeConstraints;
  factory<debug_stream>::pointer     _logPrinter;
  factory<RiskLimit>::pointer _riskLimits;
  /// The queues 'TradeLogic' broadcasts to
  factory<PlacementsHandler>::pointer  _placementsHandler;
  factory<CancelsHandler>::pointer     _cancelsHandler;

  // The underlying trade-logic-component that makes the decisions
  TradeLogicComponent* _mainTLComponent;

  // Used to guesstimating market-impact of liquidity-taking orders
  //   when they are placed.
  factory<TakeLiquidityMarketImpactModel>::pointer _tlMIM;

  vector<char>    _associations;     // which symbols (cid-s) are associated with this TL?
  vector<int>     _numTouches;       // Number of packets that touched each stock, between last wakeup and current. 
  vector<TimeVal> _lastTouch;        // The last time this stock has been touched (we artificially add touches after a whil without it)
  vector<char>    _currentlyTrading; // are we currently trading each cid (i.e. we are not in target pos and/or have outstanding orders)
  
  vector<Mkt::OrderResult> _lastOrderResult; // last orderstate returned by placeorder
  vector<int> _errCount; // Used to throttle printing of same error messages
  
  // Id number of this trade-logic instance
  int _tradeLogicId;
  double _OrderProb; // Hack to slow down trading
  // The order-placement-suggestion is accepted. Send to DM and broadcast to the matching queue.
  bool placeOrder( int cid, OrderPlacementSuggestion& placementSugg );

  bool isAssociated( int cid ) const {return _associations[cid] == 1;}
  void addTouch( int cid ) { _numTouches[cid]++; _lastTouch[cid]=_dm->curtv(); }
  int  numTouches( int cid ) const { return _numTouches[cid]; }
  void clearTouches() { _numTouches.assign(_dm->cidsize(), 0); }

  /// returns the number of orders that were placed
  int placeNewOrders( int cid, vector<OrderPlacementSuggestion> &placementSuggs );
  int cancelUndesiredOrders( int cid, unsigned int bidCapacity, unsigned int askCapacity, double priority );

  /// Does specified order attempt to take or provide liquidity, given
  ///  current market conditions.
  static bool takesLiquidity(const OrderPlacementSuggestion &ops);
public:

  TradeLogic( TradeLogicComponent* component );
  virtual ~TradeLogic() {};

  virtual void update( const DataUpdate& du ) { addTouch(du.cid); }
  virtual void update( const OrderUpdate& ou );
  virtual void update( const WakeUpdate& wu );  
  virtual void update( const TradeRequest& tradeRequest );

  // If the association between TradeLogic and stock is being broken, cancel any outstanding orders, 
  // and do anything necessary to remove link to specified stock in state.
  // This may be a bit tricky because it may mean that a stock may have
  //   an associated TradeLogic, have the trade logic disconnected, and then
  //   later receive order responses (e.g, fills) that don't map to any
  //   TradeLogic, or even map to a different TradeLogic than the one that
  //   generated the order.  Is this the right design????
  virtual void disconnect();
  virtual bool associate( int cid );
  virtual bool disassociate( int cid );
  
  virtual void setOrderProb( double prob ) { _OrderProb = prob;}
  // Apply the cancel suggestion (send cancel request to DM and cancel update in the relevant queue)
  void cancelOrder( OrderCancelSuggestion cancelSuggestion );
  void cancelAllOrders( int cid, OrderCancelSuggestion::CancelReason reason );
  void cancelAllOrders( int cid, ECN::ECN ecn, OrderCancelSuggestion::CancelReason reason );
};

#endif     //  #define __TRADELOGIC_H__
