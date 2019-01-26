#ifndef __TNORM_H__
#define __TNORM_H__

class TNorm {
  // Pre-computed x and y values, linearly interpolated for use in
  //   lhsCutoff.
  const static double _xValues[];
  const static double _yValues[];

 public:
  /*
    Compute, for a unit-normal distribution, the LHS cutoff point such that the 
      expected value of the remaining distribution has mean <mean>.
    Aka:  Compute a such that
      E(X | a < X) = mean, 
      for a (pre-truncation) unit normal (mean 0, sd 1) distribution
    Current version is very simple based on linear-interpolation against
      lookup table with pre-computed values.  May want to replace with
      more precise algorithm.
  */
  static bool computeLHSCutoff(double mean, double &fv);

  /*
    Compute, for a truncated unit normal distribution with specified
      LHS cutoff, the new mean.
  */
  static bool computeMean(double cutoff, double &fv);
};


#endif  // __TNORM_H__
