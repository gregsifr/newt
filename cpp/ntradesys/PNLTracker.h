
#ifndef __PNLTRACKER_H__
#define __PNLTRACKER_H__

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>
using namespace clite::util;


#include "DataUpdates.h"
#include "DataManager.h"
#include "HFUtils.h"
#include "Fills.h"

class DataManager;

/*
  Simple widget that:
  - Listens to the shared event bus for OrderUpdates.
  - Updates positions.
  - Can be explicitly queried to compute PNL (aka, does not continuously 
    compute PNL on the fly).  
*/
class PNLTracker : OrderHandler::listener, TimeHandler::listener {
 protected:

  factory<DataManager>::pointer _dm; // Underlying DataManager, assumed to be externally created....
  vector<Fills> _fills;              // Holds list of fill records for esch stock....
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
  // Assumes start day with 0 position in all stocks.
  PNLTracker();
  // Allows caller to specify BOD positions and prices.
  PNLTracker(vector<int> &initPosV, vector<double> &initPxV);
  virtual ~PNLTracker();

  // Get PNL for single stock.
  virtual double getPNL(int cid, double prcClose, bool includeTC);

  // Get PNL for all stocks in _dm population set.
  virtual double getPNL(vector<double> &prcCloseV, bool includeTC);

  // Get absolute (unsigned) number of shares filled, for single stock
  virtual int getAbsoluteSharesFilled(int cid);
  // And added up acroiss all stocks in population set.
  virtual int getAbsoluteSharesFilled();

  virtual double getAbsoluteDollarsFilled();
  virtual double getAbsoluteDollarsFilled(int cid);

  // Specify BOD positions and prices.
  void specifyInitialPositions(vector<int> &initPosV, vector<double> &initPxV);  

  // Print PNL in easily human readable format to specified output stream.
  virtual void printPNL(debug_stream *pdebug, tael::Severity plevel);
  // Print to internal debug stream.
  virtual void printPNL(tael::Severity plevel);

  /*
    UpdateListener functions that should be overridden by this class....
  */
  virtual void update( const OrderUpdate &u );
  virtual void update( const TimeUpdate &au );
};


/*
  Version of PNLTracker that does not listen to underlying HF infra
    for trade fills/replies.  Instead, its user/caller must explicitly
    notify it of fills.
  Intended for simulating signal performance in isolation of transaction costs
    and fill/n-fill logic.
*/
class PNLTrackerExplicit : public PNLTracker {
 public:
  // Assumes start day with 0 position in all stocks.
  PNLTrackerExplicit();
  // Allows caller to specify BOD positions and prices.
  PNLTrackerExplicit(vector<int> &initPosV, vector<double> &initPxV);

  virtual ~PNLTrackerExplicit();

  /*
    UpdateListener functions that should be overridden by this class....
  */
  virtual void update( const OrderUpdate &u );

  virtual void addFill(int cid, double price, Mkt::Trade direction, Mkt::Side side, 
		       ECN::ECN ecn, int size, TimeVal fillTime, 
		       double tcost, double cbid, double cask);  
};


#endif    //  __PNLTRACKER_H__
