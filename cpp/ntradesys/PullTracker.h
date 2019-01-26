#ifndef __PULLTRACK_H__
#define __PULLRACK_H__


#include "DataManager.h"
#include "StocksState.h"
#include "CentralOrderRepo.h"
#include <string>
#include <list>
#include <UCS1.h>
#include "Suggestions.h"

#include <cl-util/table.h>
#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>

// Q&D Version, merge with forward tracker

using namespace clite::util;
using std::string;

#define MAXPULLRETS 9

class PullRecord{
 public:
  int cid;
  Mkt::Side side;
  double px;
  string desc;
  double f2m_rets[MAXPULLRETS];
  double f2f_rets[MAXPULLRETS];
  static const int intervals[MAXPULLRETS];
  factory<StocksState>::pointer _stocksState;
  bool done;
  int tick_counter;
  int counter;
  int mult;
  
  PullRecord(int _cid,Mkt::Side _side,double _px, string _desc): 
    cid(_cid),
    side(_side),
    px(_px),
    desc(_desc),
    _stocksState( factory<StocksState>::get(only::one) ),
    done(false),
    tick_counter(0),
    counter(0)
    {
    mult = (side==Mkt::BID) ? 1 : -1;
  }
  
  bool tick(){
    tick_counter++;
    if (done) return(true);
    if (tick_counter>=intervals[counter]){
      SingleStockState* ss = _stocksState -> getState( cid );
      double curr_px = ss->bestPrice(side);
      double curr_mpx;
      ss->lastNormalMid(&curr_mpx);
      f2m_rets[counter]=(mult*(curr_mpx-px)/px);
      f2f_rets[counter]=(mult*(curr_px-px)/px);
      counter++;
    }
    if (counter>=MAXPULLRETS)
      done = true;
    return(done);
   }

  void fprint(TimeVal tv){
    factory<debug_stream>::pointer _logPrinter=factory<debug_stream>::get(std::string("pulltrk"));
    char logmsg[10000];
    char _ret_str[100];
    logmsg[0]=0;
    factory<UCS1>::pointer ucsSignal = factory<UCS1>::get(only::one);
    double alpha=0.0;
    if (!ucsSignal->getAlpha(cid,alpha))
      alpha = 0.0;
    alpha*=1e4; //bps
    strcat(logmsg,desc.c_str());
    sprintf(_ret_str," %.2f ",alpha);
    strcat(logmsg,_ret_str);
    for (int i=0;i<MAXPULLRETS;++i){
      sprintf(_ret_str," %.1f %.1f ",f2m_rets[i]*1e4,f2f_rets[i]*1e4);
      strcat(logmsg,_ret_str);
    }
    strcat(logmsg,"\n");
    TAEL_TPRINTF(_logPrinter.get(), &tv.tv, TAEL_ERROR, logmsg);
  }
   
};

//typedef list<PullRecord *> OrderList;

class PullTracker :
public TimeHandler::listener,
public CancelsHandler::listener

{
  factory<DataManager>::pointer _dm;
  factory<CentralOrderRepo>::pointer _centralOrderRepo;
  std::list<PullRecord*> _orders;
  long _lastTick;
 public:
  PullTracker();
  virtual void update (const OrderCancelSuggestion& cxlMsg );
  void addOrder(int id,string desc);
  virtual void update(const TimeUpdate &tu);
};



#endif
