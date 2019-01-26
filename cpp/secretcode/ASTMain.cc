#include "DataManager.h"

#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>
#include <cl-util/table.h>
using namespace clite::util;

#include "HFUtils.h"

#include "ASTMain.h"
#include "AlphaSignalTester.h"
#include "ExplanatoryModel.h"
#include "BarraUtils.h"

// Include files for specfic signal being tested.
#include "KFRTSignal.h"
#include "OpenTracker.h"
#include "VolatilityTracker.h"
#include "IBVolatilityTracker.h"
#include "ImbTracker.h"
#include "MarketImpactResearch.h"
#include "IntervalVolumeTracker.h"

#include <tael/Log.h>
#include <tael/FdLogger.h>

using namespace trc;
 
ASTConfigHelper::ASTConfigHelper() {
  setDefaultValues();
  setConfigOptions();
}

ASTConfigHelper::~ASTConfigHelper() {

}

void ASTConfigHelper::setDefaultValues() {
  useAsynchLogger = true;
  taelLogFile = "/spare/local/ase/ntradesys/tsim/AST.log";
  listenISLD = listenBATS = listenARCA = listenNYSE = true;
  exchangeFile = "/apps/exec/param-files/exchanges.20090828";
  useVolume = false;
  printTicks = false;
  exitOnClose = false;
  signalKFRTBasic = false;
  signalKFRTETF = false;
  signalKFRTExpMBarra = false;
  signalPLSOB = false;
  signalSharesTraded = false;
  signalSharesTradedBuyerTL = false;
  signalSharesTradedSellerTL = false;
  signalVolatility = false;
  barraRiskFile = "/apps/compustat/barra/data/RSK/USE3S0909.RSK";
  checkBarraWeights = false;
  printBarraMatrices = true;  
  timerFreq = 1000;
  sampleFreq = 60000;
}

void ASTConfigHelper::setConfigOptions() {
  defOption("exchangeFile", &exchangeFile, "File holding ticker --> exchange mapping");
  defOption("listenISLD", &listenISLD, "Listen to ISLD data (default true)", true);
  defOption("listenBATS", &listenBATS, "Listen to BATS data (default true)", true);
  defOption("listenARCA", &listenARCA, "Listen to ARCA data (default true)", true);
  defOption("listenNYSE", &listenNYSE, "Listen to NYSE data (default true)", true);
  defOption("taelLogFile", &taelLogFile, "File to which TAEL logger should send output");
  defOption("useVolume", &useVolume, "Whether KFRT signal should attempt to use volume info in signal");
  defOption("printTicks", &printTicks, "Whether KFRT signal should print info about state update on every tick");
  defOption("exitOnClose", &exitOnClose, "Whether tester should exit when it sees market-close message");
  defOption("signalKFRTBasic", &signalKFRTBasic, "Include KFRT-Basic signal");
  defOption("signalKFRTETF", &signalKFRTETF, "Include KFRT-ETF signal");
  defOption("signalKFRTExpMBarra", &signalKFRTExpMBarra, "Include KFRT-EXPM-BARRA signal");
  defOption("signalPLSOB", &signalPLSOB, "Include PLSOB signal");
  defOption("signalSharesTraded", &signalSharesTraded, "Include SharesTraded signal");
  defOption("signalSharesTradedBuyerTL", &signalSharesTradedBuyerTL, "Include SharesTraded signal");
  defOption("signalSharesTradedSellerTL", &signalSharesTradedSellerTL, "Include SharesTraded signal");
  defOption("signalVolatility", &signalVolatility, "Include Volaltility estimate signal");
  defOption("etfBetaFile", &etfBetaFile, "File holding stock -> {ETF, Beta} mappings");
  defOption("plsobKFile", &plsobKFile, "File holding stock -> PLSOB model params mapping");
  defOption("barraRiskFile", &barraRiskFile, "Barra .RSK file containing stock factor betas");
  defOption("barraIndexWeightsFile", &barraIndexWeightsFile, "File containing weights for synthetic indeces used to construct risk factor analogues");
  defOption("checkBarraWeights", &checkBarraWeights, "Check whether the BARRA index weights sum to expected value (true)");
  defOption("printBarraMatrices", &printBarraMatrices, "Print barra factorWeights and factorBetas matrices (for debugging)");
  defOption("timer-freq", &timerFreq, "Global timer-frequency");
  defOption("sample-freq", &sampleFreq, "AlphaSignalObservation sampling frequency");
}


int main(int argc, char **argv) {

  bool help;
  if (argc < 2) { cerr << "Need at least one argument!" << endl; return 1; }
  bool live = strncmp(argv[1], "--live", 6) == 0;
  if (live == true) {
    argc = argc - 1;
    argv = argv + 1;
  }
  CmdLineFileConfig cfg (argc, argv, "config,C");
  cfg.defSwitch("help,h", &help, "print this help.");

  /*
    Get new Live/Hist DataManager from DataManager factory.
  */
  factory<DataManager>::pointer dm(factory<DataManager>::get(only::one));

  /*
    Parse command line options into LSTZTraderConfigHelper structure.
  */
  ASTConfigHelper cfgHelper;
  cfg.add(*dm);
  cfg.add(cfgHelper);

  cfg.add(*(file_table_config::get_config()));
  cfg.add(*(debug_stream_config::get_config()));

  if (!cfg.configure()) { cerr << cfg << endl; return 1; }

  if (help) { cerr << cfg << endl; return 1; }

  /*
    Set up debug print / output file, plus logging destination.
  */     
  tael::Logger ddebug(tael::Logger(*(new tael::LoggerConfiguration((size_t) MAX_BINARY_BUFFER_FILE_SIZE))));  // Debugging info, to stdout/stderr.
  tael::Logger dsignal(tael::Logger(*(new tael::LoggerConfiguration((size_t) MAX_BINARY_BUFFER_FILE_SIZE))));    // Signal testing, to aysnch log file.

  int fd = open(cfgHelper.taelLogFile.c_str(), O_RDWR | O_CREAT | O_APPEND, 0640);
  if (fd == -1) {
      cerr << "Unable to open server log " << cfgHelper.taelLogFile << ", dying." << endl;
      return 1;
  }
  else {
	  boost::shared_ptr<tael::LoggerDestination> fld = new tael::FdLogger(fd);
      HFUtils::setupLogger(&dsignal, fld.get());
  }

  /*
    Lib2 version of code had calls here for turnining on/off up data sources.  
    In Lib3/neworder version of code, enabling/disabling various data sources
      is done automagically, via config options.    
  */

  if(!dm->initialize()) {
    std::cerr << "Unable to initialize DataManager, aborting program" << std::endl;
    return 2;
  }

  /*
    Signal specifific stuff:
    VolTracker configuration is fairly important for BasicKFRTSignal to work as expected....
  */
  // Imbalance signal.  Note:  Should be explicitly placed in factory system.
  factory<ImbTracker>::pointer imbSignal = factory<ImbTracker>::get(only::one);

  // Basic KFRT signal.
  IBVolatilityTracker volt(1000, 3600, 300, imbSignal.get());
  SpdTracker spdt(50, 60, 1000);  
  factory<BasicKFRTSignal>::pointer kfrtSignal = factory<BasicKFRTSignal>::get(only::one);
  kfrtSignal->setPrintTicks(cfgHelper.printTicks);

  AlphaSignal *signal;

  // Experimental - for testing AverageTradingImpactModel
  AverageTradingImpactModel _atim;
  // Experimental - for testing VolumeImbalanceTradingImpactModel.
  VolumeImbalanceTradingImpactModel _tim(imbSignal.get());  
  // Experimental - for testing BookTradingImpactModel.
  BookTradingImpactModel _btim;  
  // Experimental - for testing ImbTradingImpactModel.
  ImbTradingImpactModel _itim(imbSignal.get());

  vector<AlphaSignal*> signals;
 
  
  if (cfgHelper.signalKFRTBasic == true) {
    //signal = new BasicKFRTSignal(&volt, &spdt, cfgHelper.useVolume, cfgHelper.printTicks);
    //signals.push_back(signal);
    signals.push_back(kfrtSignal.get());
  } 
  if (cfgHelper.signalKFRTETF == true) {
    factory<ETFKFRTSignal>::pointer etfKFRTSignal = factory<ETFKFRTSignal>::get(only::one);
    etfKFRTSignal->setPrintTicks(cfgHelper.printTicks);
    signals.push_back(etfKFRTSignal.get());
  } 
  if (cfgHelper.signalPLSOB == true) {
    //BookElemTrackerHW _bet(dm, ddebug, 60000);
    //PartialOrderBookTracker _pobt(dm, &_bet, dm->cidsize(), 10);
    //LiteOrderBookTracker _lobt(dm);
    //signal = new PLSOBSignal(dm, &_pobt, &_lobt, cfgHelper.plsobKFile);
    signals.push_back(imbSignal.get());
  } 
  if (cfgHelper.signalKFRTExpMBarra == true) {
    bmatrixd factorWeights;
    bmatrixd factorBetas;
    ExplanatoryReturnModel *erm;

    // KFRT with ExplanatoryModel, using BARRA linear factor model.
    // Should be able to use entire BARRRA factor set, or industry factors only,
    //   depending on the BARRA .RSK file specified.
    if (!BarraUtils::populateStockBetasFile(dm.get(), cfgHelper.barraRiskFile, factorBetas, cfgHelper.checkBarraWeights)) {
      cerr << "Unable to populate BARRA factorBetas matrix " << endl; 
      return 1;      
    }
    if (!BarraUtils::populateIndexWeightsFile(dm.get(), cfgHelper.barraIndexWeightsFile, factorWeights, cfgHelper.checkBarraWeights)) {
      cerr << "Unable to populate BARRA factorWeights matrix " << endl; 
      return 1;      
    }    
    if (cfgHelper.printBarraMatrices) {
      string fw("factorWeights");
      string fb("factorBetas");
      BarraUtils::printFactorMatrix(dm.get(), fb, factorBetas);
      BarraUtils::printFactorMatrix(dm.get(), fw, factorWeights);
    }
    erm = new ERMLinearMultiFactor(NULL, factorWeights, factorBetas);
    ExplanatoryModelKFRTSignal *expmS = new ExplanatoryModelKFRTSignal(erm);
    factory<ExplanatoryModelKFRTSignal>::insert( only::one, expmS );
    expmS->setPrintTicks(cfgHelper.printTicks);
    signals.push_back(expmS);
  }  

  if (cfgHelper.signalSharesTraded == true) {
    signal = new SharesTradedSignal(cfgHelper.sampleFreq);
    signals.push_back(signal);
  } 
  if (cfgHelper.signalSharesTradedBuyerTL == true) {
    signal = new SharesTradedBuyerTLSignal(cfgHelper.sampleFreq);
    signals.push_back(signal);    
  }
  if (cfgHelper.signalSharesTradedSellerTL == true) {
    signal = new SharesTradedSellerTLSignal(cfgHelper.sampleFreq);
    signals.push_back(signal);    
  }  
  if (cfgHelper.signalVolatility == true) {
    signal = new VolatilitySignal(&volt);
    signals.push_back(signal);    
  }    
 
  // Make tester - configured to sample every minute.
  // timer-update frequency should be set to <= this.
  AlphaSignalTester tester(signals, dsignal, 0, cfgHelper.sampleFreq, cfgHelper.exitOnClose);
  
  //
  // Generate a global timer to driver things that expect regular admin updates.
  //
  dm->addTimer(Timer(TimeVal(0,cfgHelper.timerFreq * 1000),TimeVal(0,0)));

  // All taken care of in constructors....
  //dm->addListener(&opent);
  //dm->addListener(&volt);
  //dm->addListener(&tester);
  
  // Hand off main-thread control to DataManager event loop.
  // Should cause code to start receiving calls to PortGenDriver s
  //   update and wakeup functions.
  dm->run();
  
  //delete dm;
  delete signal;
}
