/*
  BarraUtils.cc
  Some utility code for working with Bara data.
  Targeted areas of functionality:
  - Reading stock factor betas/exposures, from exernally generated file.
  - Reading risk/industry-factor weights, from externally generated file.
*/

#ifndef __BARRAUTILS_H__
#define __BARRAUTILS_H__


#include "DataManager.h"

#include <string>
using std::string;

#include <vector>
using std::vector;


// boost matrix.
#include <boost/numeric/ublas/matrix.hpp>

typedef boost::numeric::ublas::matrix<double> bmatrixd;

class BarraUtils {
 public:
  static bool populateStockBetasFile(DataManager *dm, string &betasFile, 
				     bmatrixd &factorBetas, bool checkWeights);
  static bool populateIndexWeightsFile(DataManager *dm, string &weightsFile,
				       bmatrixd &indexWeights, bool checkWeights);
  static void printFactorMatrix(DataManager *dm, string &fname, bmatrixd &matr);
};


#endif   // __BARRAUTILS_H__
