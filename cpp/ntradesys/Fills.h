/*
  Fills.h
  Code for keeping track of fills/executions.  
  Used for e.g. transaction cost tracking & PNL estimation.
  Part of ntradesys project.  
  Code by Matt Cheyney, 2008.  All rights reserved.
*/


#ifndef __FILLS_H__
#define __FILLS_H__

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

/*
  List of executions, aka distinct fills on an order....
*/
class Exec {
 public:
  double _price;           // Fill px.
  int _size;               // Fill size.  Always positive #.
  Mkt::Trade _dir;         // BUY, SELL, SHORT.
  Mkt::Side _side;         // BID or ASK side.
  ECN::ECN _ecn;           // Where fill occurred....
  TimeVal _fillTV;         // Wall/sim time, on local client machine, when fillw as received.
  int _orderID;            // ID of original order that generated fill.
  double _tc;              // Transaction costs from trade.  Positive # = made $ = good.  Negative # = lost $ = bad.
  double _cbid;            // Composite bid & ask, at time of fill.
  double _cask;            //   Do these ignore odd lots????

  Exec();
  Exec( const OrderUpdate &ou, double bid, double ask);
  Exec( double price, int size, Mkt::Trade dir, Mkt::Side side, ECN::ECN ecn, const TimeVal &fillTV,
	double tcost, double cbid, double cask);
  /*
    Human readable/debugging output....
  */
  int snprint(char *buf, int n) const;     
};

/*
  List of executions, e.g. associated with a single order.
  Also has a bunch of utility functions for computing summary info about
    a collection of fills.
*/
class Fills {
  private:
    std::vector<Exec> fillList;

  public:
    Fills();
    ~Fills();
    void Clear();
    // Add fill, from OrderUpdate directly.
    void AddFill( const OrderUpdate &ou, 
	    double cbid, double cask);
    // Add synthetic fill.
    void AddFill( double price, Mkt::Trade dir, Mkt::Side side, 
		  ECN::ECN ecn, int size, const TimeVal &tv,
		  double tcost, double cbid, double cask);
    int GetNumFills() { return fillList.size(); }
    double GetAvgFillSize( TimeVal now, int secs=-1 );
    TimeVal GetAvgFillTime( TimeVal now, int secs=-1 );
    int GetShsFilled( TimeVal now, int secs=-1 );

    /*
      Some basic queries for avg fill prices.
    */
    // Straight (equally weighted) fill price.
    double sAvgFillPx();
    // Size (# shs) weighted avg fill price.
    double wAvgFillPx();

    // Total # of shares filled.
    // SIGNED version.
    int totalSharesFilled();
    // And UNSIGNED version.
    int absoluteSharesFilled();

    // Total dollars filled.
    // SIGNED version.
    double totalDollarsFilled();
    // And UNSIGNED version.
    double absoluteDollarsFilled();

    /*
      Query for series of prices, sizes, times, etc of fills.
    */
    // Fill a vector with set of fill prices.
    void fillPriceVector(vector<double> &pv);
    // Fill a vector with with of fill sizes.
    void fillSizeVector(vector<int> &sv);
    // Fill a vector with fillT values from all fills.
    void fillFillTimeVector(vector<TimeVal> &tv);

    // If got any fills, populate tv with time of last one & return true.
    // Otherwise, return false and do not populate tv.
    bool lastFillTime(TimeVal &tv);

    /*
      Queries related to PnL calculation....
    */
    // Compute the change in cash that has occurred over the course of the day.
    // If any positions remain, they are assumed to be liqudiated for prcClose.
    // By default, results from this call include transaction costs.
    // If strategy entered day with 0 position, or if a fake fill was inserted 
    //   to represent the initial BOD position, then this # should also
    //   represent intra-day PNL in the stock.
    double computeChangeCash(double prcClose, bool includeTC);

    /*
      Exposes access to underlying fills vector.  Should be a better way to do this!!!!
    */
    vector<Exec>::iterator begin();
    vector<Exec>::iterator end();
};


#endif   // __FILLS_H__
