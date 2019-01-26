/*
  CentralOrderRepo.h

  Original version:  dpuder, 2009.
  Mod History:
  - Moved OrderRecord to seperate file (OrderRecord.{cc,h}).
*/

#ifndef __CENTRALORDERREPO_H__
#define __CENTRALORDERREPO_H__

#include <map>
#include <vector>

#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>
#include <clite/message.h>

#include "DataManager.h"
#include "Markets.h"
#include "Suggestions.h"
#include "OrderRecord.h"

using std::map;
using std::vector;
using namespace clite::util;


/// The following two queues are added to DataManager( which is the global coordinator) by CentralOrderRepo
/// Each broadcaster to these queues should get them from the factory
/// TradeLogic populates these queues, and only after it adapted the suggestions, i.e. only if & when these are real actions taken
typedef clite::message::dispatch<OrderPlacementSuggestion> PlacementsHandler; 
typedef clite::message::dispatch<OrderCancelSuggestion>    CancelsHandler;    

/**
 *   A widget that essentially keeps a mapping from all current outstanding orders we placed to an OrderRecord 
 *   which keeps some extra identifying basic information about the order.
 *   The widget:
 *   - Listens to the order-placement-updates to learn about new order placements.
 *   - Listens to the order-cancels-updates to learn about reasons for cancelations
 *   - Listens to order-updates to learn about completion of orders
 *
 *   Notes:
 *   - When an order is placed, an OrderRecord is broadcast through the dispatch mechanism.
 *     - Tbe CentralOrderRepo sees this broadcast, and:
 *       - Inserts OrderRecord into a map from OrderID --> OrderRecord.
 *   - When an order is cancelled, an OrderCancelSuggestion is broadcast.
 *     - The CentralOrderRepo sees this broadcast, and:
 *       - Maps Cxl Request --> OrderRecord.
 *       - Sets OrderRecord --> CxlReaon.
 *   - When an order is done, an OrderUpdate is broadcast.
 *     - The CentralOrderRepo sees this broadcast, and:
 *       - Maps OrderUpdate --> OrderRecord.
 *       - Removes OrderRecord from CentralOrderRepo internal state.
 */
class CentralOrderRepo : 
  public PlacementsHandler::listener, 
  public CancelsHandler::listener, 
  public OrderHandler::listener 
{
private:
  static int nextPlacerId;

protected:
  factory<DataManager>::pointer  _dm;
  factory<debug_stream>::pointer _logPrinter;
  /// For each symbol: a map from order-Ids to OrderRecord (OrderRecord)
  vector< map<int,OrderRecord> > _mapTable;   

  virtual void update( const OrderPlacementSuggestion& placementMsg );
  virtual void update( const OrderCancelSuggestion&    cxlMsg );
  virtual void update( const OrderUpdate& ou );

public:
  CentralOrderRepo();

  /// Utility function.  Get a unique componentId or tradeLogicId.
  static int allocatePlacerId() { return nextPlacerId++; }

  /// Get pointer to the Queues this object holds

  /// Returns the OrderRecord of the specified orderId, or NULL if failed
  const OrderRecord* getOrderRecord( int orderId ) const;
  const OrderRecord* getOrderRecord( int cid, int orderId ) const; /// Somewhat more efficient than the previous one

  /// Get the OrderRecords of the specified componentId and tradeLogicId
  vector<const OrderRecord*> getOrderRecords( int cid, Mkt::Side side, int tradeLogicId, int componentId ) const;
  vector<const OrderRecord*> getOrderRecords( int cid, int tradeLogicId, int componentId ) const;
  /// Get the OrderRecords of the specified componentId
  vector<const OrderRecord*> getOrderRecords( int cid, int componentId ) const;
  /// Get the OrderRecords of the specified tradeLogicId
  vector<const OrderRecord*> getOrderRecords( int cid ) const;

  /// Add a OrderRecord to structure. 
  /// Returns false if failed to add it (because orderId not found in DM or already found in CentralOrderRepo)
  bool addOrderRecord( const OrderPlacementSuggestion& placementMsg );

  int totalOutstandingSize( int cid, Mkt::Side side, int componentId ) const;
  int totalOutstandingSize( int cid, Mkt::Side side, int componentId, int tradeLogicId ) const; /// I suspect this is redundant
  int totalOutstandingSize( int cid, Mkt::Side side, int componentId, int tradeLogicId, int componentSeqNum) const; /// Also redundant??
  int totalOutstandingSizeNotCanceling( int cid, Mkt::Side side, int componentId, int tradeLogicId ) const; /// I suspect this is redundant
  int totalOutstandingSizeMoreEqAggresiveThan( int cid, Mkt::Side side, double px, int componentId ) const; /// for JQ
};

#endif  // __CENTRALORDERREPO_H__
