/*
  Simple main/driver program that prints trade ticks.
*/

#ifndef __TTPMAIN_H__
#define __TTPMAIN_H__

#include "Configurable.h"

#include <string>
using std::string;

class TTPConfigHelper : public Configurable {
public:
   
  // Which ECNs engine listens to, and is allowed to trade on.
  bool listenISLD, listenBATS, listenARCA, listenNYSE;

  TTPConfigHelper();
  virtual ~TTPConfigHelper();
  void setDefaultValues();
  void setConfigOptions();
};



#endif   //  __TTPMAIN_H__
