#include "FeeCalc.h"
#include "DataManager.h"
#include "Markets.h"
#include <cl-util/float_cmp.h>

const double MILLION_DOUBLE = 1000000.0;
const string NA_STRING( "N.A." );

FeeCalc::FeeCalc() 
  : _ecnFees( ECN::ECN_size )
{
  // Get the brokerage identity from DM.
  // This is not clean, as, for instance, it means we always use MS for sim.
  DataManager::TradeSystem tradesys = factory<DataManager>::get(only::one) -> getTradeSystem();
  switch( tradesys ) {
  case DataManager::COLO:     _brokerage = MS; break;
  case DataManager::SIMTRADE: _brokerage = MS; break;
  case DataManager::NONE:     _brokerage = MS; break;
  }   
  
  factory<debug_stream>::pointer  logPrinter = factory<debug_stream>::get( string("trader") );

  // First, read the global fee parameters
  string global_fees_filename( "global_fees" );
  file_table<ParamReader> globalFeeParams( global_fees_filename );
  if ( globalFeeParams.empty() ){
    throw std::runtime_error( "No parameters read in from global-fees-param-file in FeeCalc::FeeCalcTracker (input_file=" +
			      global_fees_filename + ")" );
  }
  file_table<ParamReader>::iterator it;
  it = globalFeeParams.find( "SEC_FEE_PER_MILLION_DOLLARS" );
  if( it != globalFeeParams.end() ) {
    _sec_fee_per_million_dollars = atof( it->second._fields[1].c_str() );
    TAEL_PRINTF(logPrinter.get(), TAEL_INFO, "FeeCalc: sec_fee_per_million_dollars = %f", _sec_fee_per_million_dollars );
  } else 
    throw std::runtime_error( "ERROR: Couldn't get sec_fee_per_million_dollars from param-file '" + global_fees_filename + "'" );
    
  it = globalFeeParams.find( "SEC_MIN_FEE" );
  if( it != globalFeeParams.end() ) {
    _sec_min_fee = atof( it->second._fields[1].c_str() );
    TAEL_PRINTF(logPrinter.get(), TAEL_INFO, "FeeCalc: sec_min_fee = %f", _sec_min_fee );
  } else 
    throw std::runtime_error( "ERROR: Couldn't get sec_min_fee from param-file '" + global_fees_filename + "'" );
    
  it = globalFeeParams.find( "NASD_FEE_PER_SHARE" );
  if( it != globalFeeParams.end() ) {
    _nasd_fee_per_share = atof( it->second._fields[1].c_str() );
    TAEL_PRINTF(logPrinter.get(), TAEL_INFO, "FeeCalc: nasd_fee_per_share = %f", _nasd_fee_per_share );
  } else 
    throw std::runtime_error( "ERROR: Couldn't get nasd_fee_per_share from param-file '" + global_fees_filename + "'" );

  it = globalFeeParams.find( "NASD_MIN_FEE" );
  if( it != globalFeeParams.end() ) {
    _nasd_min_fee = atof( it->second._fields[1].c_str() );
    TAEL_PRINTF(logPrinter.get(), TAEL_INFO, "FeeCalc: nasd_min_fee = %f", _nasd_min_fee );
  } else 
    throw std::runtime_error( "ERROR: Couldn't get nasd_min_fee from param-file '" + global_fees_filename + "'" );

  it = globalFeeParams.find( "NASD_MAX_FEE" );
  if( it != globalFeeParams.end() ) {
    _nasd_max_fee = atof( it->second._fields[1].c_str() );
    TAEL_PRINTF(logPrinter.get(), TAEL_INFO, "FeeCalc: nasd_max_fee = %f", _nasd_max_fee );
  } else 
    throw std::runtime_error( "ERROR: Couldn't get nasd_max_fee from param-file '" + global_fees_filename + "'" );

  // Second, read the brokerage-specific fee parameters
  string brokerage_specific_filename;
  switch( _brokerage ) {
    case MS: brokerage_specific_filename = "fees_table_MS"; break;
    case LB: brokerage_specific_filename = "fees_table_LB"; break;
  }
  file_table<ParamReader> specificFeeParams( brokerage_specific_filename );
  if ( specificFeeParams.empty() ){
    throw std::runtime_error( "No parameters read in from specific--fees-param-file in FeeCalc::FeeCalcTracker (input_file=" +
			      brokerage_specific_filename + ")" );
  }
  // The per-ECN fees
  for( int i=0; i<ECN::ECN_size; i++ ) {
    ECN::ECN ecn = (ECN::ECN)i;
    it = specificFeeParams.find( ECN::desc(ecn) );
    if( it != specificFeeParams.end() ) {
      TAEL_PRINTF(logPrinter.get(), TAEL_INFO, "FeeCalc:: Found fees of %s", ECN::desc(ecn) );
      _ecnFees[ecn] = new EcnFees( it->second._fieldNames, it->second._fields );
    } else {
      TAEL_PRINTF(logPrinter.get(), TAEL_INFO, "FeeCalc:: Couldn't find fees of %s", ECN::desc(ecn) );
      _ecnFees[ecn] = new EcnFees( ecn );
    }
  }
  // the brokerage fee
  it = specificFeeParams.find( "BROKERAGE_FEE_PER_SHARE" );
  if( it != specificFeeParams.end() ) {
    _brokerage_fee_per_share = atof( it->second._fields[1].c_str() );
    TAEL_PRINTF(logPrinter.get(), TAEL_INFO, "FeeCalc: brokerage_fee_per_share = %f", _brokerage_fee_per_share );
  } else 
    throw std::runtime_error( "ERROR: Couldn't get brokerage_fee_per_share from param-file '" + brokerage_specific_filename + "'" );
}

FeeCalc::~FeeCalc() {
  for( unsigned int i=0; i<_ecnFees.size(); i++ ) 
    delete _ecnFees[i];
}

// This is the "Sec Section 31 Fee", which is taken from the seller side of each trade
// The content of the function is based on analysing real "account-summaries" from Lime-Brokerage
double FeeCalc::getSECFee( double dollarAmount ) const {
  double ret = dollarAmount / MILLION_DOUBLE * _sec_fee_per_million_dollars;
  // round in cents
  ret = ((int)(ret*100 + 0.5)) / 100.0;
  // make sure it's at least $0.01 (which is the minimum)
  if( ret < _sec_min_fee ) ret = _sec_min_fee;

  return ret;
}

// This is the "NASD Transaction Activity Fee(TAF)": a per-share fee on sells
// The rules are based on the last line from Lime's "Brokerage US Execution Rebte and Fee Schedule" from 3/8/09
// The content of the function is based on analysing real "account-summaries" from Lime-Brokerage
double FeeCalc::getNASDFee( int size ) const {

  typedef clite::util::cmp<5> cmp; // two numbers which are < 0.000005 apart are considered equal

  double ret = _nasd_fee_per_share * size;
  // round UP in cents (ceiling)  (0.021 ==> 0.03, but 0.02 ==> 0.02)
  double floor = ((int)(ret*100)) / 100.0;
  if( cmp::GT( ret, floor ) )
    ret = floor + 0.01;
  else
    ret = floor;
  // apply the minimum and maximum fees
  if( ret < _nasd_min_fee ) ret = _nasd_min_fee;
  if( ret > _nasd_max_fee ) ret = _nasd_max_fee;

  return ret;
}

double FeeCalc::getTotalFees( int size, double price, Mkt::Side side, ECN::ECN ecn, Mkt::Tape tape, bool takeLiq ) const
{
  double totalFees = 0.0;
  int abs_size = std::abs( size );
  totalFees += abs_size * (takeLiq ? takeLiqFee(ecn,tape) : -addLiqRebate(ecn,tape));
  totalFees += brokerageFee( abs_size );
  if( side == Mkt::ASK ) {
    totalFees += getSECFee( abs_size*price );
    totalFees += getNASDFee( abs_size );
  }
  return totalFees;
} 


EcnFees::EcnFees(  const vector<string>& fieldNames, const vector<string>& fields ) 
  : _feesKnown(true)
{
  
  factory<debug_stream>::pointer  logPrinter = factory<debug_stream>::get( string("trader") );
  int n_fields = fieldNames.size();

  /////////////////////////
  // CHECK HEADER LINE
  /////////////////////////
  if( n_fields != 10 ) {
    TAEL_PRINTF(logPrinter.get(), TAEL_ERROR, "ERROR: brokerage-specific-fees file should have 10 fields in header (and has %d).", n_fields );
    throw table_data_error( "Header line of ecn-fees has wrong # fields" );
  }
  if( fieldNames[0] != string("ECN") ) {
    TAEL_PRINTF(logPrinter.get(), TAEL_ERROR, "ERROR: brokerage-specific-fees file has the wrong first field name in header. "
			"(it's %s while %s is expected)", fieldNames[0].c_str(), "ECN" );
    throw table_data_error( string("The first field-name in the header line of volume-distr should be 'ECN'") );
  }
  
  /////////////////////////
  // CHECK DATA LINE
  /////////////////////////

  // if there's different number of fields Vs. fieldNames, throw exception
  if( fields.size() != n_fields ) {
    TAEL_PRINTF(logPrinter.get(), TAEL_ERROR, "ERROR: unexpected # fields in line of brokerage-specific-fees file (%d au lieu de %d)",
			(int) fields.size(), n_fields );
    for(unsigned int i=0; i< fields.size(); i++)
      TAEL_PRINTF(logPrinter.get(), TAEL_ERROR, "Field #%2d: '%s'", i, fields[i].c_str() );
    throw table_data_error( "ERROR: unexpected # fields in line of brokerage-specific-fees file" );
  }

  /////////////////////////
  // GET THE DATA
  /////////////////////////

  // get the ecn
  _ecn = ECN::ECN_parse( fields[0].c_str() );
  if( _ecn == ECN::UNKN ) {
    TAEL_PRINTF(logPrinter.get(), TAEL_ERROR, "ERROR: Unfamiliar ECN (%s) in EcnFees::EcnFees", fields[0].c_str() );
    throw table_data_error( "ERROR: unfamiliar ECN in line of brokerage-specific-fees file" );
  }

  // get the fees
  for( int i=1; i<n_fields; i++ ) {
    string name = fieldNames[i];
    // the value
    double value = 0.0;
    if( fields[i] == NA_STRING ) 
      TAEL_PRINTF(logPrinter.get(), TAEL_WARN, "WARNING: Unknown value for the fee %s of %s", name.c_str(), ECN::desc( _ecn ) );
    else
      value = atof( fields[i].c_str() );

    // the tape
    string tapeName = name.substr( 0,6 );
    Mkt::Tape tape;
    if(       tapeName == string("TAPE_A") ) tape = Mkt::TAPE_A; 
    else if ( tapeName == string("TAPE_B") ) tape = Mkt::TAPE_B; 
    else if ( tapeName == string("TAPE_C") ) tape = Mkt::TAPE_C; 
    else {
      TAEL_PRINTF(logPrinter.get(), TAEL_ERROR, "ERROR: Unfamiliar Tape (%s) in EcnFees::EcnFees", tapeName.c_str() );
      throw table_data_error( "ERROR: unfamiliar Tape in header file of brokerage-specific-fees file" );
    }

    // which fee
    string feeName = name.substr( 7 );
    if(      feeName == string("TAKE") )  _takeLiqFee[tape]   = value;
    else if( feeName == string("ADD") )   _addLiqRebate[tape] = -value; // transforming rebate to fee
    else if( feeName == string("ROUTE") ) _routingFee[tape]   = value; 
    else {
      TAEL_PRINTF(logPrinter.get(), TAEL_ERROR, "ERROR: Unfamiliar Fee Type (%s) in EcnFees::EcnFees", feeName.c_str() );
      throw table_data_error( "ERROR: unfamiliar fee-type in header file of brokerage-specific-fees file" );
    }
  }
}

EcnFees::EcnFees( ECN::ECN ecn ) 
  : _ecn( ecn ),
    _feesKnown( false )
{
  for( int i=0; i<Mkt::NUM_TAPES; i++ ) {
    _takeLiqFee[i] = 0.0;
    _addLiqRebate[i] = 0.0;
    _routingFee[i] = 0.0;
  }
}
