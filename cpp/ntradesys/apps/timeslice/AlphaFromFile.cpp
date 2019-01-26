// Read in alphas from a file
#include <iostream>
#include <fstream>

#include <cl-util/factory.h>
using namespace clite::util;

#include "DataManager.h"
#include "AlphaFromFile.h"

using std::cerr;
using std::ifstream;

// CTOR
AlphaFromFile::AlphaFromFile() :
  _dm( factory<DataManager>::get(only::one) )
{
  cerr << "AlphaFromFile::CTOR\n";

  alpha_from_file_config::config *alpha_cfg = alpha_from_file_config::get_config();
  if ( !alpha_cfg ){
    cerr << "AlphaFromFile::CTOR alpha_from_file_config not configured\n";
    exit(1);
  }
  
  _alphaDir = alpha_cfg->path();
  _alphaSuf = alpha_cfg->suffix();

  int idate = _dm->getSDate();

  // expand to hold alpha for all cids
  _alphaTable.resize(_dm->cidsize());

  // read in map of where each symbol is trading
  readExchangesFile();

  // read in all alpha files
  for ( int cid = 0 ; cid < _dm->cidsize() ; ++cid ){
    cerr << " about to call readAlphaFile(" << cid << ", " << idate << ")\n";
    readAlphaFile(cid, idate);
  }

  cerr << "AlphaFromFile::CTOR read in " << _alphaTable.size() << " alpha files\n";

}

// DTOR
AlphaFromFile::~AlphaFromFile(){
}

// NOT USED - read alphas from file in CTOR
bool AlphaFromFile::setAlpha(int cid, double alpha) {
  cerr << "AlphaFromFile::setAlpha() SHOULDN'T BE CALLED\n";
  assert(0);  // SHOULDN'T BE HERE!
}

// read in one day's worth of alphas for one symbol
void AlphaFromFile::readAlphaFile(int cid, int date) {
  cerr << "AlphaFromFile::readAlphaFile(cid " << cid << ", date " << date << ")\n";
  
  string sym = _dm->symbol(cid);

  char buf[16];
  sprintf(buf, "%8d", date);
  string dateStr = string(buf);

  // get exchange abbrev (Bloomberg notation)
  map<string, string>::iterator iSym;
  if ( (iSym = _exchMap.find(sym)) == _exchMap.end() ){
    cerr << "AlphaFromFile::setAlpha() Could not find exchange entry for " << sym << "\n";
    exit(1);
  }
  string exch = (*iSym).second;

  // file name _alphaDir/YYYYMMDD/SYM_EXCH.alpha.snap, where EXCH is the Bloomberg abbrev
  string alphaFile = _alphaDir  + "/" + dateStr + "/" + sym + "_" + exch + "." + _alphaSuf;

  cerr <<"AlphaFromFile::readAlphaFile(cid " << cid << ", date " << date << ") exch " << exch 
       << " alphaFile " << alphaFile << "\n";

  ifstream ifs(alphaFile.c_str());
  if (!ifs){
    cerr << "Could not open alphaFile " << alphaFile << "\n";
    exit(1);
  }

  assert( !(cid < 0) && cid < int(_alphaTable.size()) );

  alpha_map_t &am = _alphaTable[cid];
  double time, mid, fv, alpha;
  double timePrev = 0;
  int iLine = 0;
  while (ifs){
    ifs >> time >> mid >> fv >> alpha;
    if (0){
      cerr << "line " << iLine << " time " << time 
	   << " mid " << mid << " fv " << fv 
	   << " alpha " << alpha << "\n";
    }
    am.insert(std::make_pair(time, alpha));
    // times should not go backwards
    if ( time < timePrev ){
      cerr << "AlphaFromFile::readAlphaFile() read in time (" << time << ")"
	   << " that is earlier than previous time (" << timePrev << ")\n";
      exit(1);
    }
    timePrev = time;
  }
  ifs.close();

  cerr << "AlphaFromFile::readAlphaFile() cid " << cid << " sym " << sym
       << " read in " << am.size() << " pts\n";
}

void AlphaFromFile::readExchangesFile() {
  string exchangesFile = alpha_from_file_config::get_config()->exchangesFile();

  ifstream ifs(exchangesFile.c_str());
  if (!ifs){
    cerr << "Could not open exchangesFile " << exchangesFile << "\n";
    exit(1);
  }
  int BUF_SIZE = 1024;
  char buffer[BUF_SIZE];
  string sym, exch;
  char delim[2] = " ";
  while(ifs.getline(buffer, BUF_SIZE)){
    if ( buffer[0] == '#' ){
      cerr << "skipping comment line " << buffer << "\n";
      continue;
    }
    string sym  = string(strtok(buffer, delim));
    string exch = string(strtok(NULL,   delim));

    // convert to Bloomberg notation
    string bbExch;
    if        ( exch == "NYSE"){
      bbExch = "UN";
    } else if ( exch == "NSDQ" ){
      bbExch = "UQ";
    } else if ( exch == "AMEX" ){
      bbExch = "UA";
    } else if ( exch == "ARCA" ){
      bbExch = "UP";
    } else {
      cerr << "No exchange entry for symbol " << sym << "\n";
      exit(1);
    }

    _exchMap.insert(std::make_pair(sym, bbExch));
  }
  ifs.close();
  
  cerr << "AlphaFromFile::readExchangesFile() read in " << _exchMap.size() << " entries\n";
}

// return the current alpha for a cid
bool AlphaFromFile::getAlpha(int cid, double &alpha) {
  TimeVal tv = _dm->curtv();
  DateTime date = _dm->getSDate();
  TimeVal midnight = DateTime::getMidnight(date.getintdate());
  // TODO: get a better way to calculate sec since midnight!
  double time = double((tv - midnight).getComparisonInt() * .001);
  alpha = getAlphaAtTime(cid, time);

  cerr << "AlphaFromFile::getAlpha() time = " << time 
       << " setting alpha=" << alpha << "\n";
  return true;
}

double AlphaFromFile::getAlphaAtTime(int cid, double time) {
  cerr << "AlphaFromFile::getAlphaAtTime(cid " << cid << ", time " << time << ")\n";

  assert( !(cid < 0) && cid < int(_alphaTable.size()) );
  assert( !(time < 34200) && !(time > 57600) );

  alpha_map_t &am = _alphaTable[cid];

  //std::pair<alpha_map_t::iterator, alpha_map_t::iterator> range;
  //range = am.equal_range(time);

  if ( am.lower_bound(time) == am.begin() ){
    cerr << "Can't find an element with time < " << time << "\n";
    exit(1);
  }
  alpha_map_t::iterator iLo = am.lower_bound(time);
  iLo--;
  alpha_map_t::iterator iHi = am.upper_bound(time);
  
  double timeLo  = (*iLo).first;
  double timeHi  = (*iHi).first;

  double alphaLo = (*iLo).second;
  double alphaHi = (*iHi).second;
  
  cerr << "timeLo "   << timeLo  << " timeHi "  << timeHi 
       << " alphaLo " << alphaLo << " alphaHi " << alphaHi << "\n";

  assert( !(timeLo > timeHi) );
  assert( !(timeLo > time) && !(timeHi < time) );

  double frac, alpha;
  if (timeHi > timeLo){
    frac = (time-timeLo) / (timeHi - timeLo);
    alpha = alphaLo + frac * (alphaHi - alphaLo);
  } else {
    alpha = 0.5 * (alphaLo + alphaHi);
  }

  cerr << "timeLo "   << timeLo  << " timeHi "  << timeHi 
       << " alphaLo " << alphaLo << " alphaHi " << alphaHi 
       << " frac " << frac << " returning alpha " << alpha << "\n";
  return alpha;
}
