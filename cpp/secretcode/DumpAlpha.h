#ifndef __DUMPALPHA_H__
#define __DUMPALPHA_H__

#include <cl-util/factory.h>
using namespace clite::util;

#include "DataManager.h"
#include "HFUtils.h"
#include "ImbTracker.h"
#include "KFRTSignal.h"
#include "UCS1.h"

#include <string>
using std::string;

#include "c_util/Time.h"
using trc::compat::util::TimeVal;

class DumpAlpha: public TimeHandler::listener {
	int updateFreq; // in milliseconds
	factory<DataManager>::pointer dm;
	factory<UCS1>::pointer ucs1Signal;
	factory<ImbTracker>::pointer imbSignal;
	factory<ETFKFRTSignal>::pointer kfrtSignal;

	bool marketOpen;
	TimeVal lastPrintTV;
	tael::Logger alphalog;
	boost::shared_ptr<tael::FdLogger> alphafld;
  public:
    DumpAlpha(int freqMsec=60000);
    void update(const TimeUpdate &tu);
    void printAlphaSignals();
};

#endif // __DUMPALPHA_H
