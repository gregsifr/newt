/*
  AlphaSignal.h 
  Code for initial attempt at describing alpha/retrurn predicting signal.

  Notes:
  - Simplified version for use with lib3/neworder branch.  Removes versions of AlphaSignal
    interface that allowed scenario analysis, or signal calculation conditional on orders
    being filled.
*/

#ifndef __ALPHASIGNAL_H__
#define __ALPHASIGNAL_H__

#include <vector>
using std::vector;

/*
  Abstract base class/interface for alpha signal.  Assumes:
  - Signal does not have time component.  Aka, can query for current expected alpha over indefinate
    future period.  But not over explicit period.
  - Allows user to query for alpha/signal expectation given current state of system, but does
    not provide easy way of doing scenario analysis, or alpha conditional on various things 
    (e.g. conditional on fill).
  - Currently only provides info on expected alpha, and on on variance/error-bar/
    uncertainty of that signal payoff.  Hopefully this is a plausible interface for high-frequency
    signals, although its probably less useful for lower frequency stuff where the traditional
    mean-variance tradeoff becomes more important.
*/
class AlphaSignal {
 protected:

 public:

  /*
    Public Interface Functions.
  */
  // Get return forecast for specified stock, given most recent information.
  // Assumes no time dimension of alpha forecast, aka assumes alpha forecast is invariant of time.
  // Does not take additional parameters, does not allow caller to ask for forecasts
  //   conditional on various scenarios.
  // Attempts to populate both point estimate of current fair-value, and variance estimate in
  //   that fair-value estimate.
  // Both alpha point-estimate, and estimate variance should be in *return space*, not px space.
  //   aka:  0.01 = alpha of +1%, not fair-value of 1 penny.
  // If signal has an alpha forecast, returns true and populates both alphaEst and alphaVar.
  //   NA is represented by returning false, and setting *fv to 0.
  // Alpha is here defined as signed (+ = going up, - = going down)
  //   market-relative expected return over some (undefined) time horizon, 
  //   conditioned on all information used to compute alpha signal.
  // It is permissable for a signal to populate alphaVar with NAN to signify that
  //   it can generate a point-estimate, but not an error-bar around that estimate.
  // It is up to the caller to respond to that information correctly.

  virtual ~AlphaSignal(){};

  virtual bool getAlpha(int cid, double &alphaEst){return false;}
};

/*
  Simple place-holder, allows someone/something to specify alpha, but
    includes no logic for internal calculation.
  Included to facilitate testing order-placement logic in absence of real
    LT signal to test against.
*/
class PlaceHolderAlphaSignal : public AlphaSignal {
 protected:
  vector<double> _alphaV;
 public:
  // Assumes population set size known in advance.
  // dv : default value.
  PlaceHolderAlphaSignal(int cidsize, double dv);
  virtual ~PlaceHolderAlphaSignal();

  // standard AlphaSignal interface functions.
  virtual bool getAlpha(int cid, double &alphaEst);

  // subclass specific interface functions
  virtual bool setAlpha(int cid, double alphaEst);
};




#endif    //  __ALPHASIGNAL_H__
