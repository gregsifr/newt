/*
  KFUtils.cc  :
    C++ utility code for Kalman Filter & related (e.g. KF param estimate) calculation.
*/


#include "KFUtils.h"

#include <math.h>

/*
  Rounding error variance.
  Component of variance introducted by rounding error, aka by the fact that
    prices are observed at discrete points only.
  Notes:
  - Measured in price space, not return space.
*/
double KFUtils::roundingErrorVariance(double px, double mpv) {
  // From simulation, with 10.0 stock px, with 0.01 mpv.
  double base = 1e-05;
  // Higher px --> lower measurement error variance
  //   with square of px, aka price doubles --> meVar goes to 1/4 old value.
  double pxMult = 1 / ((px/10.0) * (px/10.0));
  // Lower mpv --> lower measurement error.
  //   with square of mpv, akak mpv goes in half, meVar goes down to
  //   1/4 old value.
  double mpvMult = (mpv / 0.01) * (mpv/0.01);
  double ret = base * pxMult * mpvMult;
  return ret;
}

/*
  bid-ask bounce error variance.
  Component of measurement error introduced by bid-ask bounce in prices.
  Conceptually:
  - Trade prices tend to bounce between bid & ask, even when the true price does
    not change.
  - This introduces a measurement error term in a price series taken from trades.
  Model (suggested by Amrit):
  - Model trade prices as binomial process X that takes value 0 or 1 (B or A).
  - Assume that "true" price is P.
    Then:
  - E(X) = P.
  - Var(X) = P (1 - P).
    Aka:
  - bid-ask bounce error variance depends on where inside the bid-ask spread the
    "true" mid price actually lies.
  Approaches:
  - One simple approach is to assume that in 1 case "true" mid lies at the mid.  
    Then: bid-ask bounce error would be 0.5 * 0.5 * spread 0.25 (* spread).
    And in case 2, the true mid lies at the bid, in which case bid-asl bounce would
    be 0.
    ==> We use 50% weight on each of these cases, aak spread/8.
    ==> Looking at space of dist, spd/6.0 is probably more accurate.
  - Another approach is to use our lastEstimate for P. 
  Notes:
  - Measured in return space, not price space.
*/
double KFUtils::bidAskBounceErrorVariance(double bid, double ask, double mpv) {
  /*
    Method #1:
  */
  double spread = (ask - bid);
  if (spread < mpv) {
    spread = mpv;
  }
 
  return (spread * spread)/4.0;

  //return spread/6.0;
  /*
    Method #2:
  */
  //double mid = (bid + ask)/2.0;
  //double spread = (ask - bid)/mid;
  //double p = fabs((lastEst - bid)/(ask - bid));
  //if (p > 1.0) {
  //  p -= 1.0;
  //}
  //return p * (1 - p) * spread;
}

/*
  Another component of measurement error variance.  
  Empirically, short-horizon prices seem to have some excess variance
    (aka variance does not scale linearly in time horizon).
  This also causes some measurement-error in prices, which should be
    accounted for in the filtering process.
  A static estimate is used below - which should probably be replaced with 
    a real properly modelled estimate.
  The short-term jitter variance probably differs with time horizon
    (e.g. different me.var for 1 day prices va 1 second prices),
    so the measurement horizon for estimation is important, and should be 
    appropriate to an *execution* model.
*/
double KFUtils::shortTermJitterErrorVariance(double bid, double ask, double mpv) {
  // Semi plausible value, from eyeballing data on a US R1mem tech stocks using
  //   Sep 2009 data.
  return 5e-5;
}

/*
  Measurement Error Variance - Here defined as 
  - rounding error variance +
  - bid-ask bounce error variance.
  assumed to be uncorrelated.
*/
double KFUtils::measurementErrorVariance(double bid, double ask, double mpv) {
  double mid = (bid + ask)/2.0;
  double reVar = roundingErrorVariance(mid, mpv);
  double babVar = bidAskBounceErrorVariance(bid, ask, mpv);
  double stjVar = shortTermJitterErrorVariance(bid, ask, mpv);
  double ret = reVar + babVar + stjVar;
  return ret;
}

/*
  Process innovation.  Aka variance introduced by underlying process noise
    in specified time interval, with specified (per second) volatility (sd).
  Return Value:
  - Return value measured in px space, not return space.
  Inputs:
  - secVol should be 1 SD of return vol per second, in *return* space
    (not in px space).
*/
double KFUtils::processInnovationVariance(double secTime, double secVol, double midPx) {
  if (secTime < 0.0) {
    secTime = 0.0;
  }
  return secTime * secVol * secVol * midPx * midPx;
}

bool KFUtils::simpleScalarKalmanFilter(double lastEst, double lastEstVar, 
				       double thisObs, double thisME, double thisPI,
				       double &thisEst, double &thisEstVar,
				       double &lastEstWeight, double &thisObsWeight) {
  if ((lastEstVar < 0.0) || (thisME < 0.0) || (thisPI < 0.0)) {
    return false;
  }

  lastEstVar = lastEstVar + thisPI;
  lastEstWeight = thisME/(thisME + lastEstVar);
  thisObsWeight = lastEstVar/(thisME + lastEstVar);
  thisEst = (lastEstWeight * lastEst) + (thisObsWeight * thisObs);
  thisEstVar = (lastEstWeight * lastEstWeight * lastEstVar) + (thisObsWeight * thisObsWeight * thisME);
  return true;
}


