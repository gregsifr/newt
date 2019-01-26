#include "SimpleAwkParser.h"

#include <errno.h>

#include <iostream>
#include <vector>
#include <string>
using namespace std;

#include <boost/regex.hpp>


SimpleAwkParser::SimpleAwkParser() :
  AwkParser(),
  file(),
  num_line(0),
  num_fields(0),
  line(""),
  fields(0, ""),
  fs("[ \t]+"),
  re(fs),
  compiled(true)
{

}

SimpleAwkParser::~SimpleAwkParser() {
    closeF();
    compiled = false;
}

// open file
bool SimpleAwkParser::openF(const string &fileName) {
    // fail if a file is already opened
    if (file.is_open()) {
        cerr << "SimpleAwkParser::openF " << fileName << " called with (data member) file already open" << endl;
        return false;
    }

    file.open(fileName.c_str());
    // fail if file did not open
    if (!file.is_open()) {
        cerr << "SimpleAwkParser::openF " << fileName << " error opening file : " << strerror(errno) << endl;
        return false;
    }
    return true;
}

// close file
bool SimpleAwkParser::closeF() {
    if (!file.is_open())
        return false;
        
    file.close();
    fields.clear();
    line.clear();
    num_fields = 0;
    num_line = 0;
   
    return true;
}

// parse a line
bool SimpleAwkParser::getLine() {
    fields.clear();
    num_fields = 0;

    getline(file, line);
    if (!file.good())
        return false;
    num_line++;

    if (compiled == false)
        return false;

    // tokenize
    boost::sregex_token_iterator i(line.begin(), line.end(), re, -1);
    boost::sregex_token_iterator j;
    while (i != j) {
      string token = *i;
      ++i;
      //cout << token << endl;    
      fields.push_back(token);
    }
    num_fields = fields.size();
    return true;
}

string SimpleAwkParser::getField(int fieldNum) {
    // fail if file is not open, line is not tokenized, or invalid field number
    if (!file.is_open() || num_line == 0 || fieldNum > num_fields)
        return "";
    if (fieldNum == 0)
        return line;
    return fields[fieldNum-1];
}

void SimpleAwkParser::setFS(const string &nFS) {
    fs = nFS;
    re = fs.c_str();
}
