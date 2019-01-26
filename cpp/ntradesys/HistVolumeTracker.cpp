#include <cl-util/table.h>
#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>
#include <cl-util/float_cmp.h>

#include "HistVolumeTracker.h"

const string SYMBOL_FIELD_NAME = "symbol";
const string TOTAL_VOLUME_FIELD_NAME = "TOTAL(K shs)";
const int MIN_N_FIELDS = 6;
const int SHS_UNITS_OF_TOTAL_VOLUME_FIELD = 1000; // i.e., the total-volume fields in in 1000s of shares

const float SymbolHistVolumeTracker::DEFAULT_VOLUME_WHEN_NO_VOLUME_DATA_IS_AVAILABLE = 1.0; // w\ equal distribution across all ECNs in Mkt

#ifndef ABS
#define ABS(x) ((x)>0?(x):(-x))
#endif

const string HIST_VOLUME_DISTRIBUTION_FILE = "volume_distr";
const string RELEVANT_LOG_FILE = "trader";

HistVolumeTracker::HistVolumeTracker() 
{
  // first, get a pointer to DataManager and to debug-printer from the factory
  factory<DataManager>::pointer  dm         = factory<DataManager>::find(only::one);
  if( !dm ) 
    throw std::runtime_error( "Failed to get DataManager from factory (in HistVolumeTracker::HistVolumeTracker)" );
  factory<debug_stream>::pointer logPrinter = factory<debug_stream>::get( RELEVANT_LOG_FILE );

  // second, get a map from symbol to hist-volume-distribution based on the hist-volume-distribution-file
  file_table<SymbolHistVolumeTracker> symToVolDistrTable( HIST_VOLUME_DISTRIBUTION_FILE );

  // now go over the symbols in dm:
  _data.resize( dm->cidsize() );
  for( int cid=0; cid<dm->cidsize(); cid++ ) {
    const char* symbol = dm->symbol( cid );
    file_table<SymbolHistVolumeTracker>::iterator it = symToVolDistrTable.find(symbol);
    if( it != symToVolDistrTable.end() ) {
      // create a copy of the SymbolHistVolumeTracker object
      _data[ cid ] = new SymbolHistVolumeTracker( it -> second ); // "it" is a pair of (key,value)
    } else {
      TAEL_PRINTF(logPrinter.get(), TAEL_ERROR, "%-5s ERROR: Couldn't get historical volume-distribution data from param-file. Assigning "
			  "default values (equally distributed volume accross all ECNs in ECN::ECN enum, with total of %.1f sh/day.",
			  symbol, SymbolHistVolumeTracker::DEFAULT_VOLUME_WHEN_NO_VOLUME_DATA_IS_AVAILABLE );
      _data[ cid ] = new SymbolHistVolumeTracker( symbol );
    }
  }
}

HistVolumeTracker::~HistVolumeTracker()
{
  for( unsigned int i=0; i<_data.size(); i++ )
    delete _data[ i ];
}

SymbolHistVolumeTracker::SymbolHistVolumeTracker( const vector<string>& fieldNames, const vector<string>& fields ) 
  : _relativeVolPerECN( ECN::ECN_size )
{

  factory<debug_stream>::pointer  logPrinter = factory<debug_stream>::get( RELEVANT_LOG_FILE );

  int n_fields = fieldNames.size();

  /////////////////////////
  // CHECK HEADER LINE
  /////////////////////////

  if( n_fields < MIN_N_FIELDS ) {
    TAEL_PRINTF(logPrinter.get(), TAEL_ERROR, "ERROR: historical-volume-distribution file has two few fields in header. "
			"(has %d where the minimum is %d)", n_fields, MIN_N_FIELDS );
    throw table_data_error( "Header line of hist-volume-dirtibution has too few fields" );
  }
  int expected_n_fields = (int)Ex::Ex_size + 2;
  if( n_fields != expected_n_fields ) { // +2 is for the fields symbol & total-vol
    TAEL_PRINTF(logPrinter.get(), TAEL_WARN, "WARNING: historical-volume-distribution file has unexpected number of fields in header. "
			"(it's %d while %d is expected)", n_fields, expected_n_fields );
  }
  if( fieldNames[0] != SYMBOL_FIELD_NAME ) {
    TAEL_PRINTF(logPrinter.get(), TAEL_ERROR, "ERROR: historical-volume-distribution file has the wrong first field name in header. "
			"(it's %s while %s is expected)", fieldNames[0].c_str(), SYMBOL_FIELD_NAME.c_str() );
    throw table_data_error( string("The first field-name in the header line of volume-distr should be ") + SYMBOL_FIELD_NAME );
  }
  if( fieldNames[n_fields-1] != TOTAL_VOLUME_FIELD_NAME ) {
    TAEL_PRINTF(logPrinter.get(), TAEL_ERROR, "ERROR: historical-volume-distribution file has the wrong last field name in header. "
			"(it's %s while %s is expected)", fieldNames[n_fields-1].c_str(), TOTAL_VOLUME_FIELD_NAME.c_str() );
    throw table_data_error( string("last field-name in the header line of volume-distr should be " ) + TOTAL_VOLUME_FIELD_NAME );
  }
  
  // get the ECNs the header-fields refer to 
  vector<ECN::ECN> ecnsInFile( n_fields - 2 );
  for( int i=0; i<n_fields-2; i++ ) {
    const string& exchangeStr = fieldNames[ i+1 ];
    ecnsInFile[ i ] = Ex::ExToECN( Ex::Ex_parse(exchangeStr.c_str()) );
  }

  /////////////////////////
  // CHECK DATA LINE
  /////////////////////////

  // if input line is empty...
  if( fields.size() == 0 ) {
    TAEL_PRINTF(logPrinter.get(), TAEL_ERROR, "ERROR: got an empty line from hist-volume-distribution file" );
    throw table_data_error( "ERROR: got an empty line from hist-volume-distribution file" );
  }

  // if there's different number of fields Vs. fieldNames, throw exception
  if( fields.size() != n_fields ) {
    TAEL_PRINTF(logPrinter.get(), TAEL_ERROR, "ERROR: unexpected # fields in line of hist-volume-distribution file (%d au lieu de %d, first field in line is %s)",
		(int) fields.size(), n_fields, fields[0].c_str() );
    throw table_data_error( "ERROR: unexpected # fields in line of hist-volume-distribution file" );
  }

  /////////////////////////
  // GET THE DATA
  /////////////////////////

  // get the symbol
  _symbol = fields[0];

  // get the total volume
  _totalVol = atof( fields[n_fields-1].c_str() ) * SHS_UNITS_OF_TOTAL_VOLUME_FIELD;

  // if it's zero, assign the default volume numbers and return
  if( _totalVol == 0.0 ) {
    TAEL_PRINTF(logPrinter.get(), TAEL_WARN, "WARNING: volume-distribution-file contains no volume data for %s. "
			"Assigning default volume-distribution numbers in SymbolHistVolumeTracker::SymbolHistVolumeTracker.", 
			_symbol.c_str() );
    assignDefaultVolValues();
    return;
  }

  // get the relative-volume-per-ECN
  float sumOfRelativeVol = 0.0;
  for( int i=0; i<n_fields-2; i++ ) {
    ECN::ECN ecn = ecnsInFile[ i ];
    float    relativeVol = atof( fields[i+1].c_str() ) / 100;
    _relativeVolPerECN[ (int)ecn ] += relativeVol;
    sumOfRelativeVol += relativeVol;
  }
 
  // verify the sum of volume-percentages is close to 1
  if( !clite::util::cmp<2>::EQ(sumOfRelativeVol,1) )
    TAEL_PRINTF(logPrinter.get(), TAEL_WARN, "WARNING: percentages for %s in volume distribution file do not sum up to ~100 (it sums to %.2f)",
			_symbol.c_str(), sumOfRelativeVol*100 );

}

SymbolHistVolumeTracker::SymbolHistVolumeTracker( const string& symbol )
  : _symbol( symbol ),
    _relativeVolPerECN( ECN::ECN_size )
{
  assignDefaultVolValues();
}

SymbolHistVolumeTracker::SymbolHistVolumeTracker( const char*   symbol )
  : _symbol( symbol ),
    _relativeVolPerECN( ECN::ECN_size )
{
  assignDefaultVolValues();
}

void SymbolHistVolumeTracker::assignDefaultVolValues() {

  _totalVol = DEFAULT_VOLUME_WHEN_NO_VOLUME_DATA_IS_AVAILABLE;
  
  // uniform distribution across ECNs
  _relativeVolPerECN.assign( (int)ECN::ECN_size, 1.0/(int)ECN::ECN_size ); 
}
