/*
  Utility class for doing string <----> other type conversion, with symtax that
  makes sense to me.
*/

#include "StringConversion.h"

#include <iostream>
#include <sstream>
#include <string>
using std::string;
#include <sstream>

bool StringConversion::stod(string input, double *d, double defaultValue) {
  std::stringstream i(input);
    if (!(i >> (*d))) {
    (*d) = defaultValue;
    return false;
  }
  return true;
}

bool StringConversion::stoi(string input, int *d, int defaultValue) {
  std::stringstream i(input);
  if (!(i >> (*d))) {
    *d = defaultValue;
    return false;
  }
  return true;
}

void StringConversion::stripQuotes(string &s) {
  size_t firstLoc;
  while ((firstLoc = s.find_first_of("\"")) != string::npos) {
    s.erase(firstLoc, 1);
  }
}

void StringConversion::stripSpaces(string &ticker) {
  size_t startpos = ticker.find_first_not_of(" "); // Find the first character position after excluding leading blank spaces 
  if (startpos != string::npos) {
    ticker = ticker.substr(startpos);
  }
  size_t lastpos = ticker.find_last_not_of(" "); // Find last non-space character.
  if (lastpos != string::npos) {
    ticker = ticker.substr(0, lastpos+1);
  }
}
