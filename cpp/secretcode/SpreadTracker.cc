#include "SpreadTracker.h"



const string RELEVANT_LOG_FILE = "misc";
const int _nperiods = 120;

SpreadTracker::SpreadTracker():
  _dm(factory<DataManager>::find(only::one)),
  _ddebug(factory<debug_stream>::get( RELEVANT_LOG_FILE )),
  _stocksState( factory<StocksState>::get(only::one) ) ,
  _sample(Timer(TimeVal(1,0),TimeVal(0,0)))
{
  //  _sample = Timer(TimeVal(1,0),TimeVal(0,0));
  _dm->addTimer(_sample);
  _spdbuffer.resize(_dm->cidsize());
  for ( int i=0;i<_dm->cidsize();i++) {
    _spdbuffer[i] = new CircBuffer<double>(_nperiods);
  }
  _dm->add_listener(this);
}

SpreadTracker::~SpreadTracker(){
    for ( int i=0;i<_dm->cidsize();i++) 
      delete _spdbuffer[i];
}

void SpreadTracker::update( const TimeUpdate &t){
  if (t.timer() == _sample){
    for (int cid=0;cid<_dm->cidsize();cid++){
      double spd;
      _stocksState->getState(cid)->lastNormalSpread(&spd);
      _spdbuffer[cid]->add(spd);
    }
  }
}

bool SpreadTracker::getAvgspd(int cid, double *aspd){
  return (_spdbuffer[cid]->getAvg(aspd));
}
