#include <cl-util/table.h>
#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>

#include "DataManager.h"
#include "ExchangeTracker.h"

using namespace clite::util;

const string EXCHANGES_FILE = "exchanges";
const double SUSPICIOUS_UNKN_PERCENTAGE = 0.2;
const string RELEVANT_LOG_FILE = "trader";

ExchangeTracker::ExchangeTracker() :
  _data(0)
{

  // first, get a pointer to DataManager and to debug-printer from the factory
  factory<DataManager>::pointer dm = factory<DataManager>::find(only::one);
  if( !dm ) 
    throw std::runtime_error( "Failed to get DataManager from factory (in ExchangeTracker::ExchangeTracker)" );
  factory<debug_stream>::pointer  logPrinter = factory<debug_stream>::get( RELEVANT_LOG_FILE );

  // second, get a map from symbol to exchanges based on the exchanges-file
  file_table<SymbolExchangeTracker> symToExcTable( EXCHANGES_FILE );
  if ( symToExcTable.empty() ){
    throw std::runtime_error( "No symbols read in by ExchangeTracker::ExchangeTracker (input_file=" + 
			      EXCHANGES_FILE + ")");   
  }
  // now go over the symbols in dm:
  _data.resize( dm->cidsize() );
  for( int cid=0; cid<dm->cidsize(); cid++ ) {
    const char* symbol = dm->symbol( cid );
    file_table<SymbolExchangeTracker>::iterator it = symToExcTable.find(symbol);
    if( it != symToExcTable.end() ) {
      // create a copy of the SymbolExchangeTracker object
      _data[ cid ] = new SymbolExchangeTracker( it->second ); // "it" is a pair of (key,value)
      TAEL_PRINTF(logPrinter.get(), TAEL_INFO, "%-5s Listed on %s", symbol, ECN::desc(_data[cid]->getExchange()) );
    } else {
      TAEL_PRINTF(logPrinter.get(), TAEL_ERROR, "%-5s ERROR: Couldn't get exchange from param-file. Exchange is set to %s.",
			 symbol, ECN::desc(ECN::UNKN) );
      //      _data[ cid ] = new SymbolExchangeTracker( symbol, SymbolExchangeTracker::guessExchange(symbol) );
      _data[ cid ] = new SymbolExchangeTracker( symbol, ECN::UNKN );
    }
  }

  // if there are too many UNKNs: throw an exception
  int nUnknowns = 0;
  for( int cid=0; cid<dm->cidsize(); cid++ ) 
    if( getExchange(cid) == ECN::UNKN ) nUnknowns++;
  if( nUnknowns > SUSPICIOUS_UNKN_PERCENTAGE * dm->cidsize() )
    throw std::runtime_error( "Suspiciously many symbols with unknown exchanges in ExchangeTracker::ExchangeTracker" );

  // for now, in order to make the move to this new code more smooth, I need to fill in a real values for each exchange, so 
  // instead of UNKNOWN, we guess the exchange. (I leave in as a separate 'paragraph' of the code so we can remove it easily).
  for( int cid=0; cid<dm->cidsize(); cid++ ) {
    if( getExchange(cid) == ECN::UNKN ) {
      delete _data[cid];
      const char* symbol = dm -> symbol(cid);
      _data[cid] = new SymbolExchangeTracker( symbol, SymbolExchangeTracker::guessExchange(symbol) );
      TAEL_PRINTF(logPrinter.get(), TAEL_ERROR, "%-5s Had to guess the exchange, and got %s", symbol, ECN::desc(getExchange(cid)) );
    }
  }      
}

ExchangeTracker::~ExchangeTracker()
{
  for( unsigned int i=0; i<_data.size(); i++ )
    delete _data[ i ];
}
  

SymbolExchangeTracker::SymbolExchangeTracker( const vector<string>& fieldNames, const vector<string>& fields ) 
{
  // assert some conditions
  if( fields.size() < 2 )
    throw table_data_error( "at SymbolExchangeTracker::SymbolExchangeTracker: Too few fields in line" );
  
  _symbol = fields[0];
  _exchange = exchangeStrToExchange( fields[1] );

  // we should log something if exchange is unknown
  if( _exchange == ECN::UNKN ) {
    factory<debug_stream>::pointer  logPrinter = factory<debug_stream>::get( RELEVANT_LOG_FILE );
    if( fields[1].compare("N.A.") == 0 || // This is the value that the exchanges file currently has for an unknown exchange
	fields[1].compare("UNKN") == 0 )
      TAEL_PRINTF(logPrinter.get(), TAEL_WARN, "WARNING: Unknown exchange for symbol %s in param-file", _symbol.c_str() );
    else
      TAEL_PRINTF(logPrinter.get(), TAEL_ERROR, "ERROR: Unrecognized exchange (%s) for symbol %s. Assuming UNKN.",
			  fields[1].c_str(), _symbol.c_str() );
  }
}

SymbolExchangeTracker::SymbolExchangeTracker( const string& symbol, ECN::ECN exchange )
  : _symbol( symbol ),
    _exchange( exchange ) {}

SymbolExchangeTracker::SymbolExchangeTracker( const char*   symbol, ECN::ECN exchange )
  : _symbol( symbol ),
    _exchange( exchange ) {}
  
Mkt::Tape SymbolExchangeTracker::exchangeToTape( ECN::ECN exchange ) {
  
  switch( exchange ) {
    case ECN::NYSE: return Mkt::TAPE_A;
    case ECN::ISLD: return Mkt::TAPE_C;
    case ECN::UNKN: return Mkt::TAPE_UNKN;
    default:        return Mkt::TAPE_B; // based on an email sent to Doron from limebrokerage on 20090624
  }
}

ECN::ECN SymbolExchangeTracker::exchangeStrToExchange( const string& exchangeStr ) {
  if( exchangeStr.compare("NSDQ") == 0 )
    return ECN::ISLD;
  return ECN::ECN_parse( exchangeStr.c_str() );
}

ECN::ECN SymbolExchangeTracker::guessExchange( const char* symbol ) {
  string sym( symbol );
  if( sym.size() <= 3 )
    return ECN::NYSE;
  return ECN::ISLD;
}
