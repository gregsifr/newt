#include "GuillotineHF.h"
#include "GuillotineTCP.h"
#include "mxdeque.h"
#include "WakeupTimer.h"

//#include <guillotine/typed_message.h>

#include <DataManager.h>
#include <Configurable.h>

#include <ExecutionEngine.h>
#include <TradeLogicComponent.h>
#include <PriorityComponent.h>
#include <PriorityCutoffComponent.h>
#include <MOCComponent.h>

#include <TradeConstraints.h>
//#include <WakeupStats.h>
#include <RiskLimits.h>

#include <ForwardTracker.h>
#include <PullTracker.h>
#include <PNLTracker.h>
#include <TCTracking.h>
#include <UCS1.h>
#include <DumpAlpha.h>
#include <Chunk.h>
#include <iostream>
#include <cstdlib>

#include <tael/Log.h>
#include <tael/FdLogger.h>
#include <signal.h>

#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>
#include <cl-util/table.h>

using namespace std; 
using namespace guillotine;
using namespace trc;
using namespace clite::util;

/*********************************************************
  Guillotine main code:
*********************************************************/
int main ( int argc, char **argv ) {

  try {
	  if (getenv("EXEC_ROOT_DIR") == NULL)
		  throw "No EXEC_ROOT_DIR defined";
	  else if (getenv("EXEC_LOG_DIR") == NULL)
		  throw "No EXEC_LOG_DIR defined";

    mxdeque<typed::request> reqx;
    mxdeque<typed::response> rspx;

    factory<DataManager>::pointer dm(factory<DataManager>::get(only::one));

    void **vp = 0;
    bool help;

    server::ServerThread srv(reqx, rspx);

    struct sigaction act;
    act.sa_handler = SIG_IGN;
    act.sa_flags = 0;

    sigemptyset(&(act.sa_mask));
    sigaction(SIGPIPE, &act, 0);

    WakeupTimer wake;

    server::hfcontext hf(reqx, rspx);

    CmdLineFileConfig cfg(argc, argv, "config,C");

    vector<string> tradeon;
    string slogfile;
    int order_rate,order_size;
    double order_price;
    
    cfg.defOption("help,h", &help, "print this help message");
    cfg.defOption("server.server-log-file", &slogfile, "server access / error log file");
    cfg.defOption("trade-on", &tradeon, "ECNs to trade on");
    cfg.defOption("maxorderrate", &order_rate, "Maximum per symbol order rate", 75);
    cfg.defOption("maxordersize", &order_size, "Maximum per symbol order size", 10000);
    cfg.defOption("maxordeprice", &order_price, "Maximum per symbol order price", 200000.0);
    
    cfg.add(*(file_table_config::get_config()));
    cfg.add(*(debug_stream_config::get_config()));

    cfg.add(*dm);
    cfg.add(srv);
    cfg.add(hf);
    cfg.add(wake);

    if (!cfg.configure()) { cerr << cfg << endl; return 1; }
    if (help) { cerr << cfg << endl; return 1; }

    // For initial small scale testing & to be able to see system is doing,
    // May want to make this configurable option also.
    
    wake.init();
//    WakeupStats wakeupStats; // prints some wake-up statistics

    if (!dm->initialize()) { return 1; }

    factory<TradeConstraints>::pointer tradeConstraints = factory<TradeConstraints>::get(only::one);
    tradeConstraints->setOrderRate( order_rate );
    for (vector<string>::const_iterator it = tradeon.begin(); it != tradeon.end(); ++it) {
        ECN::ECN ecn = ECN::ECN_parse(it->c_str());
        if (ecn == ECN::UNKN) {
            cerr << "Unknown trading ECN: " << *it << endl;
            return 1;
        }
        tradeConstraints ->set( ecn, true );
    }
    factory<RiskLimit>::pointer _riskLimits =  factory<RiskLimit>::get(only::one);
    _riskLimits->setMaxShs(order_size);
    _riskLimits->setMaxNotional(order_price);
    // Create unconditional signal instance - initially used for crossing 
    //   & probing for hidden liquidity.
    // Created through factory system.  Also has side effect of causing
    //   factory system to know about UCS1 so that instance can be found
    //   elsewhere.
    factory<UCS1>::pointer ucsSignal(factory<UCS1>::get(only::one));

    // ALERT ALERT - may want to remove for production use.
    // Create a PNL tracker, which can be used to dump info on
    //   what our system thinks intra-day PNL should be, e.g. for 
    //   reconciliation.
    //factory<PNLTracker>::pointer pnlTracker(factory<PNLTracker>::get(only::one));

    // ALERT ALERT - may want to remove for production use.
    // Create a TC tracker, which can be used to dump detailed info on
    //   trade requests, order-placements, and fills, in a format that organizes
    //   inof so that it makes it easy to spot screwy behavior.
    factory<TROPFTrackerImmediate>::pointer tcTracker(factory<TROPFTrackerImmediate>::get(only::one));
    factory<ForwardTracker>::pointer fwdTracker(factory<ForwardTracker>::get(only::one));
    factory<PullTracker>::pointer pullTracker(factory<PullTracker>::get(only::one));
    factory<Chunk>::pointer chunk(factory<Chunk>::get(only::one));
    // for now, pick the same TradeLogicComponent for all stocks
    //TradeLogicComponent* component = new PriorityComponent(ucsSignal.get());
    TradeLogicComponent* component;
    if (dm->isMOCmode()) {
    	component = new MOCComponent();
    } else {
    	component = new PriorityCutoffComponent(ucsSignal.get());
    }

    // Create underlying ExecutionEngine object. 
    ExecutionEngine engine( component );

    int fd = open((string(getenv("EXEC_LOG_DIR")) + string("/") + slogfile).c_str(), O_RDWR | O_CREAT | O_APPEND, 0640);
    boost::shared_ptr<tael::LoggerDestination> fld(new tael::FdLogger(fd));
    if (fd == -1) {
        cerr << "Unable to open server log " << slogfile << ", dying." << endl;
        return 1;
    }
    hf.initialize(&engine, fld);
    srv.setLoggerDestination(fld);
    srv.start();

    if (!dm->getPositions() || !dm->getLocates()) {
        cerr << "Couldn't get locates or positions!" << endl;
    }

    if (!dm->isMOCmode()) DumpAlpha* da = new DumpAlpha();
    //dm->setPositionsUpdatedOnStartup(true);

    dm->run();
    srv.stop();

    delete component;

    srv.join(vp);

  } catch (std::runtime_error &e) {
      cerr << "Error: " << e.what() << endl;
  }
};

