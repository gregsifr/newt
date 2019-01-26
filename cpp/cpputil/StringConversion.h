/*
  Utility class for doing string <----> other type conversion, with obvious success/failure

*/

#ifndef __STRINGCONVERSION_H__
#define __STRINGCONVERSION_H__

#include <iostream>
#include <sstream>
#include <string>
using std::string;

class StringConversion {
 public:
  // Convert from string <input> to double.
  // If conversion could be accomplished error-free, then populate *d with
  //   converted value and return true.
  // Otherwise, populated *d with default value and return false;
  static bool stod(string input, double *d, double defaultValue);
  static bool stoi(string input, int *d, int dafaultValue);

  static void stripQuotes(string &s);
  static void stripSpaces(string &s);
};


#endif  // __STRINGCONVERSION_H__
