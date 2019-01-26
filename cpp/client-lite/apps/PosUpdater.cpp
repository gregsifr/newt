#include <DataManager.h>
#include <DataUpdates.h>
#include <cstdio>

#include <cl-util/debug_stream.h>
#include <cl-util/table.h>
#include <clite/message.h>
#include <c_util/Time.h>

using namespace std;
using namespace clite::util;
using namespace clite::message;

class Watcher : 
  public dispatch<TimeUpdate>::listener
{
    public:

    factory<DataManager>::pointer dm;
    Timer sync_timer;
    bool _mkt_open;
    Watcher ( int sync ) : 
      dm(factory<DataManager>::get(only::one)),
      sync_timer(TimeVal(sync,0),TimeVal(0,0)),
      _mkt_open(false)
    {
      dm->addTimer(sync_timer);
    }

    void update ( const TimeUpdate &tu){
      if ( _mkt_open && (tu.timer() == sync_timer)) {
	printf("Reloading positions and locates, 'cause it's been a while... %s\n",trc::compat::util::DateTime(dm->curtv()).getfulltime());
        dm->getPositionsAsync();
        dm->getLocatesAsync();
      } 

      if (tu.timer() == dm->marketOpen()){
	_mkt_open = true;
      }
    }
};

int main ( int argc, char **argv ) {

    bool help;
    int sync;
    factory<DataManager>::pointer dm = factory<DataManager>::get(only::one);
    CmdLineFileConfig cfg(argc, argv, "config,C");
    cfg.defSwitch("help,h", &help, "print this help.");
    cfg.defOption("sync-freq",&sync,"Wait time before sending a sync request to Margin Server",300);
    cfg.add(*dm);
    cfg.add(*file_table_config::get_config());
    cfg.add(*debug_stream_config::get_config());
    cfg.configure();
    if (help) { cerr << cfg << endl; return 1; }
    Watcher trd(sync);
    dm->initialize();
    dm->add_listener(&trd);
    dm->run();
} 
