#include <cl-util/table.h>
#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>

#include "DataManager.h"
#include "ModelTracker.h"

using namespace clite::util;

const string MODELS_FILE = "lrcfmodel";
const string RELEVANT_LOG_FILE = "trader";

ModelTracker::ModelTracker() {

  // first, get a pointer to DataManager and to debug-printer from the factory
  factory<DataManager>::pointer dm = factory<DataManager>::find(only::one);
  if( !dm ) 
    throw std::runtime_error( "Failed to get DataManager from factory (in ModelTracker::ModelTracker)" );
  factory<debug_stream>::pointer  logPrinter = factory<debug_stream>::get( RELEVANT_LOG_FILE );

  // second, get a map from symbol to exchanges based on the exchanges-file
  file_table<SymbolModelTracker> symToModTable( MODELS_FILE );
  if ( symToModTable.empty() ){
    throw std::runtime_error( "No symbols read in by ModelTracker::ModelTracker (input_file=" + 
			      MODELS_FILE + ")");   
  }
  // now go over the symbols in dm:
  _data.resize( dm->cidsize() );
  for( int cid=0; cid<dm->cidsize(); cid++ ) {
    const char* symbol = dm->symbol( cid );
    file_table<SymbolModelTracker>::iterator it = symToModTable.find(symbol);
    if( it != symToModTable.end() ) {
      // create a copy of the SymbolModelTracker object
      _data[ cid ] = new SymbolModelTracker( it->second ); // "it" is a pair of (key,value)
    } else {
      TAEL_PRINTF(logPrinter.get(), TAEL_ERROR, "Couldn't find model for %s",symbol);
      _data[ cid ] = new SymbolModelTracker(symbol);
    }
  }
  _enabled = true; // Start enabled
  for( int cid=0; cid<dm->cidsize(); cid++ ) 
    _data[cid]->print();
}

ModelTracker::~ModelTracker()
{
  for( unsigned int i=0; i<_data.size(); i++ )
    delete _data[ i ];
}
  
void ModelTracker::setallCutoffs(double cutoff){
   for( unsigned int i=0; i<_data.size(); i++ )
     _data[i]->cutoff = cutoff;
}

SymbolModelTracker::SymbolModelTracker(const char* symbol): valid(false),_symbol(symbol),k(-1),r(0),meany(-1e6),stdy(1e-6),c(0),q(0),p2(0),h1(0),h2(0),dpull(0),dpq(0),dspd(0),ltm(0),crimb(0),aspd(0),excessimb(0) 
{}


SymbolModelTracker::SymbolModelTracker(const string& symbol): valid(false),_symbol(symbol),k(-1),r(0),meany(-1e6),stdy(1e-6),c(0),q(0),p2(0),h1(0),h2(0),dpull(0),dpq(0),dspd(0),ltm(0),crimb(0),aspd(0),excessimb(0) 
{}


SymbolModelTracker::SymbolModelTracker( const vector<string>& fieldNames, const vector<string>& fields ) 
{
  // assert some conditions
  if( fields.size() < 21 )
    throw table_data_error( "at SymbolModelTracker::SymbolModelTracker: Too few fields in line" );
  
  _symbol = fields[0];
  
  k=atof(fields[2].c_str());
  r=sqrt(atof(fields[15].c_str()));
  meany=atof(fields[18].c_str());
  stdy=atof(fields[19].c_str());
  q = atof(fields[4].c_str());
  p2 = atof(fields[5].c_str());
  h1 = atof(fields[6].c_str());
  h2 = atof(fields[7].c_str());
  dpull = atof(fields[8].c_str());
  dpq = atof(fields[9].c_str());
  dspd = atof(fields[10].c_str());
  ltm = atof(fields[11].c_str());
  crimb = atof(fields[12].c_str());
  aspd = atof(fields[13].c_str());
  excessimb = atof(fields[14].c_str());
  cutoff = atof(fields[20].c_str());
  valid = true;
}



double SymbolModelTracker::linearBlend( double _q, double _p2, 
					double _h1, double _h2, double _dpq, 
					double _dpull, double _dspd, double _ltm, 
					double _crimb, double _excessimb, double _aspd){
  double ret = c;
  ret += q * _q;
  ret += p2 * _p2;
  ret += h1 * _h1;
  ret += h2 * _h2;
  ret += dpull * _dpull;
  ret += dpq * _dpq;
  ret += dspd * _dspd;
  ret += ltm * _ltm;
  ret += crimb * _crimb;
  ret += aspd * _aspd;
  ret += excessimb * _excessimb;
  
  // Compute z-score
  ret = (ret - meany)/(stdy*r);
  
  return ret;
  
}

void SymbolModelTracker::print(){
  factory<debug_stream>::pointer  logPrinter = factory<debug_stream>::get( RELEVANT_LOG_FILE );
  TAEL_PRINTF(logPrinter.get(), TAEL_ERROR, "Model for %s,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f,%f",_symbol.c_str(),k,r,meany,stdy,q,p2,h1,h2,dpull,dpq,dspd,ltm,crimb,aspd,excessimb,cutoff);
  
}
