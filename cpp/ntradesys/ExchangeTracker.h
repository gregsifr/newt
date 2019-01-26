/*
 *  SymbolExchangeTracker: a simple class that gets the exchange of a symbol from the parameter-file and keeps it
 *  ExchangeTracker:       keeps a SymbolExchangeTracker for each symbol referred to by DataManager
 *
 */

#ifndef _EXCHANGETRACKER_H_
#define _EXCHANGETRACKER_H_

#include <vector>
#include <string>

#include "Markets.h"

using std::vector;
using std::string;

/// An exchange-tracker for a single particular symbol
class SymbolExchangeTracker {

public:

  typedef std::string key_type;  // the key-word "key_type" is used in clite::util::file_table
  /// returns the symbol this line refers to
  std::string const &get_key() const { return _symbol; }


  /// fields - those in the line dedicated to a specific symbol; fieldNames - the header line (ignored for now)
  SymbolExchangeTracker( const vector<string>& fieldNames, const vector<string>& fields );
  /// for manual initialization
  SymbolExchangeTracker( const string& symbol, ECN::ECN exchange );
  SymbolExchangeTracker( const char*   symbol, ECN::ECN exchange );
  
  /// get exchange, get listing-tape
  ECN::ECN  getExchange() const { return _exchange; }
  Mkt::Tape getTape() const { return exchangeToTape(_exchange); }

  /// mapping Exchanges to listing-tapes
  static Mkt::Tape exchangeToTape( ECN::ECN exchange );

  /// convert exchange-string to exchange
  static ECN::ECN exchangeStrToExchange( const string& exchangeStr );

  /// guess the exchange by the length of the symbol: This is VERY RISKY and should be avoided as much as possible.
  static ECN::ECN guessExchange( const char* symbol );  

  // FUTURE ADDITION: is it an ETF ? (Might be important, for example when trading the ECN SLK).

protected:

  string    _symbol;
  ECN::ECN  _exchange;

};

/// An exchange-tracker for all symbols referred to by DataManager
class ExchangeTracker {
public:

  /// create an exchange-tracker from the "exchanges" file using file_table, and completing it with default values for all those 
  /// symbols that were not found in the file
  ExchangeTracker();
  virtual ~ExchangeTracker();
  
  ECN::ECN  getExchange( unsigned int cid ) const { return _data[cid]->getExchange(); }
  Mkt::Tape getTape    ( unsigned int cid ) const { return _data[cid]->getTape(); }

private:
  vector<SymbolExchangeTracker*> _data;
  
};

# endif // _EXCHANGETRACKER_H_
