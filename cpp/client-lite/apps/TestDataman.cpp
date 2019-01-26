#include <DataManager.h>
#include <VolumeTracker.h>

#include <BookTools.h>

#include <cl-util/Configurable.h>
#include <cl-util/factory.h>


#include <iostream>
#include <vector>
#include <string>
using namespace std;
using namespace clite::util;

class SimpleTrader : 
    public WakeupHandler::listener,
    public MarketHandler::listener,
    public TapeHandler::listener,
    public OrderHandler::listener,
    public TimeHandler::listener
{
    public:

        factory<DataManager>::pointer dmp;
        DataManager &dm;

        VolumeTracker vol;
        bool go;
        bool trade;
        bool wake;
        bool showdata;
        int min;
        int levels;
        vector<bool> changed;
        SimpleTrader (double trd = 0.40, double cxl = 0.10) :
            dmp(factory<DataManager>::find(only::one)), dm(*dmp),
            go(false), trade(true), wake(false), changed(dm.cidsize(),false), 
            trdThr(trd), cxlThr(cxl) { changed.resize(dm.cidsize()); }
        mutable char buf[120];

        /// Actions to take on waking up.

        void update ( const TimeUpdate &tu ) {
            tu.snprint(buf, 120);
            printf("%s", buf);
        }
        void update ( const WakeUpdate &w ) {

            if (go && wake) {
                char *tm = TVtoTimeStr(dm.curtv());
                printf("=== Wake up at %10ld.%6ld - %s ===\n", dm.curtv().sec(), dm.curtv().usec(), tm);

                for (unsigned int cid = 0; cid < dm.cidsize(); cid++) {
                    if (changed[cid]) {
                        printf("  %s Volume: %9d | VWAP: %7.2f | last trade: %7.2f \n", 
                                dm.symbol(cid), vol.volume(cid), vol.vwap(cid), vol.lastTrade(cid));
                        printBook(cid, levels);

                        for (int i = 0; i < ECN::ECN_size; ++i) {
                            if (dm.subBook(ECN::ECN(i)) != 0) {
                                printSubBook(cid, levels, ECN::ECN(i));
                            }
                        }
                        if (trade) {
                            attemptTrade(cid);
                            printTrades(cid, 10);
                        }
                        changed[cid] = false;
                    }
                }
                printf("=== Wake up complete ===\n");
                wake = false;
            }
        }

        // above trdThr and we place orders; below cxlThr and we cancel.
        double trdThr;
        double cxlThr;

        void update ( const DataUpdate &du ) {
            if (showdata) { 
                TimeVal now = dm.curtv();
                du.snprint(buf, 120);
                char *tm = TVtoTimeStr(now);
		double bpx,apx;
		size_t bsz,asz;
		bool bid,ask;
		bid = getMarket(dm.masterBook(), du.cid, Mkt::BID, 0, &bpx, &bsz);
                ask = getMarket(dm.masterBook(), du.cid, Mkt::ASK, 0, &apx, &asz);
		printf("Live %10ld.%06ld (%s): %s\n", now.sec(), now.usec(), tm, buf);
		if ( bid && ask && (bpx>apx)){
		printBook(du.cid, levels);
		
		for (int i = 0; i < ECN::ECN_size; ++i) {
		  if (dm.subBook(ECN::ECN(i)) != 0) {
		    printSubBook(du.cid, levels, ECN::ECN(i));
		  }
		}
		printf("=== End of books ===\n");
		}
                //printSubBook(du.cid, levels, du.ecn);
            }
                
            changed[du.cid] = true;
        }

        void update ( const TapeUpdate &tu ) {
            if (showdata) {
                char *tm = TVtoTimeStr(dm.curtv());
                tu.snprint(buf, 120);
                printf("%s %s\n", tm, buf);
            }
        }

        void update ( const OrderUpdate &ou ) {
            ou.snprint(buf, 120);
            std::cout << "orderFilter: " << buf << std::endl;
            printf(" -- %s book for %s -- \n", ECN::desc(ou.ecn()), dm.symbol(ou.cid()));
            printAnyBook(dm.subOrderBook(ou.ecn()), ou.cid(), 10);
            printTrades(ou.cid(), 10);
        }

        void printTrades(int cid, int n) {
            size_t asz, bsz, myasz, mybsz, myact, mybct;
            double apx, bpx;
            bool bid, ask;
            printf("----- My Order Book for %s -----\n", 
                    dm.symbol(cid));
            int i = 0;
            lib3::OrderBook *book = dm.orderBook();
            do {
                bid = getMarket(dm.masterBook(), cid, Mkt::BID, i, &bpx, &bsz);
                ask = getMarket(dm.masterBook(), cid, Mkt::ASK, i, &apx, &asz);
                
                lib3::OrderBookLevel *bob = book->get_level(cid, Mkt::BID, bpx);
                lib3::OrderBookLevel *aob = book->get_level(cid, Mkt::ASK, apx);
                myasz = aob? aob->size : 0;
                mybsz = bob? bob->size : 0;
                myact = aob? aob->num_orders : 0;
                mybct = bob? bob->num_orders : 0;
                if (bid && ask) printf("%2lux %4lu @ %7.2f |%1d| %7.2f @ %4lu x%2lu \n", 
                        mybct, mybsz, bpx, i, 
                        apx, myasz, myact);
                else if (bid)   printf("%2lux %4lu @ %7.2f |%1d| ------- @ -------- \n", 
                        mybct, mybsz, bpx, i); 
                else if (ask)   printf("-------- @ ------- |%1d| %7.2f @ %4lu x%2lu \n", 
                        i, apx, myasz, myact);
                ++i;
            } while ((bid || ask) && i < n);
        }
        template <typename BOOK>
        void printAnyBook(BOOK *book, int cid, int n) {
            if (!n) return;
            size_t asz, bsz;
            double apx, bpx;
            bool bid, ask;
            int i = 0;
            do {
                bid = getMarket(book, cid, Mkt::BID, i, &bpx, &bsz);
                ask = getMarket(book, cid, Mkt::ASK, i, &apx, &asz);
                
                if (bid && ask) printf("%7lu @ %7.2f |%1d| %7.2f @ %7lu \n", bsz, bpx, i, apx, asz);
                else if (bid)   printf("%7lu @ %7.2f |%1d| ------- @ ----- \n", bsz, bpx, i); 
                else if (ask)   printf("----- @ ------- |%1d| %7.2f @ %7lu \n", i, apx, asz);
                ++i;
            } while ((bid || ask) && i < n);
            printf("-----\n");
        }

        void printBook(int cid, int n) {
            if (!n) return;
            size_t asz, bsz;
            double apx, bpx;
            bool bid, ask;
            printf("----- Book for %s -----\n", 
                    dm.symbol(cid));
            int i = 0;
            do {
                bid = getMarket(dm.masterBook(), cid, Mkt::BID, i, &bpx, &bsz);
                ask = getMarket(dm.masterBook(), cid, Mkt::ASK, i, &apx, &asz);
                
                if (bid && ask) printf("%7lu @ %7.2f |%1d| %7.2f @ %7lu \n", bsz, bpx, i, apx, asz);
                else if (bid)   printf("%7lu @ %7.2f |%1d| ------- @ ----- \n", bsz, bpx, i); 
                else if (ask)   printf("----- @ ------- |%1d| %7.2f @ %7lu \n", i, apx, asz);
                ++i;
            } while ((bid || ask) && i < n);
            /*
            for (int s = 0; s< 2; ++s) {
                pair<DataManager::book_iterator, DataManager::book_iterator> ords 
                    = dm.getMarketList(cid, Mkt::Side(s), 0);
                char buf[100];
                for (DataManager::book_iterator bit = ords.first; bit != ords.second; ++bit) {
                    bit->snprint(buf, 100);
                    printf("%s", buf);
                }
                printf("\n");
            }
            */
            
        }
         void printTrBook(int cid, int n, int m) {
            size_t asz, bsz;
            double apx, bpx;
            bool bid, ask;
            printf("----- Book for %s min size %d -----\n", 
                    dm.symbol(cid), m);
            int i = 0;
            do {
                bid = getTradableMarket(dm.masterBook(), cid, Mkt::BID, i, 100, &bpx, &bsz);
                ask = getTradableMarket(dm.masterBook(), cid, Mkt::ASK, i, 100, &apx, &asz);
                if (bid && ask) printf("%7lu @ %7.2f |%1d| %7.2f @ %7lu \n", bsz, bpx, i, apx, asz);
                else if (bid)   printf("%7lu @ %7.2f |%1d| ------- @ ----- \n", bsz, bpx, i); 
                else if (ask)   printf("----- @ ------- |%1d| %7.2f @ %7lu \n", i, apx, asz);
                ++i;
            } while ((bid || ask) && i < n);
        }
        void printSubBook(int cid, int n, ECN::ECN ecn) {
            if (!n) return;
            size_t asz, bsz;
            double apx, bpx;
            bool bid, ask;
            printf("----- %4s Book for %s -----\n", 
                    ECN::desc(ecn), dm.symbol(cid));
            int i = 0;
            do {
                bid = getMarket(dm.subBook(ecn), cid, Mkt::BID, i, &bpx, &bsz);
                ask = getMarket(dm.subBook(ecn), cid, Mkt::ASK, i, &apx, &asz);
                if (bid && ask) printf("%7lu @ %7.2f |%1d| %7.2f @ %7lu \n", bsz, bpx, i, apx, asz);
                else if (bid)   printf("%7lu @ %7.2f |%1d| ------- @ ----- \n", bsz, bpx, i); 
                else if (ask)   printf("----- @ ------- |%1d| %7.2f @ %7lu \n", i, apx, asz);
                ++i;
            } while ((bid || ask) && i < n);
            /*
            for (int s = 0; s< 2; ++s) {
                pair<DataManager::book_iterator, DataManager::book_iterator> ords 
                    = dm.getSubMarketList(ecn, cid, Mkt::Side(s), 0);
                char buf[100];
                for (DataManager::book_iterator bit = ords.first; bit != ords.second; ++bit) {
                    BookElem be;
                    bit->snprint(buf, 100);
                    if (dm.getBookElem(ecn, bit->id(), be)) {
                        printf("%s", buf);
                        be.snprint(buf, 100);
                        printf("=%s ", buf);
                    } else {
                        printf("%s=#F ", buf);
                    }
                        
                }
                printf("\n");
            }
            */
        }
        void printTrSubBook(int cid, int n, ECN::ECN ecn, int m) {
            size_t asz, bsz;
            double apx, bpx;
            bool bid, ask;
            printf("----- %4s Book for %s with min size %d -----\n",
                    ECN::desc(ecn), dm.symbol(cid), m);
            int i = 0;
            do {
                bid = getTradableMarket(dm.subBook(ecn), cid, Mkt::BID, i, 100, &bpx, &bsz);
                ask = getTradableMarket(dm.subBook(ecn), cid, Mkt::ASK, i, 100, &apx, &asz);
                if (bid && ask) printf("%7lu @ %7.2f |%1d| %7.2f @ %7lu \n", bsz, bpx, i, apx, asz);
                else if (bid)   printf("%7lu @ %7.2f |%1d| ------- @ ----- \n", bsz, bpx, i);
                else if (ask)   printf("----- @ ------- |%1d| %7.2f @ %7lu \n", i, apx, asz);
                ++i;
            } while ((bid || ask) && i < n);
        }

        void printSizes(int cid, int n, ECN::ECN ecn) {
            int asz, bsz;
            double apx, bpx;
            printf("----- %4s size for %s -----\n",
                    ECN::desc(ecn), dm.symbol(cid));
            int i = 0;
            getMarket(dm.masterBook(), cid, Mkt::BID, 0, &bpx, 0);
            getMarket(dm.masterBook(), cid, Mkt::ASK, 0, &apx, 0);
            do {
                bsz = getMarketSize(dm.subBook(ecn), cid, Mkt::BID, bpx);
                asz = getMarketSize(dm.subBook(ecn), cid, Mkt::ASK, apx);

                printf("%5d @ %7.2f |%1d| %7.2f @ %5d \n", bsz, bpx, i, apx, asz);
                ++i;
                apx += 0.01;
                bpx -= 0.01;
            } while (i < n);
        }


        void attemptTrade ( int cid ) {
            if (validMarket(dm.masterBook(), cid) && changed[cid]) {
                size_t asz = 0, bsz = 0;
                double apx = 0.0, bpx = 0.0;
                if (!getMarket(dm.masterBook(), cid, Mkt::BID, 0, &bpx, &bsz)) {
                    printf("getMarket BID returned false\n");
                    return;
                }
                if (!getMarket(dm.masterBook(), cid, Mkt::ASK, 0, &apx, &asz)) {
                    printf("getMarket ASK returned false\n");
                    return;
                }

                double mpx  = (apx*bsz + bpx*asz) / (asz + bsz);
                double bsig = (mpx - bpx) / (apx - bpx);
                double asig = (apx - mpx) / (apx - bpx);

                lib3::OrderBook *book = dm.orderBook();
                int amine = getMarketSize(book, cid, Mkt::ASK, apx);
                int bmine = getMarketSize(book, cid, Mkt::BID, bpx);

                int atot = 1000; 
                int btot = 1000; 
                if (bsig > trdThr && bmine < 1000 && btot < 10000) {
                    ECN::ECN best_ecn;
                    int sz, maxsz = 0;
                    double fee;
                    for (int n = 0; n < ECN::ECN_size; ++n) {
                        ECN::ECN ecn = ECN::ECN(n);
                        if ((sz = getMarketSize(dm.subBook(ecn), cid, Mkt::BID, bpx)) > maxsz)  {
                            best_ecn = ecn;
                            maxsz = sz;
                        }
                    }
                    if (maxsz == 0) {
                        printf("Surprise! no best ECN.\n");
                        printf("getMarket said: %u %.2f x %.2f %u\n", bsz, bpx, apx, asz);
                        printAnyBook(dm.masterBook(), cid, 3);
                        for (int n = 0; n < ECN::ECN_size; ++n) {
                            ECN::ECN ecn = ECN::ECN(n);
                            printf("-- %s --\n", ECN::desc(ecn));
                            printAnyBook(dm.subBook(ecn), cid, 3);
                        }
                        return;
                    }

                    printf("Placing buy order %s (%f) on %s for $%.2f\n", 
                            dm.symbol(cid), bsig, ECN::desc(best_ecn), fee);
                    int ordid;
                    dm.placeOrder(cid, best_ecn, 100, bpx, Mkt::BID, 3600, false, &ordid);
                    printf ("\tPlaced buy order %d\n", ordid);
                }
                if (asig > trdThr && amine < 1000 && atot < 10000) {
                    ECN::ECN best_ecn;
                    int sz, maxsz = 0;
                    double fee;
                    for (int n = 0; n < ECN::ECN_size; ++n) {
                        ECN::ECN ecn = ECN::ECN(n);
                        if ((sz = getMarketSize(dm.subBook(ecn), cid, Mkt::ASK, apx)) > maxsz)  {
                            best_ecn = ecn;
                            maxsz = sz;
                        }
                    }
                    if (maxsz == 0) {
                        printf("Surprise! no best ECN.\n");
                        printf("getMarket said: %u %.2f x %.2f %u\n", bsz, bpx, apx, asz);
                        printAnyBook(dm.masterBook(), cid, 3);
                        for (int n = 0; n < ECN::ECN_size; ++n) {
                            ECN::ECN ecn = ECN::ECN(n);
                            printf("-- %s --\n", ECN::desc(ecn));
                            printAnyBook(dm.subBook(ecn), cid, 3);
                        }
                        return;
                    }

                    printf("Placing sell order %s (%f) on %s for $%.2f\n", 
                            dm.symbol(cid), bsig, ECN::desc(best_ecn), fee);
                    int ordid;
                    dm.placeOrder(cid, best_ecn, 100, apx, Mkt::ASK, 3600, false, &ordid);
                    printf ("\tPlaced sell order %d\n", ordid);
                }
                if (bsig < cxlThr /*&& dm.openOrders(cid, Mkt::BID, bpx) > 0*/) {
                    printf("Canceling orders on %s @ %f (%f)\n", dm.symbol(cid), bpx, bsig);
                    dm.cancelMarket(cid, Mkt::BID);
                }
                if (asig < cxlThr /*&& dm.openOrders(cid, Mkt::ASK, apx) > 0*/) {
                    printf("Canceling orders on %s @ %f (%f)\n", dm.symbol(cid), apx, asig);
                    dm.cancelMarket(cid, Mkt::ASK);
                }
            }
        }
};

TimeVal parseTimeVal ( const string &s ) {
    int dot = s.find('.');
    if (dot == string::npos) {
        return TimeVal(atoi(s.c_str()), 0);
    } else {
        return TimeVal(atoi(s.substr(0, dot).c_str()), atoi(s.substr(dot+1, s.size()).c_str()));
    }
}

int main ( int argc, char **argv ) {
    bool help = false, trading = false, go = false, showdata = false;
    int min = 0, bls = 0;
    factory<DataManager>::pointer dm;
    CmdLineFileConfig cfg (argc, argv, "config,C");
    double cxlt, trdt;

    vector<string> oneoffs, repeats;

    cfg.defSwitch("help,h", &help, "print this help.");
    cfg.defOption("minsize,m", &min, "min size");
    cfg.defSwitch("trade", &trading, "run with trading");
    cfg.defSwitch("go-now", &go, "start now");
    cfg.defOption("show-data", &showdata, "show individual data updates.");
    cfg.defOption("cxlthr", &cxlt, "mpx cancel threshold", 0.1);
    cfg.defOption("trdthr", &trdt, "mpx trade threshold", 0.4);
    cfg.defOption("book-levels", &bls, "print out # of book levels", 5);

    cfg.defOption("oneoff", &oneoffs, "one-off timers");
    cfg.defOption("repeat", &repeats, "repeat timers");

    
    dm.reset(new DataManager());
    factory<DataManager>::insert(only::one, dm);
    
    cfg.add(*dm);
    cfg.add(*debug_stream_config::get_config());
    cfg.add(*file_table_config::get_config());
    cfg.configure();
    if (help) { cerr << cfg << endl; return 1; }

    if(!dm->initialize()) return 2;

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

    //rj.resize(dm->cidsize());
    SimpleTrader trd(trdt, cxlt);
    trd.min = min;
    trd.trade = trading;
    trd.go = go;
    trd.showdata = showdata;
    trd.levels = bls;
    dm->add_listener(&trd);
    
    dm->run();

    return 0;
}
