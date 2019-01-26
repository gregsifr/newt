#include <cstdio>
#include <cstring>
#include <sstream>
#include <iostream>
#include <typeinfo> // for using "bad_cast"
#include <boost/dynamic_bitset.hpp>
#include "Util/CFile.h"

#include <cl-util/debug_stream.h>
#include <cl-util/table.h>

#include <tael/Log.h>
#include <tael/FdLogger.h>

#include "DataManager.h"

#include "TradeLogic.h"
#include "AlphaSignal.h"
#include "PriorityComponent.h"
#include "StocksState.h"
#include "TradeConstraints.h"
#include "AlphaFromFile.h"
#include "CostLogWriter.h"
#include "TimeSlicesTrader.h"
#include <FollowLeaderSOB.h>
#include "ModelTracker.h"

using std::cout;
using std::cerr;
using std::endl;
using std::ifstream;
using std::bad_cast;
using boost::dynamic_bitset;

#define MAX_BINARY_BUFFER_FILE_SIZE (1 << 26)

const int    MARKET_OPEN_TIME_IN_SECONDS = (9*60 + 30) * 60;      // 9:30am
const tael::Severity MATT_DEFAULT_DEBUG_LEVEL = TAEL_INFO; // debug level for Matt's EOD file

const int   BUF_SIZE = 8096;
char ts_buffer[BUF_SIZE]; // used to get printing messages and print it to screen

const int    HOURS_TO_PRINT_IN_LOG_FILE[] = {40000,50000,60000,70000,80000,90000}; // just prints to the log file so that we can see 
const int    N_TIMES_TO_PRINT = sizeof(HOURS_TO_PRINT_IN_LOG_FILE)/sizeof(int);    // how the program advances
const int    LATE_AT_NIGHT = 200000; // 8pm


TimeSlicesTrader::TimeSlicesTrader( ExecutionEngine& ee, 
				    DataManager& dm, 
				    string tradeRequestsFilename, 
				    string summaryFilename, 
				    string logFilename, 
				    string mattFilename, 
				    char   delimiter, 
				    int printLevel,
				    double priority )
  :_ee(ee), 
   _dm(dm), 
   _stocksState( factory<StocksState>::get(only::one) ),
   _stocksInfo(_dm.cidsize()),
   _marketIsOpen( false ),
   _summaryFilename( summaryFilename ),
   _logPrinter( factory<debug_stream>::get(std::string("trader"))),
   _MattPrinter(*(new tael::LoggerConfiguration((size_t) MAX_BINARY_BUFFER_FILE_SIZE))),
   _priority(priority)
{
  int fd = open(mattFilename.c_str(), O_RDWR | O_CREAT | O_APPEND, 0640);
  if (fd == -1) {
	  TAEL_PRINTF(_logPrinter, TAEL_WARN, "TimeSlicesTrader::TimeSlicesTrader: failed to set matt-file %s.",
			mattFilename.c_str() );
  } else {
    tael::FdLogger fdl = new tael::FdLogger(fd);
    _MattPrinter->addDestination(fdl);
  }
  TAEL_PRINTF(&_MattPrinter, TAEL_NONE, "print level is set to %s",
		  tael::Severity::s_ascii[MATT_DEFAULT_DEBUG_LEVEL] );

  // Set the delimiter
  _delimiter[0] = delimiter;
  _delimiter[1] = 0;   // in order to be used as a string, the delimiter has to terminate with a 0
  TAEL_PRINTF(_logPrinter, TAEL_INFO, "_delimiter is %s", _delimiter ); // In my experience, this print is helpful

  // Initialize the Stock-specific--Information objects
  for( int cid=0; cid<_dm.cidsize(); cid++ )
    _stocksInfo[ cid ] = new StockInfo( _dm, cid, _MattPrinter );

  // get trade-requests (and initialize the vector '_tradeRequests')
  readTradeRequests( tradeRequestsFilename );

  // adding "PNLTracker" as a dm-listener.
  // This is risky, as we need to assume the DM was already constructed (I mean, this is true in the current design of this program,
  // but could be false in other designs)
}

TimeSlicesTrader::~TimeSlicesTrader()
{
  cerr << "In TimeSlicesTrader::~TimeSlicesTrader() DTOR\n";

  // delete dynamically allocated variables
  for( int cid=0; cid<_dm.cidsize(); cid++ ){
    cerr << " deleting _stocksInfo[" << cid << "]\n";
    delete _stocksInfo[ cid ];
    _stocksInfo[ cid ] = NULL;
  }
  _stocksInfo.clear();

  for( size_t i=0; i<_tradeRequests.size(); i++ ){
    cerr << " deleting tradeRequest i " << i << "\n";
    delete _tradeRequests[ i ];
    _tradeRequests[ i ] = NULL;
  }
  _tradeRequests.clear();
}

void TimeSlicesTrader::update( const TimeUpdate &t ) {
  if (t.timer() == _dm.marketOpen()) {
    TAEL_PRINTF(_logPrinter, TAEL_INFO, "Market is open at %s", getCurrTimeString(_dm) );
    _marketIsOpen = true;
    updateClosePrices();   // will not run in live mode, only in sim
  }
  else if (t.timer() == _dm.marketClose()) {
    _marketIsOpen = false;
    printSummary(); 
    printMattSummaryInfo();
    TAEL_PRINTF(_logPrinter, TAEL_NONE, "Market is closed. Stopping DM." );
    _ee.stop();
    _dm.stop( "Market is closed" );
  }
  // if market is still not marked as open but should be: update the corresponding variables
  else if( !_marketIsOpen && secondsSinceMidnight( _dm.curtv() ) > MARKET_OPEN_TIME_IN_SECONDS ) {
    TAEL_PRINTF(_logPrinter, TAEL_ERROR, "ERROR. Didn't get a MARKET_OPEN message. Assuming it is open by now." );
    _marketIsOpen = true;
    updateClosePrices();
  }
}

void TimeSlicesTrader::update( const OrderUpdate &ou ) {
  if (ou.action()!=Mkt::FILLED)
    return;
  // print to stdout
  ou.snprint( ts_buffer, BUF_SIZE );
  TAEL_PRINTF(_logPrinter, TAEL_INFO, "%-5s \t %s \t %s", _dm.symbol( ou.cid() ), getCurrTimeString(_dm), ts_buffer );
  // if this is a fill - send to the relevant trade-request-instances
  if( ou.action() == Mkt::FILLED){
    int numRelevantTradeRequests = 0;
    vector<ExternTradeRequest*>::iterator it;
    for( it = _tradeRequests.begin(); it != _tradeRequests.end(); ++it ) {
      if( int((*it)->getCid()) == ou.cid() && (*it)->isActive() ) {
	(*it) -> processAFill( ou );
	numRelevantTradeRequests++;
      }
    }
    // Report suspicious cases (where numRelevantTradeRequests != 1)
    if( numRelevantTradeRequests != 1 )
      TAEL_PRINTF(_logPrinter, TAEL_WARN, "%-5s \t %s \t WARNING: A fill-update (%s) matched %d trade-requests.",
			  _dm.symbol( ou.cid() ), getCurrTimeString(_dm), ts_buffer, numRelevantTradeRequests );
  }

}

void TimeSlicesTrader::update( const WakeUpdate &wu) {
  // if market is closed - do nothing
  if( !_marketIsOpen )
    return;
  
  // Go through each stock and update the market information
  for( int cid = 0; cid < _dm.cidsize(); cid++ )
    _stocksInfo[ cid ] -> updateMarketVars();

  // Call 'wakeup' in every Trade-Request
  vector<ExternTradeRequest*>::iterator it;
  for( it = _tradeRequests.begin(); it < _tradeRequests.end(); it++ )
    (*it) -> wakeup();
}

//////////////////////////////////////////////
// THE MAIN FUNCITON
//////////////////////////////////////////////


TimeVal parseTimeVal ( const string &s ) {
    int dot = s.find('.');
    if (dot == string::npos) {
        return TimeVal(atoi(s.c_str()), 0);
    } else {
        return TimeVal(atoi(s.substr(0, dot).c_str()), atoi(s.substr(dot+1, s.size()).c_str()));
    }
}


int main( int argc, char** argv ) {
  return TimeSlicesTrader::main( argc, argv );
}


int TimeSlicesTrader::main( int argc, char** argv ) {

//   if (argc < 2) {
//     cerr << "Must specify either --live or --hist as first option." << endl;
//     return 5;
//   }

  BufferedReader::set_max_total_cache(8 * 1024 * 3000); // To enable big simulations (based on line 187 of hyp2-base/Util/CFile.h

  DataManager *dm = new DataManager();
  if ( !factory<DataManager>::insert(only::one, dm) ){
    cerr << "DataManager already exists!\n";
  }

  CmdLineFileConfig cfg (argc, argv, "config,C");

  bool   help;
  //string tradeLogicName;
  string tradeRequestsFilename, exchangesFile;
  string summaryFilename, logFilename, mattFilename;
  char   delimiter;
  int    printLevel;
  vector<string> tradeon;
  //string alphaDir;
  bool   useAlpha;
  double priority = 1.0;
  bool overridecutoff;
  double cutoff;
  int timer_freq;
  
  vector<string> oneoffs, repeats;
  cfg.defOption("help,h", &help, "print this help.");
  //cfg.defOption("trade-logic", &tradeLogicName, "Trade Logic ('Drit'/'Quan')", "OPC");
  cfg.defOption("trades-file", &tradeRequestsFilename, "a file with one line per trade-request, in Mcquinn's format");
  //cfg.defOption("exchanges-file", &exchangesFile, "the exchanges file");
  cfg.defOption("summary-file", &summaryFilename, "The summary file");
  cfg.defOption("log-file", &logFilename, "A log file of the trading");
  cfg.defOption("matt-file", &mattFilename, "A log file for Matt's prints");
  cfg.defOption("delimiter", &delimiter, "the delimiter assumed in the trades-file and used in summary files", ',');
  cfg.defOption("prints-lvl", &printLevel, "verbosity of logfile ([min]0 to [max]6)", 6);
  cfg.defOption("trade-on", &tradeon, "ECNs to trade on");
  cfg.defOption("alpha-from-file", &useAlpha, "Use alphas read from file", false);
  cfg.defOption("override", &overridecutoff, "set to Override model cutoffs",false);
  cfg.defOption("cutoff", &cutoff, "Overridden cutoff for all cids",0.0);
  cfg.defOption("priority", &priority, "Priority to pass on",1.0);
  cfg.defOption("timer-freq", &timer_freq, "Basic Timer frequency",1000);
  
  cfg.defOption("oneoff", &oneoffs, "one-off timers");
  cfg.defOption("repeat", &repeats, "repeat timers");

  //cfg.defOption("alpha-dir", &alphaDir, "Directory holding historical alphas");

  cfg.add( *dm );
  cfg.add(*(file_table_config::get_config()));
  cfg.add(*(debug_stream_config::get_config()));
  //cfg.add(*(alpha_from_file_config::get_config()));

  cfg.configure();
  if( help ) { cerr << cfg << endl; return 1; }
  if( argc < 2 ) {
    cerr << "Two Few arguments!" << endl;
    cerr << cfg << endl;
    return 3;
  }

  if( !dm->initialize() ) return 2;
  
  TimeVal pd = TimeVal(0,timer_freq);
  TimeVal ph = TimeVal(0,0);
  dm->addTimer(Timer(pd,ph));
  
  for (vector<string>::iterator it = oneoffs.begin(); it != oneoffs.end(); ++it) {
    TimeVal tv = parseTimeVal(*it);
    dm->addTimer(Timer(tv));
    printf("Once %d.%06d\n", tv.sec(), tv.usec());
  }
  for (vector<string>::iterator it = repeats.begin(); it != repeats.end(); ++it) {
    int plus = it->find('+');
    TimeVal pd, ph;
    if (plus == string::npos) {
      pd = parseTimeVal(*it);
      ph = TimeVal(0,0);
    } else {
      pd = parseTimeVal(it->substr(0, plus));
      ph = parseTimeVal(it->substr(plus+1, it->size()));
    }
    dm->addTimer(Timer(pd, ph));
    printf("Every %d.%06d +%d.%06d\n", pd.sec(), pd.usec(), ph.sec(), ph.usec());
  }

  cerr << "Loaded " << dm->cidsize() << " stocks" << endl;
  factory<TradeConstraints>::pointer ecnsToTrade = factory<TradeConstraints>::get(only::one);
  
  for (vector<string>::const_iterator it = tradeon.begin(); it != tradeon.end(); ++it) {
    ECN::ECN ecn = ECN::ECN_parse(it->c_str());
    if (ecn == ECN::UNKN) {
      cerr << "Unknown trading ECN: " << *it << endl;
      return 1;
    }
    ecnsToTrade->set( ecn, true );
  }
  // set up alpha signal  
  AlphaSignal *as = NULL;
  if ( useAlpha ){
    as = new AlphaFromFile();
  } else {
    double constantAlpha = 0.0;
    as = new PlaceHolderAlphaSignal(dm->cidsize(), constantAlpha);
  }
  if ( !factory<AlphaSignal>::insert(only::one, as) ){
    cerr << "AlphaSignal already exists!\n";
  }
  // initialize Execution Engine
  // make one tradeLogicComponent for all stocks
  //TradeLogicComponent *ptlc = new PriorityComponent;
  TradeLogicComponent* ptlc = new FollowLeaderSOB();

  ExecutionEngine ee( ptlc);

  // initialize the TimeSlicesTrader
  TimeSlicesTrader trader( ee, *dm, 
			   tradeRequestsFilename, summaryFilename, logFilename, mattFilename,
			   delimiter, printLevel, priority );
  dm->add_listener( &trader ); // make sure TimeSlicesTrader gets all the updates
  CostLogWriter *clw = new CostLogWriter();

  if (overridecutoff){
    std::cerr << "Overriding cutoffs to " << cutoff << std::endl;
    factory<ModelTracker>::pointer _modelTracker =  factory<ModelTracker>::get(only::one); 
    _modelTracker->setallCutoffs(cutoff);
  }
    cerr << "Now running DataManager...\n";
  dm->run();
 
  if (0){
    delete ptlc;
    delete as;
    delete dm;
    ptlc = NULL;
    as = NULL;
    dm = NULL;
  }

  return 0;
} // end of TimeSlicesTrader::main

//////////////////////////////////////////////
//////////////////////////////////////////////
//////////////////////////////////////////////

// update the closing prices of all names (should be ran every day in simulation)
void TimeSlicesTrader::updateClosePrices() {
  for( int cid=0; cid<_dm.cidsize(); cid++ )
    _stocksInfo[cid] -> updateClosePx();
}

// read trade-request file 
// Format (',' stands for the given delimiter): <symbol>,<startTrade HH:MM:SS.mmm>,<delta-target>,<seconds-to-deadline>
void TimeSlicesTrader::readTradeRequests( const string& tradeRequestsFilename ) {
  
  ifstream  tradesFile;
  tradesFile.open( tradeRequestsFilename.c_str() );
  if( !tradesFile ) {
    TAEL_PRINTF(_logPrinter, TAEL_ERROR, "ERROR! Could not open %s for reading data.", tradeRequestsFilename.c_str() );
    return;
  }
  
  // Now read lines, one by one, and create an instance of TradeRequest for each line
  char *symbol, *startTradeTimeStr, *deltaTargetStr, *secsToDeadlineStr, *priorityStr;
  for ( int i = 0; i<_dm.cidsize();i++)
    cerr << "Symlist: " << i << " " << _dm.symbol(i) << "\n";
  while( tradesFile.getline(ts_buffer, BUF_SIZE) ) {
    if ( strlen(ts_buffer) == 0){
      continue;
    }
    symbol =            strtok( ts_buffer, _delimiter );
    startTradeTimeStr = strtok( NULL,   _delimiter ); // Does this deal well with EOL ????
    deltaTargetStr =    strtok( NULL,   _delimiter );
    secsToDeadlineStr = strtok( NULL,   _delimiter );
    priorityStr       = strtok( NULL,   _delimiter );
    int cid = _dm.cid( symbol );
    if( cid < 0 ) { // An invalid ticker (should be (-1) in this case, actually)
      continue;
      
    }
    TAEL_PRINTF(_logPrinter, TAEL_INFO, "trade request line: %-5s\t%12s\t%6s\t%5s",
			symbol, startTradeTimeStr, deltaTargetStr, secsToDeadlineStr );
    TimeVal startTradeTime = HHMMSS_mmmToTimeVal( startTradeTimeStr, _dm.getSDate());
    int secsToDeadline = atoi( secsToDeadlineStr );
    TimeVal deadline( startTradeTime.sec() + secsToDeadline, startTradeTime.usec() );
    int deltaTarget = atoi( deltaTargetStr );
    double priority = atof( priorityStr );
    
    ExternTradeRequest* atr = new ExternTradeRequest( _ee, *(_stocksInfo[cid]), _MattPrinter, 
						      startTradeTime, deadline, deltaTarget, priority );
    _tradeRequests.push_back( atr );
    TAEL_PRINTF(_logPrinter, TAEL_DATA, "%-5s created a new trade-request with startTradeTime=%s deadline=%s deltaTarget=%d",
			symbol, DateTime(startTradeTime).gettimestring(), DateTime(deadline).gettimestring(), deltaTarget );
    
  }
  
  if( tradesFile.is_open() )
    tradesFile.close();
  
  return;
}

// print the header (field names) of the portfolio-summary-file
void TimeSlicesTrader::printSummary() {

  // open the ouptut file
  ofstream summaryStream;
  summaryStream.open( _summaryFilename.c_str() );
  if( !summaryStream.is_open() ) {
    TAEL_PRINTF(_logPrinter, TAEL_ERROR, "ERROR! Could not open %s for writing trade-summary!", _summaryFilename.c_str() );
    return;
  }

  // print the header
  ExternTradeRequest::snprintSummaryHeader( ts_buffer, BUF_SIZE, _delimiter );
  summaryStream << ts_buffer << endl;

  // print a summary-line for each trade-request
  vector<ExternTradeRequest*>::iterator it;
  for( it = _tradeRequests.begin(); it < _tradeRequests.end(); it++ ) {
    (*it) -> snprintSummary( ts_buffer, BUF_SIZE, _delimiter );
    summaryStream << ts_buffer << endl;
  }

  // close the summary file
  if( summaryStream.is_open() )
    summaryStream.close();

}

void TimeSlicesTrader::printMattSummaryInfo() {

  tael::Severity lev = _MattPrinter.configuration().threshold();
  _MattPrinter.SetThreshold(TAEL_INFO);
  TAEL_PRINTF(&_MattPrinter, TAEL_INFO, "TimeSlicesTrader::printMattSummaryInfo called-------------------------");
  TAEL_PRINTF(&_MattPrinter, TAEL_INFO, "  ------------HF PNL (includes actual fill prices)-----------------------");
  printPNLTracker();
  _MattPrinter.SetThreshold(lev);
}


void TimeSlicesTrader::printPNLTracker() {

}

