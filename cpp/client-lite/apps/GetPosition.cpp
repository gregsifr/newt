#include <DataManager.h>
#include <DataUpdates.h>
#include <cstdio>

#include <cl-util/debug_stream.h>
#include <cl-util/table.h>
#include <clite/message.h>

using namespace std;
using namespace clite::util;
using namespace clite::message;


int main ( int argc, char **argv ) {

    bool help;

    factory<DataManager>::pointer dm = factory<DataManager>::get(only::one);
    CmdLineFileConfig cfg(argc, argv, "config,C");
        cfg.defSwitch("help,h", &help, "print this help.");
    cfg.add(*dm);
    cfg.add(*file_table_config::get_config());
    cfg.add(*debug_stream_config::get_config());
    cfg.configure();
    if (help) { cerr << cfg << endl; return 1; }
    dm->initialize();
    bool pos = dm->getPositions();
    bool loc = dm->getLocates();
    if (!pos) {
        cerr << "Couldn't get positions!" << endl; 
    }
    if (!loc) {
        cerr << "Couldn't get locates!" << endl; 
    }

    for (int i = 0; i < dm->cidsize(); ++i) {
        cout << dm->symbol(i) << "\t" << dm->position(i) << "\t" << dm->locates(i) << endl;
    }
} 
