#include <DataManager.h>
#include <BookTools.h>
#include <cstdio>
#include <vector>
#include <string>
#include <boost/algorithm/string.hpp>

using namespace boost::algorithm;
using namespace std;
using namespace clite::util;
using namespace clite::message;

/* The complicated version? */
/*
class CrossTrader {
    : public UpdateListener 
    , public Configurable {

        DataManager &dm;

        int max ( int a, int b ) { return a > b? a : b; }
        int abs ( int x ) { return max(x, -x); }
    public:

        void wakeup ( ) {
        
        
        }

        bool fillInTrade ( int cid, int pos, int target,
                Mkt::ECN *use_ecn, int *use_sz, double *use_px) {
            using namespace Cmp;

            double fees, price, best_fees = 1000000000, targ_px;
            int size, tr_size = abs(target - pos);
            
            Mkt::Trade trade;
            if (pos < target) trade = Mkt::BUY;
            else if (pos < 0) trade = Mkt::SHORT;
            else trade = Mkt::SELL;
            
            Mkt::Side side = trade == Mkt::BUY? Mkt::ASK : Mkt::BID;
            Mkt::ECN best_ecn = Mkt::UNKN;
            bool bigenough = false;

            if (!dm.getMarket(cid, side, 0, &targ_px, 0)) return false;
            for (int ecn = 0; ecn < Mkt::NUM_ECNS; ++i) {
                int ecn_sz = dm.getSubMktSize(cid, Mkt::ECN(ecn), side, targ_px);
                if (dm.checkOrder(

 */

class BSpecCrossTrader : 
    public dispatch<OrderUpdate>::listener, 
    public dispatch<TimeUpdate>::listener, 
    public Configurable 
{

    factory<DataManager>::pointer dm;
    bool open, waited;
    int deftarget;
    bool sweep;
    ECN::ECN ecn;
    string ecnstr;
    string trpx_offset_str;
    double trpx_offset;
    vector<string> symtarget;
    vector<int> target;
    Timer wait, every;

    public:

    BSpecCrossTrader() : dm(factory<DataManager>::get(only::one)), open(false), waited(false), 
    wait(TimeVal(0,0)), every(TimeVal(0, 0)) {
        defOption("default-target", &deftarget, "target for each stock unless otherwise specified.", 0);
        defOption("symbol-target", &symtarget, "target for a particular symbol: eg 'MSFT,-50,+0.04'");
        defOption("default-limit", &trpx_offset_str, "price offset to go past the far side of the book.", "0.0");
        defOption("sweep", &sweep, "Use BATS intermarket routing");
        defOption("ecn", &ecnstr, "Which ECN to use for odd lots/non-sweep", "ISLD");
    }
        
    bool initialize ( ) {
        target.resize(dm->cidsize(), deftarget);
        for (vector<string>::iterator targ = symtarget.begin(); targ != symtarget.end(); ++targ) {
            vector<string> splitres;
            split(splitres, *targ, is_any_of(" \t,"), token_compress_on);
            int cid;
            if (splitres.size() ==2 && (cid = dm->cid(splitres[0].c_str())) != -1) {
                int t = atoi(splitres[1].c_str());
                target[cid] = t;
                printf("Target of %s is explicitly %d\n", splitres[0].c_str(), t);
            } else {
                printf("Failed to set target: \"%s\"\n", targ->c_str());
                return false;
            }
        }
            ecn = ECN::ECN_parse(ecnstr.c_str());
            if (ecn == ECN::UNKN) {
                printf("No ecn %s\n", ecnstr.c_str());
                return false;
            }

        if (trpx_offset_str[0] == '_')
            trpx_offset_str[0] = '-';

        trpx_offset = atof(trpx_offset_str.c_str());
        printf("Crossing the market %+4.2f\n", trpx_offset);
        
        return true;
    }
        
    void update ( const OrderUpdate &ou ) {
        char buf[256];
        ou.snprint(buf, 255);
        printf("%s\n", buf);
    }

    void update ( const TimeUpdate &t ) {
        if (t.timer() == dm->marketOpen()) {
            if (!dm->getPositions()) {
                printf("Failed to get positions!\n");
                dm->stop("No positions!");
            }
            wait = Timer(dm->curtv() + TimeVal(30,0));
            every = Timer(TimeVal(2,0), TimeVal(0,0));
            dm->addTimer(wait);
            dm->addTimer(every);
        }

        if (t.timer() == dm->marketClose()) open = false;

        if (t.timer() == wait) waited = true;
        if (t.timer() == every && waited) {
            bool done = true;
            for (int cid = 0; cid < dm->cidsize(); ++cid) {

                int pos = dm->position(cid);
                Mkt::Side trade = pos < target[cid]? Mkt::BID : Mkt::ASK;
                Mkt::Side side = trade == Mkt::BID? Mkt::ASK : Mkt::BID;
                size_t size;
                size_t tr_size = abs(target[cid] - pos);
                printf("Trading %s : have %d, target %d, trade is %s %d\n",
                        dm->symbol(cid), pos, target[cid], Mkt::SideDesc[trade], tr_size);

                double px = 0.0;

                if (getMarketCumSize(dm->orderBook(), cid, side == Mkt::BID? Mkt::ASK : Mkt::BID) != 0) {
                    done = false; 
                    continue;
                }
                if (tr_size != 0) {
                    done = false;

                    if (getMarket(dm->masterBook(), cid, side, 0, &px, &size)) {

                        double tr_px = px + ( trade == Mkt::BID? 1.0 : -1.0 ) * trpx_offset;
                        if (tr_size > 100) tr_size = 100;
                        if (tr_size == 100 && sweep) {
                            Mkt::OrderResult res;
                            res = dm->placeBatsOrder(cid, tr_size, tr_px, trade, DataManager::BATS_ALLMARKETS);
                            if (res == Mkt::GOOD)
                                printf("%s: Placed BATS sweep order to %s %s - %lu @ %f \n",
                                        TVtoTimeStr(dm->curtv()), Mkt::TradeDesc[trade], 
                                        dm->symbol(cid), tr_size, px);
                            else 
                                printf("%s: Failed to place BATS sweep order to %s %s - %lu @ %f : %s \n",
                                        TVtoTimeStr(dm->curtv()), Mkt::TradeDesc[trade], 
                                        dm->symbol(cid), tr_size, px, Mkt::OrderResultDesc[res]);
                        } else {
                            Mkt::OrderResult res;
                            res = dm->placeOrder(cid, ecn, tr_size, tr_px, trade, 0);
                            if (res == Mkt::GOOD)
                                printf("%s: Placed %s order to %s %s - %lu @ %f \n",
                                        TVtoTimeStr(dm->curtv()), ECN::desc(ecn), Mkt::TradeDesc[trade], 
                                        dm->symbol(cid), tr_size, px);
                            else 
                                printf("%s: Failed to place %s order to %s %s - %lu @ %f : %s \n",
                                        TVtoTimeStr(dm->curtv()), ECN::desc(ecn), Mkt::TradeDesc[trade], 
                                        dm->symbol(cid), tr_size, px, Mkt::OrderResultDesc[res]);
                        }
                    }
                }
            }

            if (done) {
                dm->stop("Positions equal to targets.");
            }
        }
    }
};


int main ( int argc, char **argv ) {

    bool help;

    factory<DataManager>::pointer dm = factory<DataManager>::get(only::one);
    BSpecCrossTrader trd;
    CmdLineFileConfig cfg(argc, argv, "config,C");
        cfg.defSwitch("help,h", &help, "print this help.");
    cfg.add(*dm);
    cfg.add(trd);
    cfg.add(*file_table_config::get_config());
    cfg.add(*debug_stream_config::get_config());
    cfg.configure();
    if (help) { cerr << cfg << endl; return 1; }
    dm->initialize();
    trd.initialize();
    dm->add_listener(&trd);
    
    dm->run();
}
