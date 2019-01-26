/*
  KFUtils.h  :
    C++ utility code for Kalman Filter & related (e.g. KF param estimate) calculation.
*/

class KFUtils {
 public:
  // Components of measurementError variance:
  // - 1 for rounding error (discreteness of prices).
  // - 1 for bid-ask bounce (observed prices tend to bounce around between 
  //     bid & ask even if "true" prices does not change).
  static double roundingErrorVariance(double px, double mpv);
  static double bidAskBounceErrorVariance(double bid, double ask, double mpv);
  static double shortTermJitterErrorVariance(double bid, double ask, double mpv);

  // Eatimate measurement error variance for a stock trade at specified price,
  // Assumes that the only source of meV for stock price is discreteness in trading prices.
  // Aka ignores other potential sources of meV such as demand for liquidity pushing
  //   stock price away from "fair" value. 
  // Should return VARIANCE (not SD) estimate.
  // Notes:
  // - error variance returned should be expressed in units of stock return, aka
  //   0.01 implies a 1% of stock px variance, *not* a 1 penny variance.
  static double measurementErrorVariance(double bid, double ask, double mpv);

  // Estimate process innovation variance for a stock with specified volatility
  //  (secVol, in units of 1 SD per second), over specified time 
  //  (secTime, in units of 1 second).
  // Assumes that only source of process innovation is stock px drift, and that
  //  said drift is continuous browniuan motion with specified constant volatility.
  // Should return VARIANCE (not SD) estimate.
  // Notes:
  // - pi variance returned should be expressed in units of stock return, aka
  //   0.01 implies a 1% return variance, not a 1 penny variance.
  static double processInnovationVariance(double secTime, double secVol, double midPx);

  // Apply simple scalar version of Kalman filter.
  // Assumes that:
  // - Quantity being estimated is scalar.
  // - Underlying value being estimated follows continuous brownian motion with
  //   0 drift and specified variance.
  static bool simpleScalarKalmanFilter(double lastEst, double lastEstVar, 
				       double thisObs, double thisME, double thisPI,
				       double &thisEst, double &thisEstVar,
				       double &lastEstWeight, double &thisObsWeight);
};
