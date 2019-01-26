/*
  BarraUtils.cc
  Some utility code for working with Bara data.
  Targeted areas of functionality:
  - Reading stock factor betas/exposures, from exernally generated file.
  - Reading risk/industry-factor weights, from externally generated file.
*/

#include "BarraUtils.h"

#include "PerStockParams.h"

#include "RUtils.h"

#include <cl-util/float_cmp.h>

/*
  Populate stock beta matrices (vs factors).
  Inputs:
  - Barra beta file, e.g. /apps/compustat/barra/data/RSK/USE3S0912.RSK
    - Not in original barra file format.
    - In original barra format.
  Notes:
  - Code assumes that Barra data file has particular format.
    In particular, assumes:   
    12 risk factors, in particular:
    - "VOLTILTY","MOMENTUM","SIZE    ","SIZENONL","TRADEACT","GROWTH  ","EARNYLD ","VALUE   ","EARNVAR ","LEVERAGE","CURRSEN ","YIELD"
    55 risk factors, with up to 5 risk factor weights per stock.
*/
bool BarraUtils::populateStockBetasFile(DataManager *dm, string &betasFile, 
					bmatrixd &factorBetas, bool checkWeights) {
  int s, f, nr;

  // Calculate number & type of factors.
  int numRiskFactors = 13;
  int numIndustryFactors = 55;
  int numIndustryWeights = 5;
  int F = numRiskFactors + numIndustryFactors;

  // Calculate number of stocks.
  int S = dm->cidsize(); 
  
  // Size factorBeta matrix accordingly.
  factorBetas.resize(S, F);

  // Walk through factorBetas matrix and zero out all elements.
  for (s=0;s<S;s++) {
    for (f=0;f<F;f++) {
      factorBetas(s, f) = 0.0;
    }
  }

  // Extract factor betas at specified indeces from file.
  // In current Barra RSK file format, tickers appear to be in field 2.
  PerStockParams p;
  string fs = ",";
  nr = PerStockParamsReader::readFile(dm, betasFile, fs, 1, p, 2);   
  if (nr <= 0) {
    std::cerr << "ERR: BarraUtils::populateStockBetasFile - Unable to read betasFile " << betasFile << std::endl;
    return false;
  }  

  // Risk factors:  "VOLTILTY","MOMENTUM","SIZE    ","SIZENONL","TRADEACT","GROWTH  ","EARNYLD ","VALUE   ","EARNVAR ","LEVERAGE","CURRSEN ","YIELD   ", "NONESTU"
  // Start at field 9 in file.
  vector<double> betas;
  dynamic_bitset<> found;
  for (int i=1;i<=numRiskFactors;i++) {

    // Populate exposures from specified column.
    nr = p.populateValues(dm, i+6, found, betas);
    if (nr != (int)dm->cidsize()) {
      std::cerr << "WARN: BarraUtils::populateStockBetasFile - reading barra betas file - risk factor " << i << ":";
      std::cerr << "  stocks in population set " << dm->cidsize() << " : stocks found in params file " << nr << std::endl;  
    }  

    // Check that index weights didn't get imbalanced, e.g. because the current stock population set is not the same as
    //    the population set used to generate the indec weights.
    double wsum = RVectorNumeric::sum(betas);
    if (!clite::util::cmp<4>::EQ(wsum, 0.0) || !clite::util::cmp<4>::EQ(wsum, 1.0)) {
      std::cerr << "WARN: BarraUtils::populateStockBetasFile - index " << i << " has unexpected weight " << wsum << std::endl;
      if (checkWeights == true) {
	return false;
      }
    }         

    // Populate index weights.
    for (s=0;s<S;s++) {
      if (found.test(s) == false) {
	continue;
      }
      factorBetas(s,i-1) = betas[s];
    }
  }

  // Industry Factors. 
  // These are encoded differently in the Barra files.
  // Instead of e.g. 12 factor exposures at a fixed offset, these have:
  // - "INDNAME1","IND1","WGT1%","INDNAME2","IND2","WGT2%","INDNAME3","IND3","WGT3%","INDNAME4","IND4","WGT4%","INDNAME5","IND5","WGT5%",
  // Starting at offset 22, and going to offset 36.
  // We ignore the INDNAME components, and exctract the IND (used as an index), and WGT (used as a beta) components.
  for (f=1;f<=numIndustryWeights;f++) {
    // Populate IND column.
    vector<int> inums;
    nr = p.populateValues(dm, 21 + 3 * (f-1), found, inums);
    if (nr != (int)dm->cidsize()) {
      std::cerr << "WARN: BarraUtils::populateStockBetasFile - reading barra industry factor " << f << ":";
      std::cerr << "  stocks in population set " << dm->cidsize() << " : stocks found in params file " << nr << std::endl;        
    }
   
    // Populate WGT column.
    vector<double> iweights;
    nr = p.populateValues(dm, 22 + 3 * (f-1), found, iweights);    

    // Use inums to assign iweights to appropriate indeces in factorBetas.
    for (s=0;s<S;s++) {
      if (!found.test(s)) {
	continue;
      }
      if (clite::util::cmp<6>::LE(iweights[s]/100.0, 0.0)) {
	continue;
      } 
      int ind = inums[s];
      int indIdx = numRiskFactors + ind - 1;
      factorBetas(s, indIdx) = iweights[s]/100.0;
    }
  }

  // Done.
  return true;
}

/*
  Populate index weights file for various Barra factors/synthetic-indeces.
  Assumes that some offlien process is used to:
  - Generate Barra synthetic index weights for a specified population set.
  - Dump these weights to a file in specified format.
  Format:
  - Header row with:
    - "Indeces" + index names 1...N.
  - Body rows with:
    - Ticker.
    - Weight of corresponding stock in index I in 1...N.

  Notes:
  - Need to make sure that, given the population set of stocks used, 
    the index weights add up to 0.0 (risk factors) or 1.0 (industry factors).
  - It would probably produce mildly bad results to produce the synthetic indeces
    using only a subset of the available population set of stocks.
  - It would probably produce quite bad results to produce synthetic indeces using stocks
    S1...Sn, but then run without using all of S1...Sn.
    - This could result in indeces that are unbalanced, aka that do not sum to
      the the right quantity (0.0 or 1.0), and thus are no-longer orthogonal to
      market returns.
    ==> Probably need to make sure that this does not happen.
*/
bool BarraUtils::populateIndexWeightsFile(DataManager *dm, string &weightsFile,
					  bmatrixd &indexWeights, bool checkWeights) {
  int s, f, nr;
  
  int S = dm->cidsize();
  int numRiskFactors = 13;
  int numIndustryFactors = 55;
  int F = numRiskFactors + numIndustryFactors;

  if (S < 1) {
    std::cerr << "BarraUtils::populateIndexWeightsFile - population set has 0 stocks" << std::endl;
    return false;
  }
  if (F < 1) {
    std::cerr << "BarraUtils::populateIndexWeightsFile - population set has 0 stocks" << std::endl;
    return false;
  }  
  // Size indexWeights to S * F matrix.
  indexWeights.resize(S, F);

  // Walk through indexWeights, and initialize all weights to 0.0.
  for (s=0;s<S;s++) {
    for (f=0;f<F;f++) {
      indexWeights(s, f) = 0.0;
    }
  }  

  // Read params file into key --> values map.
  PerStockParams p;
  string fs = ",";
  nr = PerStockParamsReader::readFile(dm, weightsFile, fs, 1, p);   
  if (nr <= 0) {
    std::cerr << "ERR: BarraUtils::populateIndexWeightsFile - Unable to read weightsFile " << weightsFile << std::endl;
    return false;
  }

  // Walk through specified indeces, and set indexWeights.
  dynamic_bitset<> found;
  vector<double> iweights;
  iweights.resize(S);
  for (f=1;f<=F;f++) {
    nr = p.populateValues(dm, f, found, iweights);
    // Check coverage of population set.
    if (nr != (int)dm->cidsize()) {
      std::cerr << "BarraUtils::populateIndexWeightsFile - reading barra risk factor " << f << ":";
      std::cerr << "  stocks in population set " << dm->cidsize() << " : stocks found in params file " << nr << std::endl;        
    }

    // Check that idx weights didnt get imbalanced by lack of coverage of population set.
    double wsum = RVectorNumeric::sum(iweights);
    if (!clite::util::cmp<1>::EQ(wsum, 0.0) || !clite::util::cmp<1>::EQ(wsum, 1.0)) {
      std::cerr << "WARN: BarraUtils::populateIndexWeightsFile - index " << f << " has unexpected weight " << wsum << std::endl;
      if (checkWeights == true) {
        return false;
      }
    }
    
    for (s=0;s<S;s++) {
      if (found.test(s) == false) {
	continue;
      }
      indexWeights(s,f-1) = iweights[s];
    }
  }

  // Done.
  return true;
}

/*
  Print matrix.
  Assumes that matrix has dimension S (# stocks) * F (# factors). 
*/
void BarraUtils::printFactorMatrix(DataManager *dm, string &fname, bmatrixd &matr) {
  int S = matr.size1();
  int F = matr.size2();

  string FS = " ";

  // Header info:
  std::cout << "---------------------------------------------------------------" << std::endl;
  std::cout << fname << std::endl;

  // Iterate through all stocks in population set.
  for (int s=0;s<S;s++) {
    // Print symbol.
    const char *sym = dm->symbol(s);
    std::cout << sym << FS;
    for (int f=0;f<F;f++) {
      double val = matr(s,f);
      std::cout << val << FS;
    }
    std::cout << std::endl; 
  }
}
