#include <DataManager.h>
#include <BookTools.h>
#include <cl-util/Configurable.h>

#include <boost/algorithm/string.hpp>

#include <iostream>
#include <vector>
#include <string>

#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>

using namespace std;
using namespace clite::util;

class SimDebug : 
    public WakeupHandler::listener,
    public MarketHandler::listener,
    public OrderHandler::listener

{

    factory<DataManager>::pointer dm_;
    DataManager &dm;
    char buf[160];

    bool showOrder, showData, showMaster;
    vector<bool> showBook;

    bool nextData, nextOrder, nextMove, nextWake, advancing;
    TimeVal nexttv;

    public:

    SimDebug ( ) 
        : dm_(factory<DataManager>::get(only::one)),
        dm(*dm_),
        showOrder(false), showData(false),
        showMaster(false), showBook(ECN::ECN_size, false),
        nextData(true), nextOrder(true), 
        nextMove(true), nextWake(true)

    { dm.add_listener(this); }

    void interact () {
        using namespace boost::algorithm;

        while (true) {
            cout << (trc::compat::util::DateTime(dm.curtv()).gettimestring()) << "> ";
            cout.flush();
            string input;
            vector<string> sres;
            getline(cin, input);

            split(sres, input, is_space(), token_compress_on);

            if (sres.size() == 0) { continue; }

            // <symbol> <b/s> <size> <price> <ecn> <timeout>
            
            if (sres[0] == "buy" || sres[0] == "sell" || sres[0] == "short") {
                if (sres.size() != 6) {
                    cerr << "trade needs 6 args: <buy/sell/short> <size> <symbol> <price> <ecn> <timeout>." << endl;
                    continue;
                }
                int cid = dm.cid(sres[2].c_str());
                if (cid == -1) {
                    cerr << "symbol " << sres[1] << " not known." << endl;
                    continue;
                }

                Mkt::Side dir;
                if (sres[0] == "buy") dir = Mkt::BID;
                else if (sres[0] == "sell") dir = Mkt::ASK;
                else continue;
                int size = atoi(sres[1].c_str());
                double px = atof(sres[3].c_str());
                ECN::ECN ecn = ECN::ECN_parse(sres[4].c_str());
                int time = atoi(sres[5].c_str());
                dm.placeOrder(cid, ecn, size, px, dir, time);
                continue;
            }

            if (sres[0] == "cxl" && sres.size() > 1) {
                int cid = dm.cid(sres[1].c_str());
                if (cid == -1) { cerr << "symbol not known." << endl; continue; }
                if (sres.size() > 2) {
                    double px = atof(sres[2].c_str());
                    dm.cancelMarket(cid, Mkt::BID, px);
                    dm.cancelMarket(cid, Mkt::ASK, px);
                } else {
                    dm.cancelMarket(cid);
                }
                continue;
            }
            if (sres[0] == "show" && sres.size() > 2) {
                bool on = sres[2] == "on";
                if (sres[1] == "data") showData = on;
                if (sres[1] == "order") showOrder = on;
                if (sres[1] == "master") showMaster = on;
                ECN::ECN ecn = ECN::ECN_parse(sres[1].c_str());
                if (ecn != ECN::UNKN) {
                    showBook[ecn] = on;
                }

                continue;
            }

            if ((sres[0] == "n" || sres[0] == "next") && sres.size() == 1) {
                nextData = nextOrder = nextMove = true;
                return;
            }
            if ((sres[0] == "n" || sres[0] == "next") && sres.size() > 1) {
                if (sres[1] == "data")  nextData = true;
                else if (sres[1] == "order") nextOrder = true;
                else if (sres[1] == "move")  nextMove = true;
                else if (sres[1] == "wake")  nextWake = true;
                else continue;

                return;
            }

            if (sres[0] == "pos") {
                for (int i = 0; i < dm.cidsize(); ++i) 
                    cout << dm.symbol(i) << ":\t" << dm.position(i) << endl;
                continue;
            }
            if (sres[0] == "print" && sres.size() > 2) {
                int cid = dm.cid(sres[1].c_str());
                ECN::ECN ecn = ECN::ECN_parse(sres[2].c_str());

                if (cid != -1) {
                    if (sres[2] == "book") {
                        printBook(cid);
                        continue;
                    } else if (sres[2] == "orders") {
                        printOrders(cid);
                        for (int i = 0; i < ECN::ECN_size; ++i)
                            printOrders(cid, ECN::ECN(i));
                        continue;
                    } else if (ecn != ECN::UNKN) {
                        printBook(cid, ecn);
                        continue;
                    }
                }
            }

            if (sres[0] == "adv" && sres.size() > 1) {
                vector<string> times;
                split(times, sres[1], is_any_of(":."));
                int hh = 0, mm = 0, ss = 0, us = 0;
                trc::compat::util::DateTime dt;
                switch(times.size()) {
                    case 4: us = atoi(times[3].c_str());
                    case 3: ss = atoi(times[2].c_str());
                    case 2: mm = atoi(times[1].c_str());
                    case 1: hh = atoi(times[0].c_str());
                            dt = dm.curtv();
                            dt.setmidnight();
                            dt.settime(hh,mm,ss);
                            nexttv = dt.getTimeVal();
                            nexttv.usec() = us;
                            advancing = true;
                            break;
                    default:
                            cerr << "time must be specified with 1-4 elements" << endl;
                            continue;
                }
                return;
            }

            if (sres[0] == "quit") {
                dm.stop("Debug: quit");
            }

            cerr << "I didn't understand that." << endl;
        }
    }

    void update ( const WakeUpdate &w ) {
        if (nextWake) {
            nextWake = false;
            interact();
        } else if (advancing && dm.curtv() >= nexttv) {
            advancing = false;
            interact();
        }
    }

    void update(const DataUpdate &du) {
        if (showData) {
            du.snprint(buf, sizeof(buf));
            cout << buf << endl;
        }
        if (showBook[du.ecn]) printBook(du.cid, du.ecn);
        if (showMaster) printBook(du.cid);
        if (nextData) {
            nextData = false;
            interact();
        }
    }

    void update (const OrderUpdate &ou) {
        if (showOrder) {
            ou.snprint(buf, sizeof(buf));
            cout << buf << endl;
        }
        if (nextOrder) {
            nextOrder = false;
            interact();
        }
    }

    void printBook(int cid) {
        int n = 10;
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
    }

    void printBook(int cid, ECN::ECN ecn) {
        int n = 10;
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
    }

    void printOrders(int cid) {
        size_t asz, bsz;
        double apx, bpx;
        bool bid, ask;
        printf("----- My Order Book for %s -----\n",
                dm.symbol(cid));
        int i = 0;
        do {
            bid = getMarket(dm.orderBook(), cid, Mkt::BID, i, &bpx, &bsz);
            ask = getMarket(dm.orderBook(), cid, Mkt::ASK, i, &apx, &asz);
            if (bid && ask) printf("%7lu @ %7.2f |%1d| %7.2f @ %7lu \n", bsz, bpx, i, apx, asz);
            else if (bid)   printf("%7lu @ %7.2f |%1d| ------- @ ----- \n", bsz, bpx, i);
            else if (ask)   printf("----- @ ------- |%1d| %7.2f @ %7lu \n", i, apx, asz);
            ++i;
        } while (bid || ask);
    }
    void printOrders(int cid, ECN::ECN ecn) {
        size_t asz, bsz;
        double apx, bpx;
        bool bid, ask;
        printf("----- My %s Order Book for %s -----\n",
                ECN::desc(ecn), dm.symbol(cid));
        int i = 0;
        do {
            bid = getMarket(dm.subOrderBook(ecn), cid, Mkt::BID, i, &bpx, &bsz);
            ask = getMarket(dm.subOrderBook(ecn), cid, Mkt::ASK, i, &apx, &asz);
            if (bid && ask) printf("%7lu @ %7.2f |%1d| %7.2f @ %7lu \n", bsz, bpx, i, apx, asz);
            else if (bid)   printf("%7lu @ %7.2f |%1d| ------- @ ----- \n", bsz, bpx, i);
            else if (ask)   printf("----- @ ------- |%1d| %7.2f @ %7lu \n", i, apx, asz);
            ++i;
        } while (bid || ask);
    }
};

int main ( int argc, char **argv ) {

    bool help = false;

    factory<DataManager>::pointer dm(factory<DataManager>::get(only::one));
    CmdLineFileConfig cfg(argc, argv, "config,C");

    cfg.defSwitch("help,h", &help, "print this help.");

    cfg.add(*dm);
    cfg.add(*debug_stream_config::get_config());
    cfg.add(*file_table_config::get_config());
    cfg.configure();
    if (help) { cerr << cfg << endl; return 1; }

    if(!dm->initialize()) return 2;

    SimDebug dbg;

    dm->run();
}
