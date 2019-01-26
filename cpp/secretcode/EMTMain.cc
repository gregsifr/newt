/*
  main/driver program for ExplanatoryReturnModelTester.

*/

#include "DataManager.h"

#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>
#include <cl-util/table.h>
using namespace clite::util;

#include "HFUtils.h"

#include "EMTMain.h"
#include "ExplanatoryModelTester.h"
#include "ExplanatoryModel.h"

#include <tael/Log.h>
#include <tael/FdLogger.h>
using namespace trc;

#include "AlphaSignal.h"

#include "PerStockParams.h"
#include "RUtils.h"

/**************************************************************************
  EMTConfigHelper code.
**************************************************************************/
EMTConfigHelper::EMTConfigHelper() {
  setDefaultValues();
  setConfigOptions();
}

EMTConfigHelper::~EMTConfigHelper() {

}

void EMTConfigHelper::setDefaultValues() {
  useAsynchLogger = true;
  taelLogFile = "/spare/local/ase/ntradesys/tsim/AST.log";
  fvSignalFlavor = "";
  ermFlavor = "";
  sampleMilliSeconds = 60000;
  barraRiskFile = "/apps/compustat/barra/data/RSK/USE3S0909.RSK";
  checkBarraWeights = false;
  printBarraMatrices = true;
}

void EMTConfigHelper::setConfigOptions() {
  defOption("taelLogFile", &taelLogFile, "File to which TAEL logger should send output");
  defOption("fvSignalFlavor", &fvSignalFlavor, "Type of sgnal to use to adjust stated-mid, when computing actual returns");
  defOption("ermFlavor", &ermFlavor, "Type of ExplatoryReturnModel to test");
  defOption("sampleMilliSeconds", &sampleMilliSeconds, "Saming frequency for returns (default 60,000)");
  defOption("ermEtfMidBetaFile", &ermEtfMidBetaFile, "ETF Beta file (for ERM-ETF ermFlavor)");
  defOption("mktCapFile", &mktCapFile, "File holding ticker --> mkt cap mapping");
  defOption("barraRiskFile", &barraRiskFile, "Barra .RSK file containing stock factor betas");
  defOption("barraIndexWeightsFile", &barraIndexWeightsFile, "File containing weights for synthetic indeces used to construct risk factor analogues");
  defOption("checkBarraWeights", &checkBarraWeights, "Check whether the BARRA index weights sum to expected value (true)");
  defOption("printBarraMatrices", &printBarraMatrices, "Print barra factorWeights and factorBetas matrices (for debugging)");
}


/*
  Helper function.
  Given a data file containing market cap info, populates vector with caps.
*/
bool populateMktCapFile(DataManager *dm, string &capFile, vector<double> &mktCaps) {
   // Read in mapping from ticker --> etf-name + etf-beta.
  PerStockParams p;
  string fs(" ");
  int nr = PerStockParamsReader::readFile(dm, capFile, fs, 1, p);   
  if (nr <= 0) {
    return false;
  }
  dynamic_bitset<> found;  
  nr = p.populateValues(dm, 1, found, mktCaps);
  if (nr != (int)dm->cidsize()) {
    cerr << "populateMarketCapFile - warning parsing params file " << capFile << std::endl;
    cerr << " stocks in population set " << dm->cidsize() << " : stocks found in params file " << nr << std::endl;    
  }  
  return true;
}

/*
  Helper function, for use with linear factor model ERM, using EW returns from all stocks 
    in population set as factor.
  - Sets factor weights to vector of [1/S, 1/S, ....].
  - Sets factorBetas to vector of [1, ..., 1]
*/
bool lfmEWPopulateFactorMatrices(DataManager *dm, bmatrixd &factorWeights, bmatrixd &factorBetas) {
  int S = dm->cidsize();
  factorWeights.resize(S, 1);
  factorBetas.resize(S, 1);
  double ew = 1.0/S;
  for (int i=0; i<S; i++) {
    factorWeights(i, 0) = ew;
    factorBetas(i, 0) = 1.0;
  }
  return true;
}

/*
  Helper function, for use with linear factor model ERM , using VW (value/cap weighted) returns
    from all stocks in population set as factor:
  - If total.cap = sum(Cap1, Capn).
    - Sets factor/weights to [Capi/total.cap].
  - Sets factorBetas to 1.0.
*/
bool lfmVWPopulateFactorMatrices(DataManager *dm, string &capFile, bmatrixd &factorWeights, bmatrixd &factorBetas) {
  vector<double> mktCaps;
  int S = dm->cidsize();
  factorWeights.resize(S, 1);
  factorBetas.resize(S, 1);
  
  mktCaps.resize(dm->cidsize());
  mktCaps.assign(dm->cidsize(), 0.0);
  if (!populateMktCapFile(dm, capFile, mktCaps)) {
    return false;
  }
  double totalCap = RVectorNumeric::sum(mktCaps);

  for (int i=0; i<S; i++) {
    factorWeights(i, 0) = mktCaps[i]/totalCap;
    factorBetas(i, 0) = 1.0;
  }
  return true;
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
  EMTConfigHelper cfgHelper;
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
    EMTMain specific stuff:
  */
  // Create ERM of specified type.
  ExplanatoryReturnModel *erm = NULL;
  AlphaSignal *idxCFVSignal = NULL;
  bmatrixd factorWeights;
  bmatrixd factorBetas;

  //  Linear factor model using only market factor, as proxied by SPY.
  if (cfgHelper.ermFlavor.compare("ETF-MID") == 0) {
    //  Assigns each stock to 0...1 ETFs, and uses ETF betas.
    erm = new ERMETFMid(cfgHelper.ermEtfMidBetaFile);
  } else if (cfgHelper.ermFlavor.compare("LFM-EW") == 0) {
    if (!lfmEWPopulateFactorMatrices(dm.get(), factorWeights, factorBetas)) {
      cerr << "Unable to populate factorWeights and factorBetas matrices " << endl; 
      return 1;
    }
    erm = new ERMLinearMultiFactor(idxCFVSignal, factorWeights, factorBetas);
  } else if (cfgHelper.ermFlavor.compare("LFM-VW") == 0) {
    if (!lfmVWPopulateFactorMatrices(dm.get(), cfgHelper.mktCapFile, factorWeights, factorBetas)) {
      cerr << "Unable to populate factorWeights and factorBetas matrices " << endl; 
      return 1;
    }
    erm = new ERMLinearMultiFactor(idxCFVSignal, factorWeights, factorBetas);
  } else if (cfgHelper.ermFlavor.compare("LFM-BARRA") == 0) {
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
    erm = new ERMLinearMultiFactor(idxCFVSignal, factorWeights, factorBetas);
  } else {
    cerr << "Unknown ermFlavor " << cfgHelper.ermFlavor << endl; return 1;
  }

  // Create fvSignal of specified type.
  AlphaSignal *fvSignal = NULL;
  if (cfgHelper.fvSignalFlavor.compare("") != 0) {
    cerr << "Non-null fvSignal flavors not yet supported " << endl; return 1;
  }

  // Make tester.
  ExplanatoryReturnModelTester emt(erm, fvSignal, dsignal, 0, cfgHelper.sampleMilliSeconds);
  
  // All taken care of in constructors....
  //dm->addListener(&opent);
  //dm->addListener(&volt);
  //dm->addListener(&tester);
  
  // Hand off main-thread control to DataManager event loop.
  // Should cause code to start receiving calls to PortGenDriver s
  //   update and wakeup functions.
  dm->run();
  
  //delete dm;
  if (fvSignal != NULL) delete fvSignal;
  if (erm != NULL) delete erm;
}

