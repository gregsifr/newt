#ifndef __GUILLOTINE_HF__
#define __GUILLOTINE_HF__

#include <boost/shared_ptr.hpp>
#include <iostream>
#include <fstream>
#include <sys/stat.h>

#include <DataManager.h>
#include <Configurable.h>
#include <c_util/Thread.h>

#include <tael/Log.h>
#include <tael/FdLogger.h>

#include <guillotine/typed_message.h>
#include "mxdeque.h"

#include <ExecutionEngine.h>

#include <HFCodes.h>

namespace guillotine { namespace server {
    // The minimum ratio of securities for which we should have non zero locates
    const double MIN_BORROW_RATIO = 0.75;
    struct hfcontext;

    struct hflistener : 
        public clite::message::dispatch<OrderUpdate>::listener,
        public clite::message::dispatch<WakeUpdate>::listener,
        public clite::message::dispatch<TimeUpdate>::listener,
        public clite::message::dispatch<TradeRequest>::listener,
        public clite::message::dispatch<UserMessage>::listener
    {
        hfcontext *c;
        hflistener ( hfcontext *c ) : c(c) { }
        virtual ~hflistener () { }

        virtual void update ( const WakeUpdate &w );
        virtual void update ( const OrderUpdate &ou );
        virtual void update ( const TimeUpdate &tu );
        virtual void update ( const UserMessage &um );
        virtual void update ( const TradeRequest &trq );
        //virtual void update ( const DataUpdate &ou );
    };

    // external action types
    struct do_stop   { 
        int cid; double sort; int clientId;
        do_stop(int c, double so, int clientId):cid(c), sort(so), clientId(clientId) {}
    };
    struct do_trade  { 
        int cid; int qty; double aggr; double sort; int short_mark; long orderID; int clientId;
        do_trade(int c, int q, double a, double so, long oID, int sm, int clientId)
            : cid(c), qty(q), aggr(a), sort(so), orderID(oID), short_mark(sm), clientId(clientId) {}
    };

    typedef boost::variant<do_trade, do_stop> do_action;

    // priority relationship: stop > trade
    // within stop/trade, sort by aggressiveness, then by delta
    struct action_gt : public boost::static_visitor<bool> {
        bool operator() ( const do_stop &l, const do_stop &r ) const { return l.sort > r.sort; }
        bool operator() ( const do_trade &l, const do_trade &r ) const { return l.sort > r.sort; }
        bool operator() ( const do_stop &, const do_trade & ) const { return true; }
        bool operator() ( const do_trade &, const do_stop & ) const { return false; }
    };

    struct equal_cid : public boost::static_visitor<bool> {
        int cid;
        equal_cid ( int c ) : cid(c) { }
        template <typename T>
        bool operator() ( const T &t ) const { return t.cid == cid; }
    };

    struct apply_action : public boost::static_visitor<> {
        hfcontext *c;
        apply_action ( hfcontext *c ) : c(c) { }
        void operator() ( const do_stop &s );
        void operator() ( const do_trade &t );
    };


    struct msg_handler : public boost::static_visitor<> {

        hfcontext *c;
        msg_handler ( hfcontext *c_ ) : c(c_) { }

        int to_cid ( const std::string &s, int clientId );

        void setup_stop ( int cid, int clientId );

        void operator() ( const typed::connect &req );
        void operator() ( const typed::trade &req );
        void operator() ( const typed::stop &req );
        void operator() ( const typed::halt &req );
        void operator() ( const typed::resume &req );
        void operator() ( const typed::status &req );
    };
    
struct hfcontext : public Configurable {
    int stratcode, stopcode, stratoff, ecncode, ratecode, reloadcode, capcode,pcrcode;
    int act_rate; // in ms;

    bool request_syncs;

    int pnlcode, tccode; // user-message codes for printing pnl and printing tc summary info,
                         //   respectively.
    int inviscode,crosscode;
    int modelcode;
    int scalecode;
    int chunkcode;
    int popluscode;
    int ignoretickdowncode;
    int requestsyncscode;
    int orderprobcode;
    mxdeque<typed::request> &reqx;
    mxdeque<typed::response> &rspx;
    clite::util::factory<DataManager>::pointer dm;

    typedef mxdeque<typed::request>::deque reqdeque;
    typedef mxdeque<typed::response>::deque rspdeque;
    reqdeque my_reqs;
    rspdeque my_rsps;

    std::deque<do_action> actions;

    std::vector<bool> stops;
    std::vector<double> fillCash;
    std::vector<int> fillShs;
    std::vector<int> fillIx;

    std::list<std::string> symlist;

    msg_handler handler;
    hflistener listener;
    std::string coststr;
    std::string namestr;
    boost::shared_ptr<tael::FdLogger> costfld;
    tael::Logger costlog;
    std::string fillsFile;
    std::string reportFile;
    std::ofstream fillsOutStream;
    std::ofstream reportOutStream;
    tael::Logger log;

    boost::shared_ptr<tael::LoggerDestination> ld;

    TimeVal last_act;
    Timer sync_timer;
    Timer sync_minor_timer;

    int sync_cid_counter;
    int fillCounter;
    int _date;

    bool has_reset_;
    bool float_fills;
    bool start_halted;
    ExecutionEngine *trd;

    void send_info ( int cid, int clientId );
    void send_info ( int cid, int qty, double prio, int clientId );

    void send_symerror ( const std::string &sym, int clientId );
    void send_halterror ( const std::string &sym, int clientId );

    public:
    void reset ( ) { 
        stops.clear(); stops.resize(dm->cidsize(), false);
        fillCash.clear(); fillCash.resize(dm->cidsize(), 0);
        fillShs.clear(); fillShs.resize(dm->cidsize(), 0);
        fillIx.clear(); fillIx.resize(dm->cidsize(), 0);
    }

    hfcontext (
            mxdeque<typed::request> &reqx, mxdeque<typed::response> &rspx
            ) : Configurable("hf"),
        reqx(reqx), rspx(rspx), dm(clite::util::factory<DataManager>::get(only::one)),
        handler(this), listener(this),
        costlog(*(new tael::LoggerConfiguration((size_t) MAX_BINARY_BUFFER_FILE_SIZE))),
        log(*(new tael::LoggerConfiguration((size_t) MAX_BINARY_BUFFER_FILE_SIZE))),
        sync_timer(TimeVal(300,0), TimeVal(0,0)),
        sync_minor_timer(TimeVal(0,0), TimeVal(0,0)),
        has_reset_(false),
        fillCounter(0),
        sync_cid_counter(0)/*,
        ld(&tael::FdLogger::stdoutLogger()),
        costfld(&tael::FdLogger::stdoutLogger())*/
    {
        defOption("base-strat-code", &stratcode, "Strategy code base (for all gt servers)", defoption::BaseCode);
        defOption("offset-strat", &stratoff, "Offset code for this gt server");
        // GVNOTE: Changed default act-rate to 5 ms from 0 ms for stability reasons.
        defOption("action-rate", &act_rate, "time to wait in ms between sending actions (0 = instant)", 0);
        defSwitch("start-halted",&start_halted,"Start the server in a halted mode");
        defOption("request-syncs", &request_syncs, "Send locate/position sync requests", true);
        defOption("stop-code", &stopcode, "Stop code #", defoption::StopCode);
        defOption("ecn-code", &ecncode, "ECN Switch code #", defoption::ECNCode);
        defOption("rate-code", &ratecode, "Rate-limit reload code #", defoption::RateCode);
        defOption("reload-code", &reloadcode, "Position reload code #", defoption::ReloadCode);
        defOption("cap-code", &capcode, "Capacity  code #", defoption::CapCode);
        defOption("pcr-code", &pcrcode, "Capacity  code #", defoption::PcrCode);
       	defOption("pnl-code", &pnlcode, "PNL-print  code #", defoption::PnlCode);
        defOption("tc-code", &tccode, "TC-print  code #", defoption::TcCode);
        defOption("invis-code", &inviscode, "Invis wait code #", defoption::InvCode);
        defOption("cross-code", &crosscode, "Cross wait code #", defoption::CrossCode);
        defOption("model-code", &modelcode, "model flip code #", defoption::ModelCode);
        defOption("scale-code", &scalecode, "Priority Scaling code", defoption::ScaleCode);
        defOption("chunk-code", &chunkcode, "Order Chunking code", defoption::ChunkCode);
        defOption("poplus-code", &popluscode, "PO+ code #", defoption::POPlusCode);
        defOption("ignoretick-code", &ignoretickdowncode, "Ignore Tick code", defoption::IgnoreTickCode);
        defOption("request-syncs-code", &requestsyncscode, "Request locates/positions syncs code", defoption::SyncUpdateCode);
        defOption("order-place-prob",&orderprobcode,"Order placement probability code",defoption::OrderProbCode);
        defOption("cost-log-file", &coststr, "Cost log filename");
        defOption("float-fills", &float_fills, "Float target over external fills (generate implicit requests)");
        defOption("name", &namestr, "Name that server should use with client", "Guillotine Server");
        defOption("fills-log-file", &fillsFile, "Name of the fills file", "fills.txt");
        defOption("report-log-file", &reportFile, "Name of the fills file", "report.txt");
        //defOption("ignore",&ignore_symbols,"Symbols to ignore for trading");
    }

    bool initialize(ExecutionEngine *engine_, boost::shared_ptr<tael::LoggerDestination> ld_) {
        ld = ld_;
        trd = engine_;
        dm->add_listener(&listener);
        dm->addTimer(sync_timer);
        reset();
        if (configured("cost-log-file")) {
        	int fd = open((string(getenv("EXEC_LOG_DIR")) + string("/") + coststr).c_str(), O_RDWR | O_CREAT | O_APPEND, 0644);
            if (fd > -1) {
                costfld.reset(new tael::FdLogger(fd));
                costlog.addDestination(costfld.get());
            }
        } else return false;
        log.addDestination(ld.get());

        for (int i = 0; i < dm->cidsize(); ++i)
            symlist.push_back(std::string(dm->symbol(i)));

        if (start_halted){
            stops.clear(); stops.resize(dm->cidsize(), true);
        }

        DateTime dt(TimeVal::now);
        _date = dt.getintdate();

        struct stat buffer ;
        string fillsFilePath = string(getenv("EXEC_LOG_DIR")) + string("/") + fillsFile;
        bool fillsFileExists = (stat(fillsFilePath.c_str(), &buffer) == 0);
        fillsOutStream.open (fillsFilePath.c_str(), ios::out | ios::app | ios::binary);
        if(!fillsFileExists) fillsOutStream << "type|date|ticker|ts_received|shares|price|exchange|liquidity|orderID|algo" << std::endl;

/*
        bool reportFileExists = (stat(reportFile.c_str(), &buffer) == 0);
        reportOutStream.open (reportFile.c_str(), ios::out | ios::app | ios::binary);
        if(!reportFileExists) reportOutStream << "type|date|ticker|ts_received|shares|price|exchange|liquidity" << std::endl;
*/
        return true;
    }

    ~hfcontext () {
    	fillsOutStream.close();
    }

};


}}
#endif
