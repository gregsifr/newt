/*
  Base class code for ExplanatoryReturnModel.  In this context, an ExplanatoryReturnModel is
    one that attempts to say what the ex-post return of some instrument "should have been",
    using any information available at the end of the return period.
  In theory. this definition of ExplanatoryModel includes the "identity" explanatory model, 
    aka definitionally the return should have been whatever the return in-fact was.
  However, to be useful in the intended contexts (mean-reversion signals, also probably
    performance attribution), the ExplanatoryModel should generally look at other information
    than the actual return of the specified instrument.
  Examples include:
  - Single (market) factor model:
    - E(R(s)) = B(s,m) * R(m).
  - Multi-factor model, e.g. Barra risk-model.
*/

#ifndef __EXPLANATORYMODEL_H__
#define __EXPLANATORYMODEL_H__

// client-lite factory.
#include <cl-util/factory.h>
using namespace clite::util;

// DataManager
#include "DataManager.h"

// vector
#include <vector>
using std::vector;

// string
#include <string>
using std::string;

// SyntheticIndex.
#include "SyntheticIndex.h"

// BarraUtils.h - for bmatrixd.
#include "BarraUtils.h"

class AlphaSignal;

/*
  Explanatory Return Model.
  - Applies over multiple stocks at once.
  - Rather than allowing direct queries for expected return over some 
    time interval (from T0 --> T1), allows for query of a current "fair value",
  - In this context, the "fair value" is not defined to have any basis.  Aka, 
    only the return in "fair value" from time to time shoudl be used, not the 
    value itself.
  - The explanatory return model interface allows for queries of current "fair value"
    only.  It is up to callers/users to keep older "fair value" estimates, and 
    calculate returns (from the differences in the respective fair values) over
    the desired time intervals.
*/
class ExplanatoryReturnModel {
 public:
  ExplanatoryReturnModel();
  virtual ~ExplanatoryReturnModel();

  virtual bool getFairValue(int cid, double &fv) = 0;
  virtual bool getInitialFairValue(int cid, double &fv) = 0;

  /*
    Static utility functions:
  */
  // Get current fair values for all stocks in a population set, using
  //   specified ExplanatoryReturnModel.
  // Returns number of stocks for which was able to get fair value.
  static int getFairValues(DataManager *dm, ExplanatoryReturnModel *erm, 
			    vector<double> &fillValues, vector<char> &fillFounds);
  // Get initial fair values for all stocks in a population set, using
  //   specified ExplanatoryReturnModel.
  // Returns number of stocks for which was able to get initial fair value.
  static int getInitialFairValues(DataManager *dm, ExplanatoryReturnModel *erm, 
			    vector<double> &fillValues, vector<char> &fillFounds);
};

/*
  Very simple explanatory return model:
  - Assumes that each stock in populations et is mapped to 0...1 ETFs,
    with a known beta vs specified ETF.
  - Assumes that beta values are static over life of ERMETDMid object.
  - Uses CBBO mid prices for ETFs, with non-tradedable (< 100 share) orders filtered out.
  - Assumes E(R(s)) = B(s,m) * R(m).
    When converted from expected-return to fair-value, assumes:
    
*/
class ERMETFMid : public ExplanatoryReturnModel, public TimeHandler::listener, public MarketHandler::listener {
 protected:
  factory<DataManager>::pointer         _dm;
  vector<string> _expNameV;                 // Name of ETF with which element is associated.  For debugging.
  vector<int>    _expCidV;                  // Cid of ETF with which element is associated.  -1 for no-mapping.
  vector<double> _expBetaV;                 // Beta wrt specified ETF.                        0 for no mapping.
  vector<double> _initialPriceV;            // 1st recorded price of specified stock.  -1 for NA.
  bool _marketOpen;
  
  bool parseParamFile(string &paramsFile);
  // Populate stock --> _expCidV mapping,
  int associateStocksWithETFS();
  // Walk through and set _initialPriceV.
  void initializePrices();

  bool getMidPrice(int cid, double &fv);

 public:
  ERMETFMid(string &betaFile);
  virtual ~ERMETFMid();
  
  // Return false and sets fv to 0.0 if can't calculate current "fair value".
  // Stocks that are not mapped to any ETF are assumed to have the "null" explanatory model, 
  //   aka E(R(s)) = 0, which in fair-value terms is translated to FV(s) always equal some constant.
  virtual bool getFairValue(int cid, double &fv);  
  virtual bool getInitialFairValue(int cid, double &fv);

  /*
    UpdateListener functions.
  */
  virtual void update(const DataUpdate &du);
  virtual void update(const TimeUpdate &au);
};

/*
  Linear multi-factor explanatory model.  This factor assumes:
  - A set of S stocks.
  - A set of F factors, each of which is represented by a synthetic index
    (an index with a fixed set of weighting and a current market-value 
    calculated on-the-fly from tick data, potentially using an alpha-signal
    derived per-stock fair value estimate, or possibly just using straight
    stated-cbbo mid prices).
  - A factor beta matrix B, with B[s, f] holding the beta of stock s 
    with respect to factor f.
  Inputs:
  - fvSignal:  An alpha signal to be used in estimating index constituent fair-values
    in SyntheticIndex calculations.  Can be specified as NULL (or dummay alpha signal)
    to just use stated mid prices.
  - factorWeights:  An S * F matrix, with M[s,f] holding weight of stock f
    in creating the synthetic index corresponding to linear factor model factor
    f.  
    This matrix is used in estimating the current market price of a synthetic
    index corresponding to factor F.
  - factorBetas:  An S * F matrix, with M[s, f] holding the beta of stock S 
    with respect to factor/synthetic-index f.
    This matrix is used in estimating the return beta of each stock with respect to
    each factor.

  Internal Operation (tentative):
  - Creates F synthetic indeces, each with the specified set of factor weightings
    (factorWeights[,f]).
  - Each of these synthetic indeces supports queries for current & initial fair-values.
  - Can then estimate return (from initial px) on each of these indeces.
  - For any stock S, "fair value" is then:
    1.0 + sigma(f)[factorBetas[s,f] * idx-return-from-initial-px(f)]
*/
class ERMLinearMultiFactor : public ExplanatoryReturnModel {
  int _S;                             // Number of stocks.
  int _F;                             // Number of factors/synthetic indeces.
  bmatrixd _factorBetas;              // Factor beta matrix.
  vector<SyntheticIndex*> _indeces;   // Set of synthetic indeces corresponding to factors.
 public:
  
  ERMLinearMultiFactor(AlphaSignal *idxCFVSignal, bmatrixd &factorWeights, bmatrixd &factorBetas);
  virtual ~ERMLinearMultiFactor();

  // Copy weights in matrix factorWeights[s, 1:F] into vector weights. 
  static void copyWeights(bmatrixd &factorWeights, int s, int F, vector<double> &weights);

  /*
    ExplanatoryReturnModel functions.
  */
  virtual bool getFairValue(int cid, double &fv);
  virtual bool getInitialFairValue(int cid, double &fv);
};


#endif  //  __EXPLANATORYMODEL_H__
