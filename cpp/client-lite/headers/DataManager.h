#ifndef _DATAMANAGER_H_
#define _DATAMANAGER_H_

#include <vector>
#include <deque>
#include <ext/hash_map>
#include <algorithm>
#include <cstdio>

#include <Common/MktEnums.h>
#include "Client/lib2/CIndex.h"

#include <holiday2/Holiday.h>
#include <holiday2/HolidaySet.h>

#include <Client/lib3/ordermanagement/common/OrderManager.h>
#include <Client/lib3/bookmanagement/common/Book.h>
#include <Client/lib3/bookmanagement/common/MarketBook.h>
#include <Client/lib3/bookmanagement/common/OrderBook.h>
#include <Client/lib3/bookmanagement/common/BookLevel.h>
#include <Client/lib3/tickmanagement/common/TickHandler.h>

#include <tael/Log.h>
#include <tael/FdLogger.h>

#include <SubOrderBook.h>

#include "c_util/Time.h"
using trc::compat::util::TimeVal;

#include <cl-util/Configurable.h>
#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>
#include <cl-util/table.h>
#include <Markets.h>
#include <DataUpdates.h>

#include <clite/message.h>

#include <BookTools.h>

#include <boost/iterator/filter_iterator.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <functional>

#include <Client/lib2/LiveSource.h>
class TimedAggrLiveSource;
class AggrDataFileSource;
class LiveSource;
class EventSourceCB;
// GVNOTE: The class PortLiveSource has been changed to PortLiveSourceImpl, and
// it now takes a template parameter. However, PortLiveSource is defined as a type
// using typedef. We want to use that definition.
//class PortLiveSource;

namespace __gnu_cxx {
    template <typename T> struct hash<T *> {
        size_t operator() (T *__x) const {
            return reinterpret_cast<size_t>(__x);
        }
    };
}

#define MAX_ALLOWED_REJECTS 10

/** A wrapper for all lib2 HF infrastructure.
  *
  * This will contain and hide all HF infrastructure, and provide a more
  * consistent and simpler interface to it.  Most other things in this package
  * will be UpdateListeners to this class.
  *
  * It consolidates physical data links and order routes to logical ECNs, with
  * separate options to switch data sources and routes without changing the code
  * accessing those ECNs.  It also supplies a single interface for data updates.
  * 
  * Users will get incremental updates from the UpdateListener, and consistent
  * state snapshots from the DataManager itself (see get(Sub)Market).
  *
  * It supplies some error and debugging logging, but statistic and debugging
  * logging should be separate modules that listen to the DataManager whenever
  * possible.
  *
  * Assumptions:
  * - Population set / CIndex membership is specified at creation of
  *   DataManager object, and is constant across life of object.
  * 
  */
class DataManager : 
    public Configurable,
    public lib3::OMListener,
    public lib3::MarketBookListener,
    public EventListener,
    public CIListener,
    public clite::message::coordinator<WakeupHandler>
{

    public:
        enum TradeSystem {
            COLO, SIMTRADE, NONE
        };

        std::vector<bool> stops;
    private:
        
        TradeSystem tradesys; /// COLO / SIMTRADE / NONE
        // bool trading_halted;

        void construct ( );

        std::string ddbgfile_, odbgfile_;
        std::string reportfile_;
        std::string alphalogfile_;
        std::string outfile_;
        //int outfd_;
        FILE* outfp;
        int numRejects;

        // dbglvl should ideally be of type trc::tael::Severity::Level instead of int. However,
        // if we do that, then Boost libraries complain. Also, it is not clear how would we
        // specify a Security::Level in a configuration file. So using int.
        int dbglvl;
        int useq;

        void checkWakeup ( int cid = -1 );

        void checkTimes();

        static const int mbk_id;
            
        std::string trade_type;  // "colo", "sim", "none"
        std::string margin_server;
        std::string position_server;
        std::string sim_config;
        std::string colo_config;


        //std::string margin_acct;
        std::string ps_acct;

        struct sim_options {
            typedef size_t key_type;
            ECN::ECN ecn;
            int seq_us, live_us, reply_us, cancel_us;
            sim_options ( std::vector<std::string> const &hdr, std::vector<std::string> const &vals );
            key_type get_key() { return (size_t)(ecn); }
        };

        struct colo_options {
            typedef size_t key_type;
            ECN::ECN ecn;
            std::string account, host;
            int port;
            colo_options ( std::vector<std::string> const &hdr, std::vector<std::string> const &vals );
            key_type get_key() { return (size_t)(ecn); }
        };

        clite::util::factory<clite::util::debug_stream>::pointer sim_exec_logger, sim_action_logger;
        std::string sim_outdir;

        
        bool parse_combined_server(std::string const &in, std::string *user, std::string *host, int *port);

        bool getPos ( bool async );
        bool getPos ( int cid, bool async );
        bool getLoc ( bool async );
    protected:

        clite::util::factory<clite::util::debug_stream>::pointer dbg;
        boost::shared_ptr<tael::FdLogger> reportfld;
        tael::Logger reportlog;
        clite::util::file_table<colo_options> *colos;
        clite::util::file_table<sim_options> *sims;

        bool live;
        bool massive;
        bool MOCmode;
        std::vector<bool> data_on;
        bool useextrd;
        bool usepoplus;
        int recvbuf;
        bool listen_only;

        //bool trade_live;

        uint32_t seqnum_, seqoff_, seqinc_;
        int sdate_;
        int edate_;
//        int i3date_, i4date_, b3date_, b3fdate_, i4fdate_;
        int nysetrdate_, arcatrdate_;
        std::string datadir_;

        std::vector<std::string> reply_groups;
        std::vector<std::string> datastr_;

        std::vector<int> symbol_subsets;

        //magic numbers all the hell over the place
        const static uint32_t seqnum_ecn_mask = 0xf << 26;
        inline int32_t next_seqnum (ECN::ECN ecn) { 
            seqnum_ = std::max(((uint32_t)curtv().sec() % 86400) * 750 + seqoff_, seqnum_ + seqinc_);
            return ((uint32_t)(ecn) << 26) | seqnum_;
        }
        inline ECN::ECN seqnum_ecn ( int seq ) {
            return ECN::ECN((uint32_t)(seq & seqnum_ecn_mask) >> 26);
        }

        holiday2::HolidaySet holidays;
        std::string holiday_file;

        struct SymbolRecord {
            char symbol[9];
            int cid;
            int position;
            int orders;
            bool set_exposure;
            double exposure;
        };


        double maxmargin_;
        std::vector<SymbolRecord> limits_;
        bool setupSymbols ( );
        void setLimits ( const SymbolRecord &sr );

        // require setup; are pointers
        lib3::OrderManager *om;
        //lib3::PositionManager *pm;
        lib3::MarginServer *ms;
        lib3::PositionServer *ps;
        EventSourceCB *ecb;
        TimedAggrLiveSource *als;
        AggrDataFileSource *adfs;
        //LiveSource *ls;
        EventSource *es;
        
        // containers
        CIndex      ci_;
        std::vector<MarketMaker> ecnToMM; // just for trading?
        std::vector<std::string> symbols_;
        
        // various settings
        std::vector<std::string> symfile_;

        typedef std::priority_queue<TimeUpdate> timerq;
        typedef std::set<Timer> timerset;


        timerq nexttimes;
        timerset timers;
        TimeVal midnight_;
        TimeVal mktopen_, mktclose_;

        bool initialized_, running_, enable_trd_;
        TimeVal curtv_;
        int date_;

        //lib3 infrastructure
        
        //indexed by ECN::ECN
        std::vector<lib3::MarketBook *> mktbks;
        std::vector<lib3::SubOrderBook *> ordbks;
        std::vector<lib3::QuoteHandlerBase<lib3::BookOrder> *> qhs;
        // GVNOTE: Perhaps we need to define a tick handler base as well?
        // The simulation classes in lib3 need one now
        std::vector<std::string> colo_accts;

        lib3::AggrMarketBook *mbk;

        bool setup_callbacks();
//        bool setup_margin_server ( );
        bool setup_position_server ( );
        bool setup_sim_trade();
        bool setup_colo_trade();
        bool setup_live_data();
        bool setup_hist_data();
        bool setup_pls ( PortLiveSource *pls, const char *addr );

        template <typename QH> 
        void setup_book (ECN::ECN ecn) {
            QH *qh = new QH(ci_, ECN::desc(ecn), ecb, es->clock, true, dbg.get());
            lib3::MarketBook *bk = new lib3::MarketBook(ci_, ecn, es->clock, dbg.get());
            bk->add_listener(mbk);
            // GVNOTE: Not sure why do we auto correct locked / crossed market.
            bk->set_auto_correct(true);
            qh->add_listener(bk);
            qh->add_listener(mbk);
            mktbks[ecn] = bk;
            qhs[ecn] = qh;
        }

        template <typename TRANS, typename TRADE>
        void setup_colo_trader (colo_options const &colo) {
            using namespace lib3;
            TRANS *tx =
                new TRANS(colo.host.c_str(), colo.port, es->clock, dbg.get());
            if (!tx->connect()) {
            	TAEL_PRINTF(dbg.get(), TAEL_ERROR, "failed to connect to %s at %s:%d",
            	        ECN::desc(colo.ecn), colo.host.c_str(), colo.port);
            }
            SingleThreadedScheduler<SockMessageStruct> *comm =
                new SingleThreadedScheduler<SockMessageStruct>(tx, dbg.get());
            TraderBase *trader = new 
                TRADE(colo.account.c_str(), om, comm, es->clock, ci_, ecb, dbg.get());
            om->add_trader(trader);
            //pm->add_account(colo.account.c_str());
            colo_accts[colo.ecn] = colo.account;
            trader->qh->add_listener(ordbks[colo.ecn]);

            // GVNOTE: Don't understand the following comment ;(
            // hack to avoid our real account for cxl-on-disconnect
            if (listen_only) {
                trader->request_position(ci_["SPY"], "_DUMMY_", true);
            }
            std::cout << "Leaving setup_colo_trader " << ECN::desc(colo.ecn) << std::endl;
        }

        template <typename SIM, typename TRADE>
        void setup_sim_trader (sim_options const &sim) {
            using namespace lib3;
            using namespace clite::util;
            // GVNOTE: Adding a null TickHandlerBase object to make it compile. Revisit this!
            //lib3::TickHandlerBase *thb = static_cast<lib3::TickHandlerBase*>(NULL);
            std::cout << "In setup_sim_trader " << ECN::desc(sim.ecn) << std::endl;
            SIM* simmkt;

            if (MOCmode) {
            	simmkt = new SIM(new lib3::MarketBook(ci_, 0, es->clock, dbg.get()), NULL, ci_, ecb, es->clock, sim_action_logger.get(), sim_exec_logger.get(), dbg.get());
            } else {
            	simmkt = new SIM(qhs[sim.ecn], NULL, ci_, ecb, es->clock, sim_action_logger.get(), sim_exec_logger.get(), dbg.get());
				if (simmkt->fqh) {
					simmkt->fqh->add_listener(mktbks[sim.ecn]);
					simmkt->fqh->add_listener(mbk);
				} else {
					TAEL_PRINTF(dbg.get(), TAEL_ERROR, "setup_sim_trader: %s has no fqh!", ECN::desc(sim.ecn));
				}
            }
            /* // GVNOTE: Due to the latest hyp2 upgrade, we no longer need to add an event source.
             * // In the constructor for SimMarket, they get the event source using ecb->get_src()
            if (live) simmkt->add_source(als);
            else simmkt->add_source(adfs);
            */
            simmkt->tt_seq    = TimeVal(0, sim.seq_us);
            simmkt->tt_live   = TimeVal(0, sim.live_us);
            simmkt->tt_conf   = TimeVal(0, sim.live_us + sim.reply_us);
            simmkt->tt_reply  = TimeVal(0, sim.reply_us);
            simmkt->tt_nolive = TimeVal(0, sim.cancel_us);
            simmkt->use_fake_updates = true;

            std::string account = std::string("RTS") + std::string(ECN::desc(sim.ecn));
            if (sim.ecn == ECN::ISLD) {
            	account = std::string("RTSNASD");
            }
            TraderBase *trader = new TRADE(account.c_str(), om, simmkt, es->clock, ci_, ecb, dbg.get());
            std::cout << "In setup_sim_trader. Created trader object for " << ECN::desc(sim.ecn)
            		  << " using account " << account.c_str() << std::endl;
            om->add_trader(trader);
            //pm->add_account(account.c_str());
            colo_accts[sim.ecn] = account;
            trader->qh->add_listener(ordbks[sim.ecn]);
        }


        bool isShort(int cid, int sz);

        void buildImpTick(DataUpdate &d, int cid, double px, int sz, ECN::ECN ecn, TimeVal tv);

        
        /** An exception for unwinding the HF callbacks. */
        class Stop { 
            public:
                Mkt::RunStatus status;
                std::string what;
                Stop(Mkt::RunStatus s, std::string w = "") :status(s), what(w) { }
                Stop() { }
        };

        MarketHandler mh;
        TapeHandler  th;
        TimeHandler  tmh;
        OrderHandler oh;
        UserMessageHandler umh;

    public:
        /** Default constructor.  Sets up configuration options and logging. */
        DataManager ( );
        /** Constructor with configuration prefix.
          * 
          * Use this to give the configuration parameters a prefix. Otherwise
          * the same as the default constructor.
          */
        DataManager ( std::string &confname );

        /** Initialize all HF infra structure.
          *
          * This should be implemented by a child class to set up all HF data
          * sources and callbacks, using the configuration parameters in DM.
          * Implementors should call DataManager::preinit before doing any work,
          * and DataManager::postinit afterwards, and should return true only if
          * no errors are met.
          */
        virtual bool initialize ( );

        /** Begin the callback loop, and return status on exit.
          *
          * Call the HF callbacks, and begin running.  Will return a status code
          * when the callback loop is either stopped, runs out of data, or
          * encounters an error.
          */
        virtual Mkt::RunStatus run ( );

        /** Stop the HF callback loop in a safe, defined way.
          *
          * This results in DataManager::run returning with a status of STOPPED.
          * TODO: inform listeners that we're about to stop with an Admin
          * message, and wake them up.
          */
        virtual void stop ( std::string reason ) { 
            Stop s; 
            s.status = Mkt::STOPPED; 
            s.what = reason; 
            throw s;
        };
        
        /** Access the symbol <-> cid mapping.
          * 
          * Securities are referenced by a small non-negative integer called a
          * cid.  This mapping is accessed by this function.
          */
        const char *symbol ( int cid ) const {
            return ci_[cid];
        }
        int cid ( const char *sym ) const {
            return ci_[sym];
        }

        /** I am not happy with this.
          *
          * In general, all cids should be occupied between 0 and the return
          * value of this function.  Rather than implementing a proper
          * container, we'll just return the max cid + 1.
          */
        int cidsize ( ) const {
            return ci_.size();
        }

        /** The current time value. */
        inline TimeVal const &curtv ( ) {
            if (running_ && ecb->curtv() > curtv_) {
                curtv_ = ecb->curtv();
            }
            return curtv_;
        }

        
        /** Is it live mode? */
        inline bool isLive() const {
	    return live;
	    }

        inline const char* getAlphaLogFile() {
        	return (std::string(getenv("EXEC_LOG_DIR")) + std::string("/") + alphalogfile_).c_str();
        }
//        inline bool isTradingHalted() const { return trading_halted; }
//        inline void setTradingHalted(bool trade_status) const { trading_halted = trade_status; }

        /** What trade-system is it? (colo/sim/none) */
        inline TradeSystem getTradeSystem() const { return tradesys; }
  
        inline bool isMOCmode( ) const { return MOCmode; }
        /** Are we listening to a particular ECN? */
        inline bool isDataOn( ECN::ECN ecn ) const { return data_on[ecn]; }
        void addTimer ( Timer t );
        Timer const & nextTimer ( ) const { return nexttimes.top().timer(); }
        inline Timer marketOpen ( ) const { return Timer(mktopen_); }
        inline Timer marketClose ( ) const  { return Timer(mktclose_); }
        inline TimeVal timeValMktClose ( ) const  { return mktclose_; }
        typedef std::vector<lib3::MarketBook *> SubBookMap;
        typedef std::vector<lib3::SubOrderBook *> SubOrderBookMap;
        inline SubBookMap const     &subBooks ( ) { return mktbks; }
        inline lib3::MarketBook     *subBook ( ECN::ECN ecn ) { return mktbks[ecn]; }
        inline lib3::AggrMarketBook *masterBook ( ) { return mbk; }
        inline lib3::OrderBook      *orderBook ( ) { return listen_only? om->prom_book:om->book; }
        inline lib3::SubOrderBook   *subOrderBook ( ECN::ECN ecn ) { return ordbks[ecn]; }
        inline SubOrderBookMap const     &subOrderBooks ( ) { return ordbks; }
	
        int getSDate() { return sdate_;}
	
        enum BatsRouteMod {
            //BATS_ONLY            = 'B',
            BATS_INTERMARKET     = 'I',  // IOC + ignore NBBO + no routing
            //BATS_POSTONLY        = 'P',  // do not remove liquidity
            BATS_NASDAQ          = 'N',  // BATS + Nasdaq (INET) only
            BATS_NYSE            = 'Y',  // BATS + NYSE only
            BATS_ARCA            = 'A',  // BATS + Archipelago only
            BATS_NSXBLADE        = 'C',  // BATS + NSX Blade only
            BATS_NMS             = 'Z',  // NSX first, then sweep BATS book
            BATS_ALLMARKETS      = 'R'   // routable to all electronic markets
        } __attribute__ ((__packed__)) ; // be only one byte

        virtual Mkt::OrderResult placeOrder ( int cid, ECN::ECN ecn, int size, double price, 
                Mkt::Side dir, int timeout, bool invisible = false, int *seq = 0, long clientOrderID = -1, Mkt::Marking marking=Mkt::UNKWN, const char* placementAlgo=0 );

        virtual Mkt::OrderResult placeBatsOrder ( int cid, int size, double price, 
                Mkt::Side dir, BatsRouteMod routing, int *seq = 0, long clientOrderID = -1, Mkt::Marking marking=Mkt::UNKWN );

        /*
        virtual bool reduceSize ( const Order *o, int newSize );
        virtual bool reduceSize ( int id, int newSize );
        virtual int  replaceOrder ( const Order *o, int size, double price );
        virtual int  replaceOrder ( int id, int size, double price );
        */
        virtual int position ( int cid );

        virtual bool getPosition ( int cid ) { return getPos(cid, false); }
        virtual bool getPositionAsync ( int cid ) { return getPos(cid, true); }
        virtual bool getPositions ( ) { return getPos(false); }
        virtual bool getPositionsAsync ( ) { return getPos(true); }
        /** Get positions incrementally.
         *
         *  Equivalent to getPositionAsync for [pos, end_pos)
         *  Optional bool output to indicate end_pos >= cidsize()
         **/
        virtual bool getPositionsIncr ( int pos, int end_pos, bool* done = 0 );
        
        virtual int locates ( int cid );
        virtual bool getLocates ( ) { return getLoc(false); }
        virtual bool getLocatesAsync ( ) { return getLoc(true); }

        virtual Order const *getOrder ( int id );

        /* Set the PO+ flag. See config. */
        void setPOPlus(bool use_po_plus) {
          usepoplus = use_po_plus;
        }
		bool POPlusMode(){
		  return usepoplus;
		}
        /** Cancel an order.
          *
          * You can attempt to cancel any order that you have an update for, but
          * you may not succeed, if that order is not currently live.
          */
        virtual bool cancelOrder ( const Order *o );
        virtual bool cancelOrder ( int id );

        /** Cancel several orders, by cid, side, and price.
          *
          * Uses OrderManager's cancelBatch to cancel a set of orders based on
          * one or more of cid, side, ECN, and price.
          */
        virtual void cancelAll ( );

        void cancelMarket ( int cid );
        void cancelMarket ( int cid, Mkt::Side side );
        void cancelMarket ( int cid, Mkt::Side side, double px );
        void cancelMarket ( int cid, Mkt::Side side, int l );
        void cancelMarket ( int cid, ECN::ECN ecn );
        void cancelMarket ( int cid, ECN::ECN ecn, Mkt::Side side );
        void cancelMarket ( int cid, ECN::ECN ecn, Mkt::Side side, double px );
        void cancelMarket ( int cid, ECN::ECN ecn, Mkt::Side side, int l );

        virtual int unreadData ( );
        WakeUpdate wakeup_message ( );
        bool advise_wakeup ( );

        virtual ~DataManager ( );

        // MarketBookListener interface function
        void onBookChange ( int book_id, lib3::BookOrder *bo, lib3::QuoteReason qr, int delta, bool done );

        // OMListener interface functions. These are also the listener callback functions
        // that are invoked by OrderManager's functions, on receiving events)
        void onSequence     ( lib3::Order *lo ) { }
        void onConfirm      ( lib3::Order *lo );
        void onFill         ( lib3::Order *lo, lib3::FillDetails *fd );
        void onCancel       ( lib3::Order *lo, lib3::CancelDetails *cd );
        void onReject       ( lib3::Order *lo, lib3::RejectDetails *rd );
        void onCancelReject ( lib3::Order *lo, lib3::CancelRejectDetails *rd );
        void onBreak        ( lib3::BreakDetails *bd );
        void onPositionUpdate   ( const char *acct, int cid, int pos );
        void onGlobalMismatch   ( const char *acct, int cid, int newpos, int oldpos );
        void onPositionMismatch ( const char *acct, int cid, int newpos, int oldpos );
        void onPositionMismatch ( const char *acct, int cid, int new_total, int new_ts,
        		                  int new_yst, int new_adj, int old_total, int old_ts,
        		                  int old_yst, int old_adj );
        void onNoSequence   ( lib3::Order *lo ) { }
        void onNoConfirm    ( lib3::Order *lo );
        void onNoCancel     ( lib3::Order *lo );

        // EventListener interface function. Invokes the appropriate handler after creating
        // the appropriate update object. Takes care of tape updates (TapeHandler), data
        // updates (MarketHandler), and user message updates (UserMessageHandler).
        int OnEvent ( Event *e );

        // CIListener interface function
        void CIChange ( CIndex *which );
};

struct Canceler : public std::binary_function<lib3::Order *, size_t, size_t> {
    DataManager *dm;
    Canceler ( DataManager *dm ) : dm(dm) { }
    size_t operator() ( lib3::Order *o, size_t howmany ) {
        return dm->cancelOrder(o->seqnum)? howmany+1 : howmany;
    }
};

bool isInvisible(lib3::Order const *o);

#endif
