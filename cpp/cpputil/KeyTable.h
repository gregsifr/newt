/*
  KeyTable.h  Utility for reading config files.  
  Parses config file which is assumed to have:
  - Multiple lines.
  - With 1 record per line.
  - Where each record can potentially have a different number of fields.
*/

#ifndef __KEYTABLE_H__
#define __KEYTABLE_H__

#include "SimpleAwkParser.h"

#include<map>
using std::map;

#include<utility>
using std::pair;

#include <cstring>

/*
  Templated map from key --> n-tuple, where n may differ from key to key.
  E.g. can be used to hold map from string --> n-tuple of strings.
*/
template <class keyt, class valt> class KeyTable {
 protected:
  map<keyt, vector<valt> > _map;

 public:  
  KeyTable();
  virtual ~KeyTable();

  // Add an entry from k --> vals.
  // Overrides previous entry.
  void addEntry(keyt &k, vector<valt> &vals);
  
  // Check for an entry from k --> vals.
  // If found, copy into fv.
  bool lookupEntry(keyt &k, vector<valt>&fv);

  // Clear table.
  void clear();

  // Number of entries in table.
  unsigned int size() {return _map.size();}

  // Pass through access to _map.  Advanced swim.
  map<keyt, vector<valt> > *umap() {return &_map;}
};


/*
  Reads in a KeyTable from a file.
*/
template <class keyt, class valt> class KeyTableReader {
 public:

  // Read in file:
  // - parse each line.
  // - attempt to tokenize.
  // - insert into _kt(key = $1, values = $2...$NF).
  // - Return # of records successfully parsed.
  // - Each record should be of form:
  //   Key --> n-tuple of {Value1, Value2, ....} 
  //       (not Key --< {Key, Value1, Value2, ...})
  // header:  0 for no header line.
  //          1 for header line.
  //          -1 for KeyTableReader should try to guess whether theres a header line.
  static int readFile(string &fname, string &fs, int header, KeyTable<keyt, valt> &fv, int startField);

  // Convert from string to valt.
  static valt fromString(string &s);
};



/******************************************************************
  KeyTable Code 
******************************************************************/
template <class keyt, class valt> 
KeyTable<keyt, valt>::KeyTable() {

}

template <class keyt, class valt> 
KeyTable<keyt, valt>::~KeyTable() {

}

template <class keyt, class valt> 
void KeyTable<keyt, valt>::addEntry(keyt &k, vector<valt> &vals) {
  // Delete old entry under k, if present.
  typename map<keyt, vector<valt> >::iterator it;
  it = _map.find(k);
  if (it != _map.end()) {
    _map.erase(it);
  }
  pair<keyt, vector<valt> > p(k, vals);
  _map.insert(p);
}

template <class keyt, class valt> 
bool KeyTable<keyt, valt>::lookupEntry(keyt &k, vector<valt>&fv) {
  typename map<keyt, vector<valt> >::iterator it;
  it = _map.find(k);
  if (it != _map.end()) {
    fv = it->second;
    return true;
  }
  fv.resize(0);
  return false;  
}

template <class keyt, class valt> 
void KeyTable<keyt, valt>::clear() {
  _map.clear();
}

/******************************************************************
  KeyTableReader Code 
******************************************************************/

template <class keyt, class valt> 
valt KeyTableReader<keyt, valt>::fromString(string &s) {
  std::istringstream stream (s);
  valt t;
  stream >> t;
  return t;

}

template <class keyt, class valt> 
int KeyTableReader<keyt, valt>::readFile(string &fname, string &fs, int header, KeyTable<keyt, valt> &fv, int startField) {
  int numRecords = 0, numDataFields = 0;
  keyt k;
  vector<valt> payload;
  SimpleAwkParser p;
  string ticker("ticker"), tmp;
  p.setFS(fs);
  fv.clear();
  
  if (!p.openF(fname)) {
    return 0;
  }

  while (p.getLine()) {
    if (p.NR() == 1 && header == 1) {
      continue;
    }
    if (p.NR() == 1 && header == -1 && 
	  p.getField(startField).compare(ticker) == 0) {
      continue;
    }

    numDataFields = p.NF()-startField;
    if (numDataFields < 0) {
      continue;
    }
    
    payload.clear();
    payload.resize(numDataFields);
    for (int i=0;i<numDataFields;i++) {
      tmp = p.getField(i+startField+1);
      valt tmp2 = fromString(tmp);
      payload[i] = tmp2;
    }
    tmp = p.getField(startField);
    k = fromString(tmp);

    fv.addEntry(k, payload);

    numRecords++;
  }

  p.closeF();

  return numRecords;
}


#endif  // __KEYTABLE_H__
