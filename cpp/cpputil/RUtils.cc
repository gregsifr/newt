#include "RUtils.h"
#include <assert.h>

#include <iostream>
using std::iostream;

#include <ostream> 
using std::ostream; 

void RUtils::stop(const string &errorString) {
  std::cerr << errorString;
  assert(false);
}



