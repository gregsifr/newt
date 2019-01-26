/*
  Main/driver program for TradeTickPrinter.

*/

#include "DataManager.h"

#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>
#include <cl-util/table.h>
using namespace clite::util;

#include "TTPMain.h"
#include "TradeTickPrinter.h"

#include <tael/Log.h>
using namespace trc;

#include <cstdio>
using namespace std;

TTPConfigHelper::TTPConfigHelper() {
  setDefaultValues();
  setConfigOptions();
}

TTPConfigHelper::~TTPConfigHelper() {

}

void TTPConfigHelper::setDefaultValues() {
  listenISLD = listenBATS = listenARCA = listenNYSE = true;
}

void TTPConfigHelper::setConfigOptions() {
  defOption("listenISLD", &listenISLD, "Listen to ISLD data (default true)", true);
  defOption("listenBATS", &listenBATS, "Listen to BATS data (default true)", true);
  defOption("listenARCA", &listenARCA, "Listen to ARCA data (default true)", true);
  defOption("listenNYSE", &listenNYSE, "Listen to NYSE data (default true)", true);
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
  TTPConfigHelper cfgHelper;
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

  /*
    Lib2 version of code had calls here for turnining on/off up data sources.  
    In Lib3/neworder version of code, enabling/disabling various data sources
      is done automagically, via config options.
  */

  if(!dm->initialize()) return 2;  
  
  // Make TradeTickPrinter.
  std::cout << "About to make TradeTickPrinter " << std::endl;
  TradeTickPrinter ttp;
  std::cout << "Returned from making TradeTickPrinter " << std::endl;

  // All taken care of in constructors....
  //dm->addListener(&opent);
  //dm->addListener(&volt);
  //dm->addListener(&tester);
  
  // Hand off main-thread control to DataManager event loop.
  // Should cause code to start receiving calls to PortGenDriver s
  //   update and wakeup functions.
  dm->run();
  
  //delete dm;
}
