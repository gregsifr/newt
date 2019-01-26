/*
 *
 *  UCS1.h  
 *  Unconditional (mid -> mid return predicting) alpha signal, V1.0.
 *
 *   UCS1 signal:
 *   Linear Blend of:
 *   - KFRTETFSignal (using SPY + SPY-BETA as explanatory model).
 *   - ImbTracker/PLSOBSignal.
 *   Uses (Externally generated) parameter file holding betas wrt those two signals.
 *   Uses default values (for stocks that it cant find in parameter file) of:
 *   - KFRTETFSignal beta = 0.0.
 *   - ImbTracker/PLSOBSignal beta = 1.0.
 *   Aka, by default uses 100% PLSOB signal and 0% KFRT signal.
*/

#ifndef __UCS1_H__
#define __UCS1_H__

#include "AlphaSignal.h"
#include "ImbTracker.h"
#include "KFRTSignal.h"


class UCS1 : public AlphaSignal {
 protected:
  factory<DataManager>::pointer _dm;         // Exists externally.  Accessed via factory system.
  factory<debug_stream>::pointer _ddebug;
  factory<ImbTracker>::pointer _imbSignal;   // ImbTracker/PLSOB signal calculator.
  factory<ETFKFRTSignal>::pointer _kfrtSignal;                // KFRT ETF signal calculator.

  // Old-style.  Uses PerStockParams.
  bool parseParamsFileOld(string &ucsBetaFile);

  // New-style.  Uses file-table.
  bool parseParamsFile();
 public:
  vector<double> _kfrtBeta;                  // KFRT signal weights, per stock.
  vector<double> _imbBeta;                   // ImbTracker/PLSOB signal weights, per stock.

  // Notes:  
  // - ETFKFRTSignal constructor has other params - but
  //   the signal weighting process assumes a fixed set of params, so these are
  //   set at the UCS1 level, not passed in to the UCS1 constructor.
  UCS1();
  virtual ~UCS1();

  /*
    AlphSignal functions.
  */
  // Get expected alpha from signal.
  virtual bool getAlpha(int cid, double &fv);  
};


class UCS1FileTableElem {
 public:
  typedef std::string key_type;  // the key-word "key_type" is used in clite::util::file_table
  /// returns the symbol this line refers to
  std::string const &get_key() const { return _symbol; }

    
  UCS1FileTableElem( const vector<string>& fieldNames, const vector<string>& fields );
  double getKFRTBeta() const {return _kfrtBeta;}
  double getImbBeta() const {return _imbBeta;}
protected:

  std::string    _symbol;
  double         _kfrtBeta;
  double         _imbBeta; 
};


#endif   // __UCS1_H__
