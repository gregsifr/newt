#ifndef __ORDERSTATERECORD_H__
#define __ORDERSTATERECORD_H__

#include "Suggestions.h"
#include "DataManager.h"
#include <cl-util/table.h>
#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>
using namespace clite::util;


using std::vector;
class OrderStateRecord {
 public:
  int _id;       // BookElem id.  Used to map from order record to original order in data manager.       
  Mkt::Side side;
  double price;
  TimeVal _tv,outv; // Add tv and orderupdate tv (cached)
  ECN::ECN ecn;
  int cid;
  int size;
  uint64_t _refnum; // Used when guessing the position
  int _q;        // initial p1. (set once).
  // Counters that should be updated on every wakeup, from incoming event queues.
  int _qshs;
  int _dqshs;
  int _p1pulls;  // # of pulls ahead of you in queue, since you showed up in queue (running total).
  int _p1fills;  // # of fills ahead of you in queue, since you showed up in queue (running total).
  int _p2adds;   // # of orders added behind you in queue (running total).
  int _p2pulls;  // # of pulls from behind you in queue.  (running total).
   // For Order Tracking
  bool _guessed;
  int _npts;
  double _max;
  double _min;
  double _sum;
  OrderCancelSuggestion::CancelReason _cancel;
  factory<DataManager>::pointer _dm;
  OrderStateRecord(const OrderUpdate &ou);
  virtual ~OrderStateRecord();
  bool initRecord( const OrderUpdate &ou); // Initialize the state info
  void applyUpdate(  const DataUpdate &du); // Update State 
  int getID() { return _id;};
  int snprint(char *s, int n) const;
  int p1shs();
  void addEval(double ev);

 protected:
  void Fill(){ _p1fills++;}
  void pullBack() {_p2pulls++;}
  void pullFront() {_p1pulls++;}
  void addBack() {_p2adds++;}
  bool guessInit(); // Called by init if it cant find the refnum
  void reguessQueuePosition(); // Calls guess init and adjusts _q for the history
  
  
};
#endif
