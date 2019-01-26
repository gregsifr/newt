/*
  TCTracking.h - Short for Transaction Cost Tracking.
  Code/utilities used for recording trade rquest, order placement,
    and fill info, and for trying to map those back to transaction cost
    calculation/estimation.
  Part of ntradesys project.  
  Code by Matt Cheyney, 2008.  All rights reserved.
*/

#ifndef __TCTRACKING_H__
#define __TCTRACKING_H__

#include <vector>
using std::vector;
#include <list>
using std::list;
#include <string>
using std::string;

// Hyp2/Hyp3 includes.
#include "c_util/Time.h"
using trc::compat::util::TimeVal;

// Client-Lite includes
#include "Markets.h"
#include "DataManager.h"

// Same directory includes.
//   for PlacementRequestsHandler; CancelRequestsHandler;
#include "CentralOrderRepo.h"
#include "TradeRequest.h"
#include "Fills.h"

// cpputil includes:
#include "CircBuffer.h"

class DataManager;
class DataCache;

typedef vector<OrderCancelSuggestion> CancelSuggestions; 
typedef vector<OrderPlacementSuggestion> PlaceSuggestions;
typedef vector<Order> Orders;
/*
  Records each distinct TradeRequest made against a stock, plus all the
    associated OrderPlacement and Fill info (derived from that TradeRequest).
  It is assumed (in design of below class) that at most 1 TradeRequest is active 
    against a given stock at a single time, per TROPFTracker instance.
    
*/
class TradeRequestRecord {
 public:
  /*************************************************************
    Internal State
  *************************************************************/
  // Root trade request.
  TradeRequest _tr;    

  // Order placements associated with TradeRequest.
  PlaceSuggestions _places;
  
  // Order cancel attempts associated with TradeRequest.
  // - Stored as a set of OrderCancelSuggestions.
  // - Can use orderId to map from OrderRecord
  //  --> associated set of cancel attempts.
  CancelSuggestions _cancels;

  // Fills associated with TradeRequest.
  // - Stored as set of Exec records.
  // - Can use orderID to map from OrderRecord --> associated set 
  //   of executions.
  Fills _fills;


  /*************************************************************
    Member Functions
  *************************************************************/
  TradeRequestRecord(const TradeRequest &tr);
  virtual ~TradeRequestRecord();

  /*
    Methods that print internal state in human readable format.
  */
  // Print transaction Cost Estimation summary info....
  void printTCSummaryInfo(DataManager *dm, 
			  debug_stream *tdebug, 
			  tael::Severity plevel,
			  string& prefix, const char* symbol);

  /*
    Methods that change internal state.
  */
  // Add a single fill.
  void addFill(const OrderUpdate &ou, double cbid, double cask);

  // Add OrderRecord (corresponding to new order placmement suggestion).
  void addOrder(const OrderPlacementSuggestion &ops);

  // Add a cancel attempt.
  void addCancelSuggestion(const OrderCancelSuggestion &ca);

  /*
    Methods that query against internal state.
  */
  // Number of fills off this request.
  int numFills() {return _fills.GetNumFills();}
  // Total # of shares filled from this request
  int totalSharesFilled() {return _fills.totalSharesFilled();}

  int numOrderPlacements() {return _places.size();}

  // Mid price at CBBO if that represents a valid market, otherwise from lastValidCBBO.
  bool initialMidPx(double *fv);
  bool initialPx(Mkt::Side side, double *fv);

  // Walk through fills, guessing which ones result from opr.
  // Stick (copies of) those into fillMe.
  int associateFills(const OrderPlacementSuggestion &ops,
		      vector<Exec> &fillMe);

  // Walk through cancels, guessing which ones result from opb.
  // Stick (copies of) those into fillMe.
  int associateCancelSuggestions(const OrderPlacementSuggestion &opr,
			      vector<OrderCancelSuggestion> &fillMe);

  /*
    Average fill price calculations.
  */
  // Straight (equally weighted) fill price.
  double sAvgFillPx();
  // Size (# shs) weighted avg fill price.
  double wAvgFillPx();

  /*
    Slippage calculations.
    Slippage:
    - Defined to be 0 if we're not sure what rel mid px was when started
      trading.
    - Positive # = good = made $ on trading,
    - Negative # = bad = lost $ on trading.
  */
  // Straight (equal weight) avg slippage.
  // Defined to be 0 if we don't have a valid initial mid px.
  double sAvgSlippage();
  // Weighted (by # shs) avg slippage.
  // Defined to be 0 if we don't have a valid initial mid px.
  double wAvgSlippage();

  /*
    Fill time calculations.
    Here we (somewhat arbitrarily) define fill time to be in minutes
      since trade request was received.
  */
  double totalFillMinutes();

  /*
    Utility functions for printing pretty output.
  */
  int snprint_base(char *buf, int n);
  int snprint_fillandslip(char *buf, int n);
  int snprint_ocs(char *buf, int n, const OrderCancelSuggestion &ocs);
  //int snprint_ord(char *buf, int n, const OrderRecord &orec, const Order *ord);
};

/*
  Records all of the TradeRequests made against a stock during a single
    trading day/session (per TradeSysImpl/TROPFTracker instance).
*/
class TradeRequestRecordSet {
 protected:
  vector<TradeRequestRecord> trRecords;
 public:

  // Initializes trRecords to empty set (aka vector of size 0 elements).
  TradeRequestRecordSet();
  virtual ~TradeRequestRecordSet();

  // Delete all records in set.
  void clear();

  // Add new TradeRequestRecord to set.
  // Subsequent fills are assumed to belong to this request until another
  //   request is added.
  // Initial state is to have no oustanding requests, aka addTradeRequest should be
  //   called before the 1st call to addFill.
  void addTradeRequest(const TradeRequest &tr);

  // Add fill to current request.
  void addFill(const OrderUpdate &ou, double cbid, double cask);

  // Add a new OrderPlacementRecord.  Incoming fills are examined to try to map
  //   them back to the corresponding OrderPlacementRecord.
  void addOrder(const OrderPlacementSuggestion &ops);

  // Add a new cancel attempt with specified parameters - to the current outstanding 
  //   trade request.
  void addCancelSuggestion(const OrderCancelSuggestion &ca);


  void printTCSummaryInfo(DataManager *dm, debug_stream *tdebug, 
			  tael::Severity plevel,
			  string& prefix, const char* symbol);
};


/*
  Widget that keeps track of TradeRequests, OrderPlacements, Fills, and Cancels.
  - Keeps in-memory tree-like structure, of, per stock:
    - TradeRequests.
      - Each TradeREquest has associated OrderPlacements.
        - Each OrderPlacement has associated Fills, and CancelSuggestions.
  - This information is stored in memory, for the life of the widget.
  - Information is not flushed over the course of the day, aka it does not
    only store information about live TradeRequests or live Orders.
  - Code is not written for efficient storage/retrieval/query of information.
  As such:
  - TROPFTracker is suitable for gathering information that needs that entire 
    dependency structure in memory, rather than e.g. being able to replicate the
    dependency struucture from log files or some other serialized record.
  - TROPFTracker should only be used for debugging, or inside toy applications that
    do not generate huge order activity.
  - TROPFTTracker should not be used in production execution engine code, especially 
    production code that handles large customer traffic volumes.
*/
class TROPFTracker : 
  public PlacementsHandler::listener, 
  public CancelsHandler::listener, 
  public OrderHandler::listener,
  public TradeRequestsHandler::listener,
  public TimeHandler::listener
 {
 protected:
  // Just used for getting symbol strings for human-readable output.
  factory<DataManager>::pointer _dm;        // Exists externally.
  // Holds 1 TradeRequestRecordSet per stock in population set....
  vector<TradeRequestRecordSet> _trsSet;
  factory<debug_stream>::pointer _ddebug;

  bool _printOnClose;                       // Automatically print TC summary info on/after mkt close.
  bool _seenMktClose;                       // Have we seen a markety close message?
  TimeVal _mktCloseTV;                      // What was the Time of that message.
  bool _printedOnClose;                     // Have we already dump state following the last mkt close message?
  
  // How long after mkt close to delay printing.
  // For live operation, generally don't want to start printing right away as:
  // - Containing program may be single threaded.
  // - Printing can tie up that thread for long periods of time.
  // - Can receive trade messages after close, e.g. delayed fills or fills from auctions.
  // By default - delay 10 minutes.
  const static int PRINT_ON_CLOSE_DELAY_MILLISECONDS = 10 * 60 * 1000;
 public:
 
  TROPFTracker();
  virtual ~TROPFTracker();

  /*
    HF dispatcher/event functions.
  */
  // Responses from trade-server.
  virtual void update(const OrderUpdate &ou); 
  // Order cancel suggestions,             
  virtual void update(const OrderCancelSuggestion &ocs );
  // Order placement suggestions (+ associated state).
  virtual void update(const OrderPlacementSuggestion &ops); 
  // EE - exernally generated trade requests.
  virtual void update(const TradeRequest &trd);
  // Timer messages.
  virtual void update(const TimeUpdate &au);

  /*
    Functions for printing state info.  In this case, TC Summary info.
  */
  // Print to an externally specified debug_stream.
  void printTCSummaryInfo(debug_stream *pdebug,  
			  tael::Severity plevel,
			  string& prefix);  

  // Print to internal debug stream.
  void printTCSummaryInfo(tael::Severity plevel,
			  string& prefix);  
};


/*
  Variant of TROPFTracker, but one that continuously prints info to a debug stream,
    rather than storing up info until the end of day and then dumping it all at once.
*/
class TROPFTrackerImmediate : 
  public PlacementsHandler::listener, 
  public CancelsHandler::listener, 
  public OrderHandler::listener,
  public TradeRequestsHandler::listener
  // public TimeHandler::listener
{
 protected:
  // Just used for getting symbol strings for human-readable output.
  factory<DataManager>::pointer _dm;        // Exists externally.  
  factory<debug_stream>::pointer _ddebug;
  string _prefix;

  const static tael::Severity plevel;

 public:
  TROPFTrackerImmediate();
  virtual ~TROPFTrackerImmediate();  

  // Responses from trade-server.
  virtual void update(const OrderUpdate &ou); 
  // Order cancel suggestions,             
  virtual void update(const OrderCancelSuggestion &ocs );
  // Order placement suggestions (+ associated state).
  virtual void update(const OrderPlacementSuggestion &ops); 
  // EE - exernally generated trade requests.
  virtual void update(const TradeRequest &trd);
};


#endif  // __TCTRACKING_H__
