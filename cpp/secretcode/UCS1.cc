/*
 *
 *  UCS1.cc  
 *  Unconditional (mid -> mid return predicting) alpha signal, V1.0.
 * 

 *   UCS1 signal:
 *   Linear Blend of:
 *   - KFRTETFSignal (using SPY + SPY-BETA as explanatory model).
 *   - ImbTracker/PLSOBSignal.
 *   Uses (Externally generated) parameter file holding betas wrt those two signals.
 *   Uses default values (for stocks that it cant find in parameter file) of:
 *   - KFRTETFSignal beta = 0.0.
 *   - ImbTracker/PLSOBSignal beta = 1.0.
 *   Aka, by default uses 100% PLSOB signal and 0% KFRT signal.
*/


#include "UCS1.h"

#include "IBVolatilityTracker.h"

#include "PerStockParams.h"
#include "StringConversion.h"

#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>
#include <clite/message.h>
using namespace clite::util;

#include <iostream>
#include <ostream>
using namespace std;

const string UCS_BETA_FILE = "ucs1betafile";
const string RELEVANT_LOG_FILE = "misc";

UCS1::UCS1() :
  _kfrtBeta(0),
  _imbBeta(0)
{
  _dm = factory<DataManager>::find(only::one);
  if( !_dm )
    throw std::runtime_error( "Failed to get DataManager from factory (in UCS1::UCS1)" ); 

  _ddebug = factory<debug_stream>::get( RELEVANT_LOG_FILE );
  if( !_ddebug )
    throw std::runtime_error( "Failed to get debug_stream from factory (in UCS1::UCS1)" ); 

  // ImbTracker - should be explicitly placed in factory system for other components to access.
  _imbSignal = factory<ImbTracker>::get(only::one);
  if( !_imbSignal)
    throw std::runtime_error( "Failed to get ImbTracker from factory (in UCS1::UCS1)" ); 

  // ETFKFRTSignal.                      useVolume   printTicks      etfBetaFile
  _kfrtSignal = factory<ETFKFRTSignal>::get(only::one);
  if( !_kfrtSignal )
    throw std::runtime_error( "Failed to get KFRTSignal from factory (in UCS1::UCS1)" );  

  // Default Betas - e.g. for stocks not found in ucsBetaFile.
  _kfrtBeta.resize(_dm->cidsize(), 0.0);
  _imbBeta.resize(_dm->cidsize(), 1.0);


  // Parse config file holding stock --> ETF + beta mapping,
  if (!parseParamsFile()) {
    char buf[256];
    snprintf(buf, 256, "Unable to parseParamsFile %s (in UCS1::UCS1)\n", UCS_BETA_FILE.c_str());
    throw std::runtime_error( buf ); 
  }
}

UCS1::~UCS1() {

}

/*
  Parse params file.  Assumed format:
  - Space-separated file.
  - header line, with:  
    Stock N-PTS COMBINED-R KFRT-BETA IMB-BETA 
  - 1 line per stock, with:
    ticker n-pts combined-r kfrt-beta imb-beta
  Assumes that _kfrtBeta & _imbBeta are all pre-populated.

  Possible codeo mods:
  - Check that n-pts meets some minimum threshold.
*/
bool UCS1::parseParamsFileOld(string &ucsBetaFile) {
  // Read in mapping from ticker --> imb-beta & kfrt-beta.
  PerStockParams p;
  string fs(" ");
  int nr = PerStockParamsReader::readFile(_dm.get(), ucsBetaFile, fs, 1, p);  
  if (nr <= 0) {
    return false;
  }
  dynamic_bitset<> found;
  nr = p.populateValues(_dm.get(), 3, found, _kfrtBeta);
  nr = p.populateValues(_dm.get(), 4, found, _imbBeta);
  if (nr != (int)_dm->cidsize()) {
    std::cerr << "UCS1::parseParamsFile - warning parsing params file " << ucsBetaFile << std::endl;
    std::cerr << " stocks in population set " << _dm->cidsize() << " : stocks found in params file " << nr << std::endl;    
  }
  return true;
}

bool UCS1::parseParamsFile() {
  // Read in the ticker etf-name  etf-beta values
  file_table<UCS1FileTableElem> symToBetaTable( UCS_BETA_FILE );

  if ( symToBetaTable.empty() ){
    throw std::runtime_error( "No symbols read in by UCS1::parseParamsFile (input_file=" + 
			      UCS_BETA_FILE + ")");    
  }

  UCS1FileTableElem *data;
  int nf = 0;
  for( int cid=0; cid<_dm->cidsize(); cid++ ) {
    const char* symbol = _dm->symbol( cid );
    clite::util::file_table<UCS1FileTableElem>::iterator it = symToBetaTable.find(symbol);    //  clite::util::
    if( it != symToBetaTable.end() ) {
      data = new UCS1FileTableElem( it->second ); // "it" is a pair of (key,value)
      _kfrtBeta[cid] = data->getKFRTBeta();
      _imbBeta[cid] = data->getImbBeta();
      nf++;
      delete data;
    }
  }  

  // Dump some debugging out put to log file to potentially assist with production issues.
  TAEL_PRINTF(_ddebug.get(), TAEL_WARN, "UCS1::parseParamsFile - parsing params file %s", UCS_BETA_FILE.c_str());
  TAEL_PRINTF(_ddebug.get(), TAEL_WARN, "stocks in population set %i : stocks found in params file %i", _dm->cidsize(), nf);
  for ( int i=0;i<_dm->cidsize();i++) {
    TAEL_PRINTF(_ddebug.get(), TAEL_WARN, "%-5s UCS1::parseParamsFile  kB%.2f  iB%.2f",
		    _dm->symbol(i),_kfrtBeta[i], _imbBeta[i]);
  }
  return true;
}

bool UCS1::getAlpha(int cid, double &fv) {
  bool haveImbAlpha, haveKFRTAlpha;
  double imbAlpha = 0.0, kfrtAlpha = 0.0, retAlpha = 0.0;
  fv = 0.0;
  haveKFRTAlpha = _kfrtSignal->getAlpha(cid, kfrtAlpha);
  haveImbAlpha = _imbSignal->getAlpha(cid, imbAlpha);
  if (haveKFRTAlpha) {
    retAlpha += kfrtAlpha * _kfrtBeta[cid];
  }
  if (haveImbAlpha) {
    retAlpha += imbAlpha * _imbBeta[cid];
  }
  if (!haveKFRTAlpha && !haveImbAlpha) {
    return false;
  }
  // make sure retalpha looks like a number, not an error condition.
  if (isnan(retAlpha) || isinf(retAlpha)) {
    return false;
  }
  fv = retAlpha;
  return true;
}


/**************************************************************************
  ETFKFRTFileTableElem Code!!!!
**************************************************************************/
/*
  Parse params file.  Assumed format:
  - Space-separated file.
  - header line, with:  
    Stock N-PTS COMBINED-R KFRT-BETA IMB-BETA 
  - 1 line per stock, with:
    ticker n-pts combined-r kfrt-beta imb-beta
  Assumes that _kfrtBeta & _imbBeta are all pre-populated.

  Possible codeo mods:
  - Check that n-pts meets some minimum threshold.
*/
UCS1FileTableElem::UCS1FileTableElem( const vector<string>& fieldNames, 
				      const vector<string>& fields ) {
  // assert some conditions
  if( fields.size() < 5 )
    throw table_data_error( "at ETFKFRTFileTableElem::ETFKFRTFileTableElem: Too few fields in line" );    
  _symbol = fields[0];
  _kfrtBeta = atof( fields[3] .c_str()); 
  _imbBeta = atof( fields[4] .c_str());  
}
