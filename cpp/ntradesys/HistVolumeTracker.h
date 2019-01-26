/**
 * Reads data about the historical daily volume distribution of a particular symbol tracked along the last ~30 days or so.
 * In particular, it keeps the average daily volume for each ECN (or at least each of those which the hist-volume-tracker
 * program listens to, either directly or through siac/utdf).
 */

#ifndef __HISTVOLUMETRACKER_H__
#define __HISTVOLUMETRACKER_H__

#include <vector>
#include <string>

#include "DataManager.h"

#include "Markets.h"

using std::vector;
using std::string;
using namespace clite::util;

/// A Hist-volume-tracker for a single symbol, which can be used by file_table (of cl-util)
class SymbolHistVolumeTracker {

public: 

  typedef std::string key_type;  // the key-word "key_type" is used in clite::util::file_table
  /// returns the symbol this line refers to
  std::string const &get_key() const { return _symbol; }

  /**
  * fields - those in the line dedicated to a specific symbol; fieldNames - the header line
  * Note: if the total-volume field is 0, we initialize all ecn-volume fields to some constant small arbitrary value,
  *       which means that all ECNs are treated equally for this symbol, but we don't assume it's not traded on any of them.
  *       (This is particularly important for brand-new symbols with no historical volume. Volume of zero might be interpreted in
  *       a wrong way).
  */
  SymbolHistVolumeTracker( const vector<string>& fieldNames, const vector<string>& fields );

  /// initializing the volume data with the default values (equal small volume on all ECNs)
  SymbolHistVolumeTracker( const string& symbol );
  SymbolHistVolumeTracker( const char*   symbol );
  
  /// returns (the percentage of) the volume on a particular ECN
  double getAvgDailyVol( ECN::ECN ecn ) const { return _relativeVolPerECN[ecn] * _totalVol; }
  double getAvgDailyVol() const { return _totalVol; }
  double getRelativeVol( ECN::ECN ecn ) const { return _relativeVolPerECN[ecn]; }

  const static float DEFAULT_VOLUME_WHEN_NO_VOLUME_DATA_IS_AVAILABLE;

protected:
  string        _symbol;
  vector<float> _relativeVolPerECN; /// relative part (in [0,1]) of each ECN-volume out of the total volume in market hours 
                                    /// (the number that appears in the volume-distribution file)
  double        _totalVol;    /// this is never zero

private:
  void   assignDefaultVolValues();

};

/// A historical-volume tracker for all symbols referred to by DataManager
class HistVolumeTracker {
public:

  /// create an exchange-tracker from the "exchanges" file using file_table, and completing it with default values for all those 
  /// symbols that were not found in the file
  HistVolumeTracker();
  virtual ~HistVolumeTracker();
  
  double getAvgDailyVol( unsigned int cid, ECN::ECN ecn ) const { return _data[cid]->getAvgDailyVol( ecn ); } /// avg # shares on ecn
  double getAvgDailyVol( unsigned int cid )               const { return _data[cid]->getAvgDailyVol(); }      /// avg # shares 
  double getRelativeVol( unsigned int cid, ECN::ECN ecn ) const { return _data[cid]->getRelativeVol( ecn ); } /// A number in [0,1]

private:
  vector<SymbolHistVolumeTracker*> _data;
  
};

#endif    // __HISTVOLUMETRACKER_H__
