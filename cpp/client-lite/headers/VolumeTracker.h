#ifndef _VOLUMETRACKER_H_
#define _VOLUMETRACKER_H_

#include "DataManager.h"
#include "DataUpdates.h"
#include <vector>
#include <cl-util/factory.h>

using clite::util::factory;

class VolumeTracker : public MarketHandler::listener, TimeHandler::listener {

    protected:
    factory<DataManager>::pointer dm;
    vector<int> volv;
    vector<double> vpxv;
    vector<double> lpxv;
    vector<double> ftrd;
    bool mktopen;

    public:
        VolumeTracker ( ) : dm(factory<DataManager>::find(clite::util::only::one)), 
                                             volv(dm->cidsize(), 0), 
                                             vpxv(dm->cidsize(), 0.0),
                                             lpxv(dm->cidsize(), 0.0),
                                             ftrd(dm->cidsize(), 0.0),
                                             mktopen(false)
                                        
        { dm->add_listener(this); }
        virtual ~VolumeTracker ( ) { }

        int volume ( int cid ) { return volv[cid]; }
        double vwap ( int cid ) { return vpxv[cid] / (double)volv[cid]; }
        double lastTrade ( int cid ) { return lpxv[cid]; }
        double firstTrade ( int cid ) { return ftrd[cid]; }
        void reset ( ) {
            volv.clear(); volv.resize(dm->cidsize(), 0);
            vpxv.clear(); vpxv.resize(dm->cidsize(), 0.0);
            lpxv.clear(); lpxv.resize(dm->cidsize(), 0.0);
            ftrd.clear(); ftrd.resize(dm->cidsize(), 0.0);
        }
            
    void update ( const DataUpdate &du ) {
        if (du.isTrade()) {
            volv[du.cid] += du.size;
            vpxv[du.cid] += du.size * du.price;
            lpxv[du.cid]  = du.price;
            if (mktopen && ftrd[du.cid] == 0.0) ftrd[du.cid] = du.price;
        }
    }
    void update ( const TimeUpdate & tu) {
        if (tu.timer() == dm->marketOpen())
            mktopen = true;
        else if (tu.timer() == dm->marketClose())
            mktopen = false;
    }


        void wakeup ( ) { };
};

#endif
