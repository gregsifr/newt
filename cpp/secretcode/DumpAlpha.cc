#include "DumpAlpha.h"

DumpAlpha::DumpAlpha(int freqMsec) :
  updateFreq(freqMsec), marketOpen(false),
  ucs1Signal(factory<UCS1>::get(only::one)),
  alphalog(*(new tael::LoggerConfiguration((size_t) MAX_BINARY_BUFFER_FILE_SIZE)))
{
	dm = factory<DataManager>::find(only::one);
	if(!dm)
	    throw std::runtime_error( "Failed to get DataManager from factory (in DumpAlpha::DumpAlpha)" );

	imbSignal = factory<ImbTracker>::get(only::one);
	if(!imbSignal)
		throw std::runtime_error( "Failed to get ImbTracker from factory (in DumpAlpha::DumpAlpha)" );

	kfrtSignal = factory<ETFKFRTSignal>::get(only::one);
	if(!kfrtSignal )
		throw std::runtime_error( "Failed to get KFRTSignal from factory (in DumpAlpha::DumpAlpha)" );

	dm->add_listener(this);
	struct stat buffer ;
	bool alphaFileExists = (stat(dm->getAlphaLogFile(), &buffer) == 0);
	int alphafd = open(dm->getAlphaLogFile(), O_RDWR | O_CREAT | O_APPEND, 0640);
	if (alphafd > -1) {
		alphafld.reset(new tael::FdLogger(alphafd));
		alphalog.addDestination(alphafld.get());
	}
	if(!alphaFileExists) TAEL_PRINTF_PLAIN(&alphalog, TAEL_INFO, "ts|ticker|kfrtAlpha|kfrtWeight|imbAlpha|imbWeight|netAlpha");
}

void DumpAlpha::update(const TimeUpdate &tu) {
  if (tu.timer() == dm->marketOpen()) {
    marketOpen = true;
  } else if (tu.timer() == dm->marketClose()) {
    marketOpen = false;
  }
  double msdiff = HFUtils::milliSecondsBetween(lastPrintTV, tu.tv());
  if(marketOpen && msdiff > updateFreq) {
	  lastPrintTV = tu.tv();
	  printAlphaSignals();
  }
}

void DumpAlpha::printAlphaSignals() {
	double kfrtAlpha, imbAlpha, netAlpha;
	bool hasKFRTAlpha, hasImbAlpha;
	for (int cid = 0; cid < dm->cidsize(); cid++) {
		std::ostringstream ss;
		kfrtAlpha = 0.0; imbAlpha = 0.0; netAlpha = 0.0;
		hasKFRTAlpha = kfrtSignal->getAlpha(cid, kfrtAlpha);
		hasImbAlpha = imbSignal->getAlpha(cid, imbAlpha);
		ss << "|" << dm->symbol(cid) << "|";
		if(hasKFRTAlpha) {
			ss << kfrtAlpha << "|" << ucs1Signal->_kfrtBeta[cid] << "|";
			netAlpha += kfrtAlpha * ucs1Signal->_kfrtBeta[cid];
		} else {
			ss << "NA" << "|" << ucs1Signal->_kfrtBeta[cid] << "|";
		}
		if(hasImbAlpha) {
			ss << imbAlpha << "|" << ucs1Signal->_imbBeta[cid] << "|";
			netAlpha += imbAlpha * ucs1Signal->_imbBeta[cid];
		} else {
			ss << "NA" << "|" << ucs1Signal->_imbBeta[cid] << "|";
		}
		ss << netAlpha;
		string s = ss.str();
		TAEL_PRINTF(&alphalog, TAEL_INFO, "%s", s.c_str());
	}
}
