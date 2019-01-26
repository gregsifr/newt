/*
  ASTMain.h
  Main function for running AlphaSignalTester, aka for testing performabce of an AlphaSignal
    against high-frequency data.
*/

#ifndef __ASTMAIN_H__
#define __ASTMAIN_H__

#include "Configurable.h"

#include <string>
using std::string;

class ASTConfigHelper : public Configurable {
public:
  string exchangeFile;  // Name of file with mapping from ticker --> exchange.
   
  // Which ECNs engine listens to, and is allowed to trade on.
  bool listenISLD, listenBATS, listenARCA, listenNYSE;
  
  string taelLogFile;

  bool useVolume;        // EW all trades (false), or treat each round-lot as separate data point (true).
  bool printTicks;       // Print debug/daignostic info on each incoming tick?
  bool exitOnClose;      // Exit when see market close message.

  // Booleans specifying whether to include signals of various flavors.
  bool signalKFRTBasic;  // Include a basic KFRT signal.
  bool signalKFRTETF;    // Include an ETF KFRT signal.
  bool signalKFRTExpMBarra;  // Include a KFRT signal that uses Barra as Explanatory Model.
  bool signalPLSOB;      // Include a PLSOB signal.
  bool signalSharesTraded;
  bool signalSharesTradedBuyerTL;
  bool signalSharesTradedSellerTL;
  bool signalVolatility;

  // Params specific to various types of signals.
  string etfBetaFile;    // File holding stock --> ETF mapping + beta.
  
  string plsobKFile;     // File holding K & pZero value estimates for PLSOB model.

  string barraRiskFile;          // Location of barra .RSK file containing stock factor betas.
  string barraIndexWeightsFile;  // Location of file containing weights for synthetic indeces used to
                                 //   construct analogues of the Barra risk factors, given the population
                                 //   set with which the program is run.
  bool checkBarraWeights;        // Check that index weights add up to expected value.
  bool printBarraMatrices;       // Dump text represnetation of barra factorBetas & factorWeights matrices to stdout?

  int timerFreq;                 // Global timer frequency.
  int sampleFreq;                // How frequentyly to sample AlphaSignalObservations.

  ASTConfigHelper();
  virtual ~ASTConfigHelper();
  void setDefaultValues();
  void setConfigOptions();
};



#endif   //  __ASTMAIN_H__
