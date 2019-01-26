/*
  Simple tester progam for RUtils & RVectorUtils classes
*/


#include "RUtils.h"

#include <iostream>
using std::iostream;

#include <ostream>
using std::ostream; 


//extern ostream cout;

int main(int argc, char **argv) {

  std::cout << "entering RUtils_test/main\n";
  std::cout.flush();
  vector<float> testV(10);
  for (int i = i;i<10;i++) {
    testV[i] = (float) i;
  }
  std::cout << "  done constructing vector testV\n";
  std::cout.flush();
 
  //cout << testV;
  std::cout << "  about to call RVector::print(testV)\n";
  std::cout.flush();
  RVector::print(testV, std::cout);
  std::cout << "  returned from RVector::print(testV)\n";
  std::cout.flush();

  vector<bool> ge10;
  std::cout << "  about to call RVectorNumeric::GT(testV)\n";
  std::cout.flush();
  RVectorNumeric::GT(testV, (float)10.0, ge10);
  std::cout << "  returned from RVectorNumeric::GT(testV)\n";
  std::cout.flush();
  
  //count << ge10; 
  std::cout << "  about to call RVector::print(ge10)\n";
  std::cout.flush();
  RVector::print(ge10, std::cout);
  std::cout << "  returned from RVector::print(testV)\n";
  std::cout.flush();
}
