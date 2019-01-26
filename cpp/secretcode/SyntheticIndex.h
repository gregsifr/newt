/*
  Code for on-the-fly calculation of "synthetic" indeces, aka those whose value
    is observed from traded constituents (plus weightings on those constituents),
    but which are themselves not traded.
  Examples include:
  - Barra risk factor indeces.
  - S&P index FV using current prices only.
*/

#ifndef __SYNTHETICINDEX_H__
#define __SYNTHETICINDEX_H__

// client-lite factory.
#include <cl-util/factory.h>
using namespace clite::util;

// DataManager
#include "DataManager.h"

// vector
#include <vector>
using std::vector;

class AlphaSignal;

/*
  Base class, mostly defines public interface.
  SyntheticIndeces can be composed with AlphaSignals to use signal
    estimated "fair-values" for computing index values.
  Examples:
  - To just use stated-mid prices for estimating index values, use
    dummy/identity alpha signal (fv = stated mid).
  - To use stated-mid prices, adjusted for odd lots, use an alpha-signal
    that so adjusts.
  - To use a more complex "fair value" estimate, plug in the corresponding
    alpha signal.
*/
class SyntheticIndex {
 protected:

 public:
  SyntheticIndex();
  virtual ~SyntheticIndex();

  // Get current value of index, as estimated from traded constituents.
  // This fair value should have same basis as sum of weights * prices, aka
  //   the level of the fair value and the return in the fair value should both
  //   be meaningful quantities.
  virtual bool getFairValue(double &fv) = 0;

  // Convenience function:
  // Get 1st valid fair value calculated over the course of the trading day.
  virtual bool getInitialFairValue(double &fv) = 0;

  // Get stated-mid, ignoring alpha signal, filtering of odd lots, etc.
  static bool constituentStatedMid(DataManager *dm, int cid, double &fv);
  // Get current "fair-value" mid estimate.
  static bool constituentFairValue(DataManager *dm, AlphaSignal *asignal, int cid, double &fv);
  // Vector versions of above.  Return number of valid values filled.
  static int constituentStatedMids(DataManager *dm, vector<double> &fvMid, vector<char> &fvFilled);
  static int constituentFairValues(DataManager *dm, AlphaSignal *asignal, vector<double> &fvMid, vector<char> &fvFilled);
};

/*
  Specific implementation of SyntheticIndex:
  - Hooked into client-lite high-frequency event handling mechanisms.
  - Optimized for use with non-sparse indeces, aka indeces for which most of 
    the stocks in the program population set have non-zero weights.
  - Calculates constituent prices using composite (L1) bid & ask prices, potentially
    adjusted to "fair-value" based on some alpha signal.
  - Attempts to be efficient about event processing & index fv calculation time.
  - Intended for use during market trading hours only, not pre and post market trading.
     In particular:
     - Does not allow (successful) query for index FV when has at least 1 constituent with
       no known valid price.
     - Does not gracefully handle the case where stocks initially do not have valid markets,
       and then get valid markets.
   - Does not consider whether stocks are "opened" for trading:
     - Assumes that each stock should have *some* market price during trading hours.
     - Used stated top-level bid & ask for stocks, even though those may be wide/non-indicative
       before a particular stock is opened for trading.

  Internal State:
  - Object keeps vector of most recent trade prices for each stock, which is updated on every DataUpdate
    for the stock.
  - This allows object to keep running total of cumulative "fair-value".
    - Avoid need to cycle through all stocks when computing "fair-value", at the cost of additional
      processing time on each data update.
    - Code structure is thus optimized for large synthetic indeces (those with many members), as opposed
      to sparse synthetic indeces (those with few members).
    - May want to add additional class that efficiently handles sparse synthetic indeces.
*/
class SyntheticIndexHF : public SyntheticIndex, public MarketHandler::listener, public TimeHandler::listener {
 protected:
  /*
    Data:
  */
  factory<DataManager>::pointer _dm;         // Exists externally.  Accessed via factory system.
  AlphaSignal *_asignal;                     // Exists externally.  Passed in via constructor param.
  vector<double> _weights;                   // Index weights, applying to stocks in population set.
  vector<double> _midPrices;                 // Stock prices, applying to stocks in population set.
  vector<char> _validPrices;                 // Whether have a known last valid mid price, for each stock.
  bool _validFV;                             // Do we believe that we currently have a picture of the market state 
                                             //   that allows us to calculate an accurate current fair-value.
  double _fv;                                // FV price.        
  bool _marketOpen;                          // Is the market open for trading?

  bool _initialValidFV;                      // Have we seen at least one valid market state that allowed us to calculate
                                             //   a fair value?
  double _initialFV;                         // 1st fair-value calculated per day.   

  /*
    Member Functions:
  */
  // Attempt to calculate index fv given current available price.
  // Returns whether successful (have valid prices for all constituents) or 
  //   unsuccessful (no valid prices for at least one constituent).
  // Sets _validFV and _fv depending on success/failure.
  bool aggregateFV();
  void initializePrices();
  void setFV(bool valid, double fv);
  void getPrice(int cid);
  void adjustFV(int cid, double midPx);
 public:
  SyntheticIndexHF(AlphaSignal *asignal, vector<double> &weights);
  virtual ~SyntheticIndexHF();

  /*
    UpdateListener functions.
  */
  // In no explanatory-model version, can apply KF estimation technique directly....
  virtual void update(const DataUpdate &du);
  // In current base class implementation, can just look for MARKET_OPEN
  //   messages and call initializePrices.
  virtual void update(const TimeUpdate &au);

  /*
    SyntheticIndex base interface functions.
  */
  virtual bool getFairValue(double &fv);
  virtual bool getInitialFairValue(double &fv);
};


#endif   //__SYNTHETICINDEX_H__
