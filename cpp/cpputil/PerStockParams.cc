/*
  PerStockParams.cc - Utility class that:
  - Uses keyTable.
  - To construct a hash table which is:
    - Indexed by CID.
    - Holds map from cid --> set of all field value specified for stock with that symbol
      in input file.  
*/

#include "PerStockParams.h"
#include "StringConversion.h"


/*************************************************************
  PerStockParams code
*************************************************************/
PerStockParams::PerStockParams() :
  _kt()
{

}

PerStockParams::~PerStockParams() {

}

void PerStockParams::addEntry(int cid, vector<string> &fields) {
  _kt.addEntry(cid, fields);
}

bool PerStockParams::lookupEntry(int cid, vector<string> &fv) {
  return _kt.lookupEntry(cid, fv);
}

void PerStockParams::clear() {
  _kt.clear();
}

int PerStockParams::populateValues(DataManager *dm, int field, dynamic_bitset<> &found, vector<string> &fv) {
  // Set found & fv to proper sizes - 1 entry for each stock in population set.
  int sz = dm->cidsize();
  found.resize(sz);
  fv.resize(sz);
  
  // Walk through all stocks in population set, trying to extract specified field.
  vector<string> fields;
  int ret = 0;
  found.reset();
  for (int i=0; i<sz;i++) {
    if (!lookupEntry(i, fields)) {
      continue;
    }
    if ((field-1) >= (int) fields.size()) {
      continue;
    }
    found.set(i);
    fv[i] = fields[field-1];
    ret++;
  }
  return ret;
}

int PerStockParams::populateValues(DataManager *dm, int field, dynamic_bitset<> &found, vector<int> &fv) {
  // Set found & fv to proper sizes - 1 entry for each stock in population set.
  int sz = dm->cidsize();
  found.resize(sz);
  fv.resize(sz);
  
  // Walk through all stocks in population set, trying to extract specified field.
  vector<string> fields;
  int ret = 0;
  found.reset();
  for (int i=0; i<sz;i++) {
    if (!lookupEntry(i, fields)) {
      continue;
    }
    if ((field-1) > (int) fields.size()) {
      continue;
    }
    string sval = fields[field-1];
    int ival;
    if (!StringConversion::stoi(sval, &ival, 0)) {
      continue;
    }
    found.set(i);
    fv[i] = ival;
    ret++;
  }
  return ret;
}

int PerStockParams::populateValues(DataManager *dm, int field, dynamic_bitset<> &found, vector<double> &fv) {
  // Set found & fv to proper sizes - 1 entry for each stock in population set.
  int sz = dm->cidsize();
  found.resize(sz);
  fv.resize(sz);
  
  // Walk through all stocks in population set, trying to extract specified field.
  vector<string> fields;
  int ret = 0;
  found.reset();
  for (int i=0; i<sz;i++) {
    if (!lookupEntry(i, fields)) {
      continue;
    }
    if ((field-1) > (int) fields.size()) {
      continue;
    }
    string sval = fields[field-1];
    double dval;
    if (!StringConversion::stod(sval, &dval, 0.0)) {
      continue;
    }
    found.set(i);
    fv[i] = dval;
    ret++;
  }
  return ret;
}

int PerStockParams::populateKeys(DataManager *dm, dynamic_bitset<> &found, vector<string> &fv) {
  // Set found & fv to proper sizes - 1 entry for each stock in population set.
  int sz = dm->cidsize();
  found.resize(sz);
  fv.resize(sz);
  
  // Walk through all stocks in population set, trying to extract key.
  vector<string> fields;
  int ret = 0;
  found.reset();
  for (int i=0; i<sz;i++) {
    if (!lookupEntry(i, fields)) {
      continue;
    }
    const char *sym = dm->symbol(i);
    string symS(sym);
    found.set(i);
    fv[i] = symS;
    ret++;
  }
  return ret;
}

/*************************************************************
  PerStockParamsReader code
*************************************************************/
int PerStockParamsReader::readFile(DataManager *dm, string &fname, string &fs, int header, PerStockParams &fv, int symbolField) {
  
  // Clear fv.
  fv.clear();

  // Make a KeyTable<string, vector<string>.
  // Use KeyTableReader to read in from file.
  // This should give you a KeyTable that maps from symbol --> vector of params.
  KeyTable<string, string > ktSymbol;
  int nf = KeyTableReader<string, string>::readFile(fname, fs, header, ktSymbol, symbolField);

  if (nf == 0) {
    return nf;
  }
  
  // Convert from KeyTable that maps from symbol --> vector of params
  //   to KeyTable that maps from cid --> vector of params.
  // This is a PerStockParams, so this operation in equivalent to 
  //   populating fv, which we try to do as follows:
  //   - Walk through all entries in KeyTable, extracting symbol + values.
  //   - Try to map from symbol --> cid.
  //   - If you get an exact match, insert an entry with cid --> values
  //      (not symbol --> values) into fv.
  //   - If you get something with no match, ignore it.
  int rv = 0;
  map<string, vector<string> > *umap = ktSymbol.umap();
  map<string, vector<string> >::const_iterator ktSymbolIter;
  for (ktSymbolIter = umap->begin(); ktSymbolIter != umap->end(); ++ktSymbolIter) {
    string symbol = ktSymbolIter->first;
    StringConversion::stripQuotes(symbol);
    int cid = dm->cid(symbol.c_str());
    if (cid != -1) {
      vector<string> tmp = ktSymbolIter->second;
      fv.addEntry(cid, tmp);
      rv++;
    }
  }  
  return rv;
}
