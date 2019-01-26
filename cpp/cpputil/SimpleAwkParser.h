

#ifndef __SIMPLEAWKPARSER_H__
#define __SIMPLEAWKPARSER_H__

#include "AwkParser.h"
#include <fstream>
#include <vector>
#include <string>
#include <boost/regex.hpp>

using std::ifstream;
using std::vector;

/*
  Simple implementation of an AwkParser.
  Limited/no performance optimziations....
  Interface described in abstract AwkParser.h
*/
class SimpleAwkParser : public AwkParser {
    public:
        SimpleAwkParser();
        virtual ~SimpleAwkParser();
        virtual bool openF(const string &fileName);
        virtual bool closeF();
        virtual bool getLine();
        virtual int NR() { return num_line; }
        virtual int NF() { return num_fields; }
        virtual string getField(int fieldNum);
        virtual void setFS(const string &nFS);

    private:
        ifstream file;
        int num_line;
        int num_fields;
        string line;
        vector<string> fields;
	string fs;
        boost::regex re;
	bool compiled;
};



#endif  // __SIMPLEAWKPARSER_H__
