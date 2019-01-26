#ifndef __FEECALC_H__
#define __FEECALC_H__


#include <string>
#include <vector>
using std::vector;
using std::string;

#include <cl-util/table.h>
#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>

using namespace clite::util;

#include "Markets.h"

/// Fees of a particular ECN
class EcnFees {
private:

  ECN::ECN  _ecn;

  bool   _feesKnown;

  double _takeLiqFee[  Mkt::NUM_TAPES];  // positive == BAD
  double _addLiqRebate[Mkt::NUM_TAPES];  // positive == GOOD
  double _routingFee[  Mkt::NUM_TAPES];  // positive == BAD

public:
  /// fields - those in the line dedicated to a specific symbol; fieldNames - the header line (ignored for now)
  EcnFees( const vector<string>& fieldNames, const vector<string>& fields );
  /// 'default' constructor: fees unknown
  EcnFees( ECN::ECN ecn );
  
  /// get fees
  bool   feesKnown() const { return _feesKnown; }
  double takeLiqFee(   Mkt::Tape tape ) const { return _takeLiqFee[tape]; }
  double addLiqRebate( Mkt::Tape tape ) const { return _addLiqRebate[tape]; }
  double routingFee(   Mkt::Tape tape ) const { return _routingFee[tape]; }

  ECN::ECN  getEcn() const { return _ecn; }
};

/**
 * For this class/interface:
 * - Fees are expressed in dollars, aka 0.01 = 1 penny.
 * - Fees are normally expressed as positive quantities (positive = bad, in this sense),
 * - Rebates are also normally expressed as positive numbers (positive = good in this case)
 */
class FeeCalc {
protected:
  // ECN fees + rebates
  vector<EcnFees*> _ecnFees;   /// fees per ECN

  // Brokerage Fees
  double _brokerage_fee_per_share;

  // SEC fees
  double _sec_fee_per_million_dollars;
  double _sec_min_fee;
  
  // NASD fees
  double _nasd_fee_per_share;  // with minimum of $0.01 and maximum of $3.75
  double _nasd_min_fee;
  double _nasd_max_fee;

  enum Brokerage { MS, LB };  // Lime Brokerage / Morgan Stanley
  Brokerage _brokerage;

public:

  FeeCalc();
  ~FeeCalc();
  
  bool   feesAreKnown ( ECN::ECN ecn ) const { return _ecnFees[ecn]->feesKnown(); }
  double takeLiqFee( ECN::ECN ecn, Mkt::Tape tape ) const { return _ecnFees[ecn]->takeLiqFee( tape ); }
  double addLiqRebate( ECN::ECN ecn, Mkt::Tape tape ) const { return _ecnFees[ecn]->addLiqRebate( tape ); }
  double routingFee( ECN::ECN ecn, Mkt::Tape tape ) const { return _ecnFees[ecn]->routingFee( tape ); }

  double brokerageFee( int size ) const { return size*_brokerage_fee_per_share; }

  /// This is the "Sec Section 31 Fee", which is taken from the seller side of each trade. 
  /// The function does not check whether it's a sell-order. It's assumed the user takes care of that.
  double getSECFee( double dollarAmount ) const;

  /// This is the "NASD Transaction Activity Fee(TAF)": a per-share fee on sells
  /// The function does not check whether it's a sell-order. It's assumed the user takes care of that.
  double getNASDFee( int size ) const;

  /// Get total fee (ECN fee + brokerage fee + SEC + NASD). Assuming no other fees (odd-lot, routing,...)
  /// This function does check if this is a sell order before adding up the NASD + SEC fees
  /// removeLiq: True ==> remove liqudity; False ==> add liqudity
  double getTotalFees( int size, double price, Mkt::Side side, ECN::ECN ecn, Mkt::Tape tape, bool removeLiq ) const;  
};

/// This is just a class that should enable the usage of "file_table" when reading the parameter files
/// It's a simple map from a string to a vector of strings
class ParamReader {
public:

  typedef std::string key_type;  // the key-word "key_type" is used in clite::util::file_table
  /// returns the symbol this line refers to
  std::string const &get_key() const { return _key; }

  /// fields - those in the line dedicated to a specific symbol; fieldNames - the header line (ignored for now)
  ParamReader( const vector<string>& fieldNames, const vector<string>& fields ) 
    : _fieldNames( fieldNames ),
      _fields( fields )
  {
    if( fields.size() < 1 )
      throw table_data_error( "at ParamReader::ParamReader: 'fields' vector is empty." );
    _key = fields[0];
  }
  
  string         _key;
  vector<string> _fieldNames;
  vector<string> _fields;
};

#endif    // __FEECALC_H__

