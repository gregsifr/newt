#ifndef __ORDERSTATETRACKER_H__
#define __ORDERSTATETRACKER_H__


#include "ModelTracker.h"
#include "DataManager.h"
#include "CentralOrderRepo.h"
#include "ImbTracker.h"
#include "SpreadTracker.h"
#include "StocksState.h"
#include "ForwardTracker.h"
#include "Suggestions.h"
#include "OrderStateRecord.h"
#include <cl-util/table.h>
#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>

using namespace clite::util;


using std::vector;


class SOBOrderRepo : 
  public MarketHandler::listener,
  public OrderHandler::listener,
  public CancelsHandler::listener {
private:
  
protected:
  int _tradeLogicId;
  factory<DataManager>::pointer      _dm;
  factory<CentralOrderRepo>::pointer _centralOrderRepo;
  factory<ImbTracker>::pointer _imbt;
  factory<SpreadTracker>::pointer _spdt;
  factory<StocksState>::pointer       _stocksState;
  factory<debug_stream>::pointer _logPrinter;
  factory<ForwardTracker>::pointer _fwdTracker;

  /// For each symbol: a map from order-Ids to OrderRecord (OrderRecord)
  vector< map<int,OrderStateRecord> > _mapTable;   
  vector<double> _priceCache[2];
  vector<uint64_t> _lastTrade; // Use to catch book updates corresponding to the trades
  virtual void update( const OrderUpdate& ou );
  virtual void update( const DataUpdate& ou );
  virtual void update( const OrderCancelSuggestion& cxlMes );
  bool addOrderRecord( const OrderUpdate& ou );

public:
  factory<ModelTracker>::pointer _modelTracker;
  
  SOBOrderRepo(int tlid);

  /// Get pointer to the Queues this object holds
  
  /// Returns the OrderRecord of the specified orderId, or NULL if failed
  OrderRecord* getOrderRecord( int orderId ) const;
  bool queuePositionUnfavorable(const Order*, double *fv);
  bool FuzzyCompareTS(TimeVal t1,TimeVal t2) {return false;}
  vector< OrderStateRecord*>getOrderRecords( int cid, Mkt::Side s, ECN::ECN e, double px );
  OrderStateRecord* getOrderRecord( int orderId );
  bool guessInit( OrderStateRecord &rec, const OrderUpdate &ou);

};


#endif  
