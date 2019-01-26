#ifndef __STOCKSTATE_H__
#define __STOCKSTATE_H__

#include <vector>
#include <string>
using std::vector;
using std::string;

#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>
using namespace clite::util;

#include "c_util/Time.h"
using trc::compat::util::TimeVal;
using trc::compat::util::DateTime;

#include "Markets.h"

#include "DataManager.h"
#include "TradeRequest.h"

struct BookTop {

  double    _bidPx, _askPx;
  size_t    _bidSz, _askSz;
  
  BookTop() 
    : _bidPx(0.0), _askPx(0.0), _bidSz(0), _askSz(0) {}
};  

/**
 * Structure that keeps track of stock-specific market infomation.
 * Intended to hold info that TradeLogic or Trader objects are likely to want access to.
 */
class SingleStockState {
 public:

  enum MktStatus {
    NORMAL,    // data is accessible and spread is positive
    LOCKED,    // data is accessible but spread shows as zero
    CROSSED,   // data is accessible but spread shows as negative
    NODATA,    // data is not accessible
    UNKNOWN    // to be used only in initialization
  };
  const static char *const MktStatusDesc[];

  SingleStockState( int cid );
  virtual ~SingleStockState() {};

  int getCid() const { return _cid; }

  // positions and targets
  int    getTargetPosition() const { return _targetPos; }
  int    getCurrentPosition() const { return _dm->position(_cid); }
  double getPriority() const { return _priority; }
  int    getInitialPosition() const { return _initPos; }
  int    getSharesTraded() const {return _shsBought + _shsSold;}
  Mkt::Marking getMarking() const { return _marking; }
  long getOrderID() const { return _orderID; }
  void setTargetPosition( int tgt ) { _targetPos = tgt; }
  void setPriority( double p ) { _priority = p; }
  void setMarking ( Mkt::Marking marking ) { _marking = marking; }
  void setOrderID ( long orderID ) { _orderID = orderID; }
  // update target & priority
  void onTradeRequest( const TradeRequest& tr );

  /// update fills statistics
  void addFill( const OrderUpdate &ou );

  void onWakeup( bool haveSeenQuotesInBook );
  void refreshBookTop();   /// Refresh top-of-the-book variables, including "mkt-status"
  void updateMktStatus( MktStatus currMktStatus ); // update mkt-status variables, including print-outs for a change

  ///////////////
  /// current mkt info
  ///////////////
  inline bool haveNormalOrLockedMarket() const { return _mktStatus==NORMAL || _mktStatus==LOCKED; }
  inline bool haveNormalMarket() const { return _mktStatus==NORMAL; }
  inline MktStatus getMktStatus() const { return _mktStatus; }
  inline const char* getMktStatusDesc() const { return MktStatusDesc[_mktStatus]; }
  TimeVal     getLastChangeInMktStatus() const { return _lastChangeInMktStatus; }
  // These versions do not do careful error checking
  inline double spread() const { return (_bookTop._askPx - _bookTop._bidPx); }
  inline double mid() const { return (_bookTop._askPx + _bookTop._bidPx)/2; }
  inline double bestPrice( Mkt::Side side ) const { return (side==Mkt::BID ? _bookTop._bidPx : _bookTop._askPx); }
  inline int    bestSize(  Mkt::Side side ) const { return (side==Mkt::BID ? _bookTop._bidSz : _bookTop._askSz); }
  inline bool   strictlyInsideSpread( double px ) const { return cmp<4>::LT(_bookTop._bidPx, px) && cmp<4>::LT(px, _bookTop._askPx); }
  // More "careful" versions: first check if market is normal
  bool   bestPrice( Mkt::Side side, double* px ) const { *px=bestPrice(side); return haveNormalMarket(); }
  bool   bestSize( Mkt::Side side, int* size ) const { *size=bestSize(side); return haveNormalMarket(); }

  bool atCBBO( Mkt::Side side, double px ) const; // is px at the current top level of side 'side' (false if mkt not normal)
  bool lessAggressiveThanCBBO( Mkt::Side side, double px ) const; // false if mkt not normal
  bool strictlyInsideLastNormalCBBO( double px ) const; // false if there's no last-normal book-top

  ///////////////
  // current exclusive-market info
  ///////////////
  double    getExclusiveBestPrice( Mkt::Side side ); /// ignores mkt-status of exclusive-market
  int       getExclusiveBestSize(  Mkt::Side side ); /// ignores mkt-status of exclusive-market
  double    getExclusiveSpread();  /// ignores mkt-status of exclusive-market
  double    getExclusiveMid();  /// ignores mkt-status of exclusive-market
  MktStatus getExclusiveMktStatus(); 
  bool      haveNormalExclusiveMarket();
  bool      haveNormalOrLockedExclusiveMarket();
  bool      getExclusiveBestPrice( Mkt::Side side, double* px ); /// returns false if exclusive market is not normal
  bool      getExclusiveBestSize(  Mkt::Side side, int*  size ); /// returns false if exclusive market is not normal
  bool      getExclusiveSpread( double* spread );  /// returns false if exclusive market is not normal
  bool      getExclusiveMid( double* mid );  /// returns false if exclusive market is not normal

  ///////////////
  /// last normal-mkt info
  ///////////////
  bool lastNormalMid( double *px ) const;
  bool lastNormalSpread( double *spread ) const ;
  bool lastNormalPrice( Mkt::Side side, double *px ) const;

  ///////////////
  // last wakeup mkt info
  ///////////////
  bool previousWakeupPrice( Mkt::Side side, double *px ) const; // returns false if last-wakeup-mkt is not normal nor locked
  bool previousWakeupNormalPrice(Mkt::Side side, double *px)  const;

  double minimalPxVariation() const { return _minimalPxVariation; }
  
 protected:

  factory<DataManager>::pointer  _dm;          
  factory<debug_stream>::pointer _logPrinter;
  const int                      _cid;

  // current mkt info
  BookTop   _bookTop;
  MktStatus _mktStatus;
  TimeVal   _lastChangeInMktStatus;

  // Top level excluding our orders. Updated only on request.
  //(Though in a very loose and shaky definition: we subtract all our orders from placing till they reach a DONE status. 
  // So sizes of levels can actually come out negative! )
  BookTop   _exclusiveBookTop;
  MktStatus _exclusiveMktStatus;
  TimeVal   _lastExclusiveMktUpdate;
  void      updateExclusiveMkt();

  // last wakeup mkt-info
  BookTop   _lastWakeupBookTop;   // BookTop as of last wakeup
  BookTop   _lastWakeupNormalTop;
  MktStatus _lastWakeupMktStatus;

  // last normal mkt info
  BookTop _lastNormalBookTop;    // Last BookTop known to be normal, including current BookTop 
  bool    _seenAnyNormalBookTop; // Did we ever see a normal cbbo since the program started?
  bool    _seenOldNormalBookTop; // Do we have an old price to compare against

  // resolution of the price of the stock (currenlt set to $0.01 for all stocks)
  double  _minimalPxVariation; 

  // Info from most recent trade-request for stock. 
  // Not clear whether SingleStockState is correct place to store this....
  int    _targetPos;
  double _priority;
  
  long _orderID;

  // Marking Info
  Mkt::Marking _marking;
  
  // Position info:
  int _initPos;
  int _shsBought;
  int _shsSold;
  int _nBuys;  // # fills (partial fills are counted independantly)
  int _nSells; // # fills (partial fills are counted independantly)
  TimeVal _lastFillTime;

  /// These are cumulative fees, according to the numbers we get from the server
  double _totalFees;     // positive is bad for you
  double _totalFeesBuy;  // -"-
  double _totalFeesSell; // -"-
};

/**
 * A main "repository" that keeps a "SingleStockState" for every stock in DataManager
 */
class StocksState :    
  public MarketHandler::listener,
  public OrderHandler::listener,
  public WakeupHandler::listener
  //public TradeRequestsHandler::listener
{
  factory<DataManager>::pointer  _dm;    
  vector<SingleStockState*>      _states;
  vector<int>                    _nDuSinceLastWakeup; // the number of data-updates since last wakeup for every stock

public:
  StocksState();
  ~StocksState();
  
  virtual void update( const DataUpdate& du ) { _nDuSinceLastWakeup[du.cid]++; }
  virtual void update( const OrderUpdate& ou );
  virtual void update( const WakeUpdate& wu );
  //virtual void update( const TradeRequest& tr ) { _states[tr._cid]->onTradeRequest(tr); }

  SingleStockState* getState( unsigned int cid ) { return _states[cid]; }
};

inline const char* getTimeString( TimeVal& tv ) { return DateTime(tv).gettimestring(); }

#endif  //__STOCKSTATE_H__
