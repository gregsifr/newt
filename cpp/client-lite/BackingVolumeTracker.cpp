#include <boost/algorithm/string.hpp>
#include "BackingVolumeTracker.h"
#include <iostream>
#include <vector>
#include <cstdio>

using namespace std;
using namespace boost::algorithm;

bool BackingVolumeTracker::initialize() {

    char buf[160];
    vector<int> latest(0, dm.cidsize());
    
    ifstream ifs(file.c_str(), ifstream::in);
    
    if (!ifs) {
        return false;
    }

    vector<string> splitres;
    
    // clear voltracker state and resize to reasonable size.
    reset();
    
    while (ifs.good()) {
        ifs.getline(buf, 160);
        split(splitres, buf, is_any_of(" \t,"), token_compress_on);
        if (splitres.empty() || splitres[0] == "" || splitres[0][0] == '#') continue;
        
        if (splitres.size() != 4) return false;

        int cid = dm.cid(splitres[0].c_str());
        if (cid != -1) {
            volv[cid] = atoi(splitres[1].c_str());
            vpxv[cid] = atof(splitres[2].c_str());
            lpxv[cid] = atof(splitres[3].c_str());
        }
    }

    return true;
}

bool BackingVolumeTracker::write() {

    string wrname = file+backext;
    ofstream ofs( wrname.c_str(), ofstream::out);
    if (!ofs) return false;

    ofs << "# Backing store for Volume Tracker generated "
        << TVtoTimeStr(TimeVal::now) << endl
        << "# This file is not robust to human editing!" << endl
        << "# SYMBOL\tVolume\tVol*Px\tLast trade" << endl;
    
    for (unsigned int i = 0; i < dm.cidsize(); ++i) {
        ofs << dm.symbol(i) << "\t" 
            << volv[i] << "\t"
            << vpxv[i] << "\t"
            << lpxv[i] << endl;
    }
    
    ofs.close();
    if (!rename(wrname.c_str(), file.c_str())) return false;
    return true;
}

void BackingVolumeTracker::update ( const DataUpdate &du ) {
    if (writeival != 0 && lastmod < (dm.curtv().sec() / writeival)) {
        lastmod = dm.curtv().sec() / writeival;
        write();
    }
    VolumeTracker::update(du);
}
