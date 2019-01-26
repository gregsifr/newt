#include <string>

using std::string;

/*

    AwkParser.  C++ utility that provide awk-like functionality for parsing
      structured text files.
    Notes:
    - To clear up some naming confusion, not a parser for the AWK programming
      language.

*/

#ifndef __AWKPARSER_H__
#define __AWKPARSER_H__


/*
  Interface for an Awk Parser.  Designed to provide C++ functionality for AWK like
  parsing of structured text files.

  Gestalt:
  - Each AWK parser instance has 0 or 1 open file with which it is associated.
    (via openF function).
  - When the AwkParser is associated with a file, it initially is not associated with any line
    of the file.
  - Each call to getLine() advances the current line (being parsed) in the file by 1
    (starting from 0, aka not associated with a line).
  - The parser object automatically breaks the current line up into fields based on a 
    specified separator (regexp or simple string?).  It provides functionality for:
    - Number of fields found.
    - Getting each field (as a string).
    - Getting the entire line (as a string).
    - Converting from string to other data types.
  - The parser design/interface does not currently provide functionality for:
    - Looking back in the file (e.g. getting previous lines).
    - Quoting (except using quote character as a FS).
    - Comments/escape characters.
  - The AwkParser class is essentially an interface specification.
  - The SimpleAwkParser is a simple implementation of that interface that doesn't
    worry much about performance optimization....
*/
class AwkParser {

 public:
  AwkParser() {};
  virtual ~AwkParser() {};
  /*
    Notes:
    - fileName should include path info to reach file.
    - Opens file in read-only mode.
    - Returns TRUE iff file successfully opened for reading.
    - Returns FALSE if this AwkParser instance already has an open file.
  */
  virtual bool openF(const string &fileName) = 0;

  /*
    Notes:
    - Close current file.
    - Returns FALSE if no currently associated file.
  */
  virtual bool closeF() = 0;


  /*
    Advance to next line in file.  
    - 1st call of this for a particular file should result in file 1st line being parsed.
    - Sets NF, NR, and results of getField to correspond to tokenization of current line.
    - Returns false if another line could not be read.
      - This typically corresponds to EOF, but might conceptually correspond to
        other cases.  Would it be a good idea to put some internal error state for
	querying?
  */
  virtual bool getLine() = 0;

  /*
    # of current line being parsed.  
    - 0 before 1st call to getLine().
    - 1 on 1st line of file, ....
    - 0 after unsuccessful call to getLine(), or file is closed.
  */
  virtual int NR() = 0;

  /*
    # of fields found/tokenized on current line.
    - 0 before 1st call to getLine.
    - 0 after unsuccessful call to getLine(), or after file is closed.
  */
  virtual int NF() = 0;

  /*
    Get field <fieldNum> of current line.
    Fields are numbered 1....NF, aka:
      the 1st valid field/token is fieldNum 1,
      the 2nd is fieldNum 2, ....
    Field 0 is a special case which corresponds to the entire raw (untokenized) 
      current line.
    Returns empty string "" on error conditions, including:
    - AwkParser not associated with any file, or not associated with 
      line in file.
    - fieldNum > NF. 
  */
  virtual string getField(int fieldNum) = 0;

  /*
    Set the current FS (Field Separator).  This is the separator used to tokenize
    the records in the file.
    What is the default FS?  Probably should be regexp corresponding to white space
     (FILL IN)?
    Semantically, calls to setFS take effects starting with the next call to getLine().
    Thus, for example:
    - calls to setFS before the 1st call to getLine affect processing of line 1.
    - call getLine() 5x (now on line 5), then call setFS.  New FS applies to processing of
      lines 6.... (until end of processing, or new FS specified).
    These semantics should be preserved even where the AwkParser does e.g. batch fetching
    and tokenization of multiple lines at once.  Aka, such an AwkParser may need to 
    re-tokenize already tokenized lines on calls to setFS()....
  */
  virtual void setFS(const string &nFS) = 0;
};


#endif  //  __AWKPARSER_H__
