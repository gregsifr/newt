#include "LiveDataManager.h"
#include "HistDataManager.h"
#include "VolumeTracker.h"
#include <tael/Log.h>
#include <tael/FdLogger.h>
#include <string>
#include <vector>
using namespace std;

class DayPlotter : public UpdateListener {

    public:
    
    DataManager &dm;
    VolumeTracker vol;
    tael::Logger plotlog;
    bool open;

    vector<double> avgbid;
    vector<double> avgask;
    int ct;
    DayPlotter ( DataManager &dm_, const string &file ) : dm(dm_), vol(dm), open(false) {
    	int fd = open(file.c_str(), O_WRONLY | O_CREAT, 0640);
        plotlog.addDestination(new tael::FdLogger(fd));
        avgask.resize(dm.cidsize(), 0.0);
        avgbid.resize(dm.cidsize(), 0.0);
        ct = 0;
    }
    
    void update ( const AdminUpdate &au ) {
        int bsz, asz;
        double bpx, apx;

        DateTime dt(dm.curtv());
        switch (au.state) {
            case Mkt::MARKET_OPEN:
                open = true;
                break;
            case Mkt::MARKET_CLOSE:
                open = false;
                for (int i = 0; i< dm.cidsize(); ++i)
                plotlog.printf("%4d%02d%02d\t%s\t%f\t%f\t%d\t%f\n",
                        dt.year(), dt.month(), dt.day(),
                        dm.symbol(i), avgbid[i]/ct, avgask[i]/ct, vol.volume(i), vol.vwap(i));
                avgask.clear();
                avgask.resize(dm.cidsize(), 0.0);
                avgbid.clear();
                avgbid.resize(dm.cidsize(), 0.0);
                vol.reset();
                ct = 0;
                break;
            case Mkt::TIMER:
                if (open) {
                    for (int i = 0; i < dm.cidsize(); ++i) {
                        if (dm.getMarket(i, Mkt::BID, 0, &bpx, &bsz) 
                                && dm.getMarket(i, Mkt::ASK, 0, &apx, &asz)) {
                            avgbid[i] += bsz ;
                            avgask[i] += asz ;
                        }
                    }
                   ++ct;
                }
                break;
            default:
                break;
        }
    }

    void wakeup ( ) { }
};

class Plotter : public UpdateListener {

    public:
    
    DataManager &dm;
    VolumeTracker vol;
    Logger plotlog;
    bool open;

    Plotter ( DataManager &dm_, const string &file ) : dm(dm_), vol(dm), open(false) {
    	int fd = open(file.c_str(), O_WRONLY | O_CREAT, 0640);
        plotlog.addDestination(new tael::FdLogger(fd));
    }
    
    void update ( const AdminUpdate &au ) {
        int bsz, asz;
        double bpx, apx;

        switch (au.state) {
            case Mkt::TIMER:
                if (open) {
                    for (int i = 0; i < dm.cidsize(); ++i) {
                        if (dm.getMarket(i, Mkt::BID, 0, &bpx, &bsz) 
                                && dm.getMarket(i, Mkt::ASK, 0, &apx, &asz)) {
                            plotlog.printf("%s\t%d\t%f\t%d\t%f\t%d\t%f\t%s\n",
                                    dm.symbol(i), bsz, bpx, asz, apx, vol.volume(i), vol.vwap(i),
                                    TVtoTimeStr(au.tv));
                        }
                    }
                    vol.reset();
                }
                break;
            case Mkt::MARKET_OPEN:
                open = true;
                break;
            case Mkt::MARKET_CLOSE:
                open = false;
                break;
            default:
                break;
        }
        if (au.state != Mkt::TIMER) {
            char buf[128];
            au.snprint(buf,127);
            fprintf(stderr, "Msg: %s\n", buf);
        }
    }

    void wakeup ( ) { }
};


int main ( int argc, char **argv ) {
    using namespace std;
    bool help;
    string plotlog;
    bool daily;

    if (argc < 2) { cerr << "Need at least one argument!" << endl; return 1; }
    bool live = strncmp(argv[1], "--live", 6) == 0;
    CmdLineFileConfig cfg (argc - 1, argv + 1, "config,C");
    cfg.defSwitch("help,h", &help, "print this help.");
    cfg.defOption("plotlog", &plotlog, "log top of book");
    cfg.defOption("daily", &daily, "average sizes over day.");
    DataManager *dm;
    if (live) dm = new LiveDataManager();
    else dm = new HistDataManager();

    dm->setFeedSource(Mkt::ISLD, Mkt::TOWER);
    dm->setFeedSource(Mkt::ARCA, Mkt::TOWER);
    dm->setFeedSource(Mkt::BATS, Mkt::TOWER);
    dm->setFeedSource(Mkt::NYSE, Mkt::TOWER);
    dm->setFeedSource(Mkt::CBSX, Mkt::NODATA);
    dm->setFeedSource(Mkt::BTRD, Mkt::NODATA);
    dm->setFeedSource(Mkt::EDGA, Mkt::NODATA);
    dm->setFeedSource(Mkt::EDGX, Mkt::NODATA);
    dm->setFeedSource(Mkt::BTRD, Mkt::NODATA);
    dm->setFeedSource(Mkt::TRAC, Mkt::NODATA);

    cfg.add(*dm);
    cfg.configure();
    if (help) { cerr << cfg << endl; return 1; }
    if (!dm->initialize()) {
        cerr << "dm->initialize returned false." << endl;
        return 2;
    }

    UpdateListener *ul;
    if (daily) ul = new DayPlotter (*dm, plotlog);
    else ul = new Plotter (*dm, plotlog);

    dm->addListener(ul);
    dm->run();
    delete dm;
    delete ul;
}


