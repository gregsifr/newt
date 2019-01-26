/*
  EMTMain.h
  Main function for running ExplanatoryReturnModelTester, aka for testing performabce of an ExplanatoryReturnModel
    against high-frequency data.
*/

#ifndef __EMTMAIN_H__
#define __EMTMAIN_H__

#include "Configurable.h"

#include <string>
using std::string;

class EMTConfigHelper : public Configurable {
 public:
  string taelLogFile;

  string fvSignalFlavor;  // Signal to use for estiming stock fair-values, for computiong actual returns.
                          // Default of "" ==> no signal, aka use straight stated mid prices.
  string ermFlavor;       // Type of explanatory return model to test.
  int sampleMilliSeconds; // Sampling frequency.  Default = 10,000 = sample every 10 seconds.
 
  string ermEtfMidBetaFile;  // SPY beta file for ERM-ETF flavor.
  string mktCapFile;      // File containing ticker --> mkt cap.  Used for LFM-VW erm flavor.

  string barraRiskFile;          // Location of barra .RSK file containing stock factor betas.
  string barraIndexWeightsFile;  // Location of file containing weights for synthetic indeces used to
                                 //   construct analogues of the Barra risk factors, given the population
                                 //   set with which the program is run.
  bool checkBarraWeights;        // Check that index weights add up to expected value.
  bool printBarraMatrices;       // Dump text represnetation of barra factorBetas & factorWeights matrices to stdout?
 
  EMTConfigHelper();
  virtual ~EMTConfigHelper();
  void setDefaultValues();
  void setConfigOptions();  
};

#endif  //__EMTMAIN_H__
