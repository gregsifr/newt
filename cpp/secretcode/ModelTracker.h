/*
 *  ModelTracker: a simple class that gets the exchange of a symbol from the parameter-file and keeps it
 *  ModelTracker:       keeps a ModelTracker for each symbol referred to by DataManager
 *  Adapted from ExchangeTracker code
 *
 */

#ifndef _MODELTRACKER_H_
#define _MODELTRACKER_H_

#include <vector>
#include <string>

#include "Markets.h"

#include <vector>
#include <string>

using std::vector;
using std::string;


/// An exchange-tracker for a single particular symbol
class SymbolModelTracker {

public:

  typedef std::string key_type;  // the key-word "key_type" is used in clite::util::file_table
  /// returns the symbol this line refers to
  std::string const &get_key() const { return _symbol; }


  /// fields - those in the line dedicated to a specific symbol; fieldNames - the header line (ignored for now)
  SymbolModelTracker( const vector<string>& fieldNames, const vector<string>& fields );
  /// for manual initialization
  SymbolModelTracker( const string& symbol );
  SymbolModelTracker( const char*   symbol );
  
  double linearBlend( double _q, double _p2, double _h1, double _h2, double _dpq, 
		      double _dpull, double _dpsd, double _ltm, double _imb, 
		      double _excessimb, double _aspd);
  int snprint(char *s, int n) const;
  void print();
  
  double cutoff;
  bool valid;
protected:

  string    _symbol;
  double k;
  double r;
  double meany;
  double stdy;
  double c,q,p2,h1,h2,dpull,dpq,dspd,ltm,crimb,aspd,excessimb;
  

};

class ModelTracker;

/// An exchange-tracker for all symbols referred to by DataManager
class ModelTracker {
public:

  /// create an exchange-tracker from the "exchanges" file using file_table, and completing it with default values for all those 
  /// symbols that were not found in the file
  ModelTracker();
  virtual ~ModelTracker();
  SymbolModelTracker * getModel(int cid) { return _data[cid]; }
  void print();
  void setallCutoffs(double cutoff);
  

  bool modelApplies( int cid ){ return (_enabled && _data[cid]->valid);}
  void enable( bool b) { _enabled = b; }

private:
  vector<SymbolModelTracker*> _data;
  bool _enabled;
  
};

# endif // _MODELTRACKER_H_
