#include <DataManager.h>
#include <DataUpdates.h>
#include <cstdio>

#include <cl-util/debug_stream.h>
#include <cl-util/table.h>
#include <clite/message.h>


using namespace std;
using namespace clite::util;
using namespace clite::message;

class Watcher : 
    public dispatch<OrderUpdate>::listener 
{
    public:

    bool rej;
    factory<DataManager>::pointer dm;

    Watcher ( ) : dm(factory<DataManager>::get(only::one)) { }


    void update ( const OrderUpdate &ou ) {
        char buf[256];
        ou.snprint(buf, 255);
        if (!rej || ou.action() == Mkt::REJECTED)
            printf("%s\n", buf);
    }

};

int main ( int argc, char **argv ) {

    bool help;
    bool rej;

    factory<DataManager>::pointer dm = factory<DataManager>::get(only::one);
    Watcher trd;
    CmdLineFileConfig cfg(argc, argv, "config,C");
    cfg.defSwitch("help,h", &help, "print this help.");
    cfg.defSwitch("rejects,R", &rej, "rejects only");
    cfg.add(*dm);
    cfg.add(*file_table_config::get_config());
    cfg.add(*debug_stream_config::get_config());
    cfg.configure();
    if (help) { cerr << cfg << endl; return 1; }
    dm->initialize();
    dm->add_listener(&trd);
    trd.rej = rej;
    
    dm->run();
} 
