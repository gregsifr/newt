/*
  PerStockParams.h - Utility class that:
  - Uses keyTable.
  - To construct a hash table which is:
    - Indexed by CID.
    - Holds map from cid --> set of all field value specified for stock with that symbol
      in input file.  
*/

#ifndef __PERSTOCKPARAMS_H__
#define __PERSTOCKPARAMS_H__

#include "KeyTable.h"
#include "DataManager.h"

#include <vector>
using std::vector;

#include <boost/dynamic_bitset.hpp>
using boost::dynamic_bitset;


/*
  Map from cid --> vector of strings representing field values found (in order) in input file.
*/
class PerStockParams {
  KeyTable<int, string > _kt;
 public:
  PerStockParams();
  virtual ~PerStockParams();

  // Add an entry mapping from cid --> fields to table.  Overrides previous such entry (if any).
  void addEntry(int cid, vector<string> &fields);
  // Lookup mapping from cid --> fields in table.
  bool lookupEntry(int cid, vector<string> &fv);
  // Cear table.
  void clear();

  // Populate vector with params value (converted to target type, if applicable).
  // Fields are assumed to be indexed 1...N (not 0...N-1), with the Value1 in Field1.
  //   (not Ticker in Field1).
  // Returns number of records found, and also populates char (boolean) vector
  //   indicating which records were populated.
  // For stocks for which:
  // - No entry is found in param file.
  // - The Param file entry does not have a sufficient # of fields.
  // - The specified field is empty:
  // Does not count as a found record, and does not populate fv[entry].
  int populateValues(DataManager *dm, int field, dynamic_bitset<> &found, vector<string> &fv);
  int populateValues(DataManager *dm, int field, dynamic_bitset<> &found, vector<int>    &fv);
  int populateValues(DataManager *dm, int field, dynamic_bitset<> &found, vector<double> &fv);
  int populateKeys(DataManager *dm, dynamic_bitset<> &found, vector<string> &fv);
};

/*
  Factory object that knows how to populate PerStockParams from config file. 
*/
class PerStockParamsReader {
 public:
  
  // Read contends of specified file into fv.
  // Return number of data records (excluding header, if any) found.
  static int readFile(DataManager *dm, string &fname, string &fs, int header, PerStockParams &fv, int symbolField=1);
};


#endif   //  __PERSTOCKPARAMS_H__


