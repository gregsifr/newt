#include "TNorm.h"
#include "LinearInterpolate.h"
// For namespace Cmp
#include "DataManager.h"

const double TNorm::_xValues[] = {-3.0, -2.5, -2.0, 
				  -1.5, -1.0, -0.5, 
				  0, 
				  0.5, 1.0, 1.5, 
				  2.0, 2.5, 3.0
};

const double TNorm::_yValues[] = {0.005, 0.017, 0.056,
				  0.138, 0.288, 0.508,
				  0.804,
				  1.139, 1.527, 1.945,
				  2.365, 2.809, 3.274
};

bool TNorm::computeLHSCutoff(double mean, double &fv) {
  if (Cmp::LT(mean, 0.0)) {
    // Cant decrease mean by cutting off part of LHS of distribution.
    return false;
  }
  // Entire distribution (aka no cutoff) has mean 0.
  if (Cmp::EQ(mean, 0.0)) {
    fv = -Inf;
    return true;
  }
  // yValues, xValues (below) is correct, although it looks non-intuitive.
  fv = LinearInterpolate::interpolate(_yValues, _xValues, 13, mean);
  return true;
}

bool TNorm::computeMean(double cutoff, double &fv) {
  fv = LinearInterpolate::interpolate(_xValues, _yValues, 13, cutoff);
  return true;
}
