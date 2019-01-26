// Read in snapshots of alpha from files
#ifndef __ALPHA_FROM_FILE__
#define __ALPHA_FROM_FILE__

#include <vector>
#include <map>
#include <string>
#include <cl-util/Configurable.h>

using std::vector;
using std::map;
using std::string;

#include "AlphaSignal.h"

class DataManager;


// copied from debug_stream.h
// goal: create a class that has a static member that is configurable
// this this class can be configured, and another class can grab this one for info
class alpha_from_file_config {

 protected:
  alpha_from_file_config(){ }
  alpha_from_file_config( const alpha_from_file_config &a){ }

 public:
  struct config : public Configurable {
    friend class alpha_from_file_config;
    private:
    config ( ) : Configurable("alpha-from-file") {
      defOption("path", &path_, "path to alpha files", ".");
      defOption("suffix", &suffix_, "suffix for alpha files", ".snap");
      defOption("exchanges-file", &exchanges_, "the exchanges file");  
    }
    std::string path_, suffix_, exchanges_;
    public:
    const std::string &path() const { return path_; }
    const std::string &suffix() const { return suffix_; }
    const std::string &exchangesFile() const { return exchanges_; }
    operator bool() const {
      return !path_.empty();
    }
  };

  static config *get_config () {
    static config cfg;
    return &cfg;
  }
};

class AlphaFromFile : public AlphaSignal {
 public: 
  typedef  map<double, double> alpha_map_t;
  typedef  vector<alpha_map_t> alpha_table_t;
  
  AlphaFromFile(); // CTOR
  virtual ~AlphaFromFile(); // DTOR

  // read in one file containing alphas
  void readAlphaFile(int cid, int date);

  void readExchangesFile();

  // standard AlphaSignal interface functions.
  virtual bool getAlpha(int cid, double &alpha);
  
  // subclass specific interface functions
  virtual bool setAlpha(int cid, double alpha);
  
  // get the alpha for this cid at this time
  double getAlphaAtTime(int cid, double secSinceMid);

 private:
  factory<DataManager>::pointer _dm;
  string _alphaDir;                            // where to find alpha files
  string _alphaSuf;                            // alpha file suffix
  map<string, string> _exchMap;                // sym -> exch
  alpha_table_t _alphaTable;
};

#endif  // __ALPHA_FROM_FILE__

