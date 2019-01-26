/**

  ImbTracker.cc

*/
#include <cl-util/table.h>
#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>
using namespace std;


#include "DataManager.h"
#include "ImbTracker.h"
#include "HFUtils.h"

// So many new lines

#define DEFAULT_K_VAL -2.4

const string RELEVANT_LOG_FILE = "misc";
const string K_FILE = "klist";

#include <string>
#include <ext/hash_map>


/*
  Some extra compilation magic to get hash_map to work.
  Copied from:  http://gcc.gnu.org/ml/libstdc++/2002-04/msg00107.html
namespace __gnu_cxx
{
        template<> struct hash< std::string >
        {
                size_t operator()( const std::string& x ) const
                {
                        return hash< const char* >()( x.c_str() );
                }
        };
}
*/


ImbTracker::ImbTracker():
  _stocksState( factory<StocksState>::get(only::one) ),
  
  _minpts(60),
  _nperiods(120),
  _msec(1000),
  _buf(0),
  _lastUpdateTV(),
  _lastPrintTV(),
  _mktOpen(false),
  _Fv(0),
  imb_cache(0),
  _klist(0)

{

  _dm = factory<DataManager>::find(only::one);
  if( !_dm ) 
    throw std::runtime_error( "Failed to get DataManager from factory (in ImbTracker::ImbTracker)" );
  
  
  _Fv.resize(_dm->cidsize());
  imb_cache.resize(_dm->cidsize());
  _buf.resize(_dm->cidsize());

  _ddebug = factory<debug_stream>::get( RELEVANT_LOG_FILE );

  // Readin the klist values
  
  file_table<KTracker> symToExcTable( K_FILE );
  
  if ( symToExcTable.empty() ){
    throw std::runtime_error( "No symbols read in by ImbTracker::ImbTracker (input_file=" + 
			      K_FILE + ")");    
  }
  
  _klist.assign(_dm->cidsize(),DEFAULT_K_VAL);
  KTracker *_data;
  for( int cid=0; cid<_dm->cidsize(); cid++ ) {
    const char* symbol = _dm->symbol( cid );
    clite::util::file_table<KTracker>::iterator it = symToExcTable.find(std::string(symbol));    //  clite::util::
    if( it != symToExcTable.end() ) {
      _data = new KTracker( it->second ); // "it" is a pair of (key,value)
      _klist[cid] = _data->getk();
    }
  }
  
  TAEL_PRINTF(_ddebug.get(), TAEL_WARN, "ImbTracker::ImbTracker called: dm->cidsize = %i  minpts = %i  nperiods = %i  msec = %i",
		  _dm->cidsize(), _minpts, _nperiods, _msec); 
  for ( int i=0;i<_dm->cidsize();i++) {
    _buf[i] = new CircBuffer<double>(_nperiods);
    _Fv[i].resize(101);
    imb_cache[i]=0;
    TAEL_PRINTF(_ddebug.get(), TAEL_WARN, "%-5s ImbTracker::ImbTracker k=%f",_dm->symbol(i),_klist[i]);

    computeImbalanceProbabilities(_klist[i],_Fv[i]);
  }
  clearAllStocks();
  _dm->add_listener(this);
}

ImbTracker::~ImbTracker() {
  for ( int i=0;i<_dm->cidsize();i++) {
    delete _buf[i];
  }
}

int ImbTracker::nPts(int cid) {
  CircBuffer<double> *cb = _buf[cid];
  return cb->size();
}

bool ImbTracker::getAImb(int cid, double &fv) {
  if (nPts(cid) < _minpts) {
    fv = 0.0;
    return false;
  }
  return _buf[cid]->getAvg(&fv);
}

bool ImbTracker::getDImb(int cid, double &fv) {
  double aspd;
  if (!getAImb(cid, aspd)) {
    fv = 0.0;
    return false;
  }
  double cspd;
  if (!getImb(cid, cspd)) {
    fv = 0.0;
    return false;
  }
  fv = (cspd - aspd);
  return true;
}

bool ImbTracker::getCDImb(int cid, double &fv, double &efv) {
  double aspd;
  if (!getAImb(cid, aspd)) {
    fv = 0.0;
    TAEL_PRINTF(_ddebug.get(), TAEL_INFO, "%-5s Couldn't calc ave Imb for npts=%d",_dm->symbol(cid),nPts(cid));
    return false;
  }
  double cspd;
  if (!getImb(cid, cspd)) {
    fv = 0.0;
    return false;
  }
  efv = (cspd - aspd);
  fv = cspd;
  return true;
}




/*
  Add a point estimate of spread for specified stock.
*/
void ImbTracker::addSampledImb(int cid, double fv) {
  CircBuffer<double> *cb = _buf[cid];
  cb->add(fv);
}

void ImbTracker::sampleAllStocks() {
  int i, nstocks = _buf.size();
  double imb;
  for (i=0;i<nstocks;i++) {
    if (!getImb(i, imb)) {
      continue;
    }
    addSampledImb(i, imb);
  }
}


void ImbTracker::clearAllStocks() {
  int i, nstocks = _buf.size();
  for (i=0;i<nstocks;i++) {
    _buf[i]->clear();
  }
}



void ImbTracker::update(const TimeUpdate & t) {
  if (t.timer() == _dm->marketOpen()) {
    _mktOpen = true;
  }
  if (t.timer() == _dm->marketClose()) {
    _mktOpen = false;
    clearAllStocks();
  }
}


void ImbTracker::update(const WakeUpdate& wu){
  double msb = HFUtils::milliSecondsBetween(_lastUpdateTV,_dm->curtv());
  if ((msb >= _msec) && (_mktOpen == true)) {
    sampleAllStocks();
    _lastUpdateTV = _dm->curtv();
  }
    
}
void ImbTracker::computeImbalanceProbabilities( double k,
					       vector<double> &Fv) {
  

  int sz=Fv.size();
  double curSum=0.0;
  double pi;
  Fv[0]=0.0;
  for (int i=1;i<sz;i++) {
    pi = pow(i+1, k);   
    curSum+=pi;
    if (i>1)
      Fv[i]=Fv[i-1]+pi;
    else
      Fv[i]=pi;
  }
  // Normalize so that all probabilities, sum to 1.0
  for (int i=0;i<sz;i++) 
    Fv[i]/=curSum;
  //Fv = RVectorNumeric::divide(Fv, curSum);
}

bool ImbTracker::getImb(int cid, double &imb){
  double truemid;
  return getImb(cid, imb,truemid);
  
}

bool ImbTracker::getImb(int cid, double &imb,double &truemid){
  
  if (calcImb(cid,imb,truemid)){
    imb_cache[cid]=imb;
    return(true);
  }
  imb = imb_cache[cid];
  SingleStockState *ss = _stocksState->getState(cid);
  double apx, bpx,mpx;

  bpx = ss->bestPrice(Mkt::BID);
  apx = ss->bestPrice(Mkt::ASK);
  mpx = (bpx+apx)/2.0;
  truemid = (1+imb)*mpx;
  if (fabs(imb)<100)
    return(true);
  return(false);
}

bool ImbTracker::calcImb(int cid, double &imb, double &truemid) {
  int extraBidLevel = 0;
  int extraAskLevel = 0;
  size_t extraBidShs = 0;
  size_t extraAskShs = 0;
  return calcImb(cid, extraBidLevel, extraBidShs, extraAskLevel, extraAskShs, imb, truemid);
}

bool ImbTracker::calcImb(int cid, int extraBidLvl, size_t extraBidShs, int extraAskLvl, size_t extraAskShs, double &imb, double &truemid){
  
  // Compute the unconditional signal for the particular cid
  int sz=_Fv[cid].size()-1;
  double imb_ask=0;
  double imb_bid=0;
  int level=0;
  int vol=0;
  double lpx;
  size_t lsz;
  imb=0;
  truemid=-1;
  double bid,ask;
  
  SingleStockState *ss = _stocksState->getState(cid);
 
  if (!ss->haveNormalMarket()){    // ss->haveNormalMarket()??
    //TAEL_PRINTF(_ddebug, TAEL_INFO, "%-5s Couldn't calc Imbalance No Valid Market(%s)(%f,%f) ",_dm->symbol(cid),DateTime(_dm->curtv()).getfulltime(),bid,ask);
    return(false);
  }
  ask = ss->bestPrice(Mkt::ASK);
  bid = ss->bestPrice(Mkt::BID);
  if (cmp<6>::LE(ask,bid)){
    TAEL_PRINTF(_ddebug.get(), TAEL_INFO, "%-5s Couldn't calc Imbalance Locked Market (%f,%f)",_dm->symbol(cid),bid,ask);
    return(false);
  }
  

  double mid=(bid+ask)/2.0;
  if (mid<1e-6){
    return(false);
  }
  int CurCntr=0;
  int OldCntr=0;
  level=0;
  while (vol<sz*100){
    if (!getMarket(_dm->masterBook(),cid,Mkt::BID,level,&lpx,&lsz)){
      TAEL_PRINTF(_ddebug.get(), TAEL_WARN, "%-5s Couldn't calc  Imb Not enough info on BID",_dm->symbol(cid));
      return(false);
    }
    if (level == extraBidLvl) {     // For scenario analysis.
      lsz += extraBidShs;
    }
    if ( (lpx-bid)/mid < -0.0025)
      break;
    vol+=lsz;
    CurCntr=std::min((int)floor(vol/100),sz);
    if (CurCntr>0){
      imb_bid+=(_Fv[cid][CurCntr]-_Fv[cid][OldCntr])*lpx;
    }
    OldCntr=CurCntr;
    level++;
  }

  imb_bid/=_Fv[cid][CurCntr];

  vol=0;
  CurCntr=0;
  OldCntr=0;
  level=0;
  while (vol<sz*100){
    if (!getMarket(_dm->masterBook(),cid,Mkt::ASK,level,&lpx,&lsz)){
      TAEL_PRINTF(_ddebug.get(), TAEL_WARN, "%-5s Couldn't calc  Imb Not enough info on ASK",_dm->symbol(cid));

      return(false);
    }
    if (level == extraAskLvl) {
      lsz += extraAskShs;
    }
    if ( (lpx-ask)/mid > 0.0025){
      vol=sz*100;
      break;
    }
    vol+=lsz;
    CurCntr=std::min((int)floor(vol/100),sz);
    if (CurCntr>0){
      imb_ask+=(_Fv[cid][CurCntr]-_Fv[cid][OldCntr])*lpx;
    }
    OldCntr=CurCntr;
    level++;
  }

  imb_ask/=_Fv[cid][CurCntr];
  imb= (imb_ask+imb_bid)/(2*mid)-1;
  truemid = (imb_ask+imb_bid)/2.0;

  return(true);
  
}

bool ImbTracker::getAlpha(int cid, double &alphaEst) {
  return getImb(cid, alphaEst);
}



KTracker::KTracker( const vector<string>& fieldNames, const vector<string>& fields ) 
{
  // assert some conditions
  if( fields.size() < 2 )
    throw table_data_error( "at KTracker::KTracker: Too few fields in line" );
  
  _symbol = fields[0];
  _k = atof( fields[1] .c_str());
}

KTracker::KTracker( const string& symbol, double k )
  : _symbol( symbol ),
    _k ( k ) {}

KTracker::KTracker( const char*   symbol, double k )
  : _symbol( symbol ),
    _k ( k ) {}
  
