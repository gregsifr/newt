/*
  Class that provides some R-language like functionality for use in
  C++.   

  Copyright Matt Cheyney, 2008.
*/

#ifndef __RUTILS_H__
#define __RUTILS_H__

#include <string>
using std::string;

#include <vector>
using std::vector;

#include <map>
using std::map;

#include <ostream>
using std::ostream;

#include "math.h"

extern ostream cout;

/*
  Functions that duplicate R functions for accessing/modifying
  R's runtime environment.
*/
class RUtils {
 public:
  // dump error string to stderr & halt.
  static void stop(const string &errorString);
};


/*
  Templated class for some R-like vector utilities.
  RVector has generic functions that apply for both
    numeric and non-numeric vectors.
  RVectorNumeric has functions that apply for only
    numeric types.
*/
class RVector {
 public:


  /*
    Vector boolean operations.
    We represent boolean vectors with vectors of chars due to 
    the unpredictable nature of vector<bool> (is it a vector of
    bits or individually accessible objects, and what happens
    when you try to take use e.g., the [] operator????
    See:
    www.informit.com/guides/content.aspx?g=cplusplus&seqNum=98 - 32k -
  */


  static const int K_BOOLEAN_OPERATION_AND = 1;
  static const int K_BOOLEAN_OPERATION_OR = 2;

  static const char kcFalse = 0;
  static const char kcTrue = 1;
  
  // scalar boolean operations....
  static char scalarBooleanOperation(const char v1, const char v2, 
				     int operation) {
    char tmp;
    switch (operation) {
    case K_BOOLEAN_OPERATION_AND:
      tmp = ((v1 == kcTrue) && (v2 == kcTrue)? true : false);
      break;
    case K_BOOLEAN_OPERATION_OR:
      tmp = ((v1 == kcTrue) || (v2 == kcTrue) ? true : false);
      break; 
    default:
      assert(false);
    }
    return tmp;
  }

  /*
    Boolean operations on vectors (or booleans), a la R.
    - Unlike R, assumes that both vectors have same length.
      Does not automatically pad shortest out to longest....
  */
  static vector<char> vectorBooleanOperation(const vector<char> &a,
					     const vector<char> &b,
					     int operation) {
    char aelem, belem;
    int alen = a.size();
    int blen = b.size();
    assert(alen == blen);
    vector<char> ret(alen);
    for (int i=0;i<alen;i++) {
      aelem = a[i];
      belem = b[i];
      ret[i] = scalarBooleanOperation(aelem, belem, operation);
    }
    return ret;
  }

  static vector<char> vectorAnd(const vector<char> &a, const vector<char> &b) {
    return vectorBooleanOperation(a, b, K_BOOLEAN_OPERATION_AND);
  }

  static vector<char> vectorAnd3(const vector<char> &a, const vector<char> &b,
				 const vector<char> &c) {
    vector<char> tmp = vectorAnd(a, b);
    vector<char> ret = vectorAnd(tmp, c);
    return ret;
  }

  static vector<char> vectorOr(const vector<char> &a, const vector<char> &b) {
    return vectorBooleanOperation(a, b, K_BOOLEAN_OPERATION_OR);
  }

  static vector<char> vectorOr3(const vector<char> &a, const vector<char> &b,
				const vector<char> &c) {
    vector<char> tmp = vectorOr(a, b);
    vector<char> ret = vectorOr(tmp, c);
    return ret;
  }

  /*  
      Set operations  
   */
  //  Union.
  template <class vtt> static vector<vtt> setUnion(const vector<vtt> &idxs1, 
					 const vector<vtt> &idxs2) {
    vector<vtt> combined = cat(idxs1, idxs2);
    vector<vtt> uniqued = unique(combined);
    return uniqued;
  }

  template <class vtt> static vector<vtt> setUnion3(const vector<vtt> &idxs1, 
					 const vector<vtt> &idxs2,
					 const vector<vtt> &idxs3) {
    vector<vtt> combined = cat3(idxs1, idxs2, idxs3);
    vector<vtt> uniqued = unique(combined);
    return uniqued;   
  }

  // Vector-as-list concatenation, a la Lisp.
  template <class vtt> static vector<vtt> cat(const vector<vtt> &p1,
					      const vector<vtt> &p2) {
    int i;
    int s1 = p1.size();
    int s2 = p2.size();
    vector<vtt> ret;
    for (i=0;i<s1;i++) {
      ret.push_back(p1[i]);
    }
    for (i=0;i<s2;i++) {
      ret.push_back(p2[i]);
    }
    return ret;
  }

  // Convenience function - cat 3 vectors/lists together
  template <class vtt> static vector<vtt> cat3(const vector<vtt> &p1,
					      const vector<vtt> &p2,
					      const vector<vtt> &p3) {
    int i;
    int s1 = p1.size();
    int s2 = p2.size();
    int s3 = p3.size();
    vector<vtt> ret;
    for (i=0;i<s1;i++) {
      ret.push_back(p1[i]);
    }
    for (i=0;i<s2;i++) {
      ret.push_back(p2[i]);
    }
    for (i=0;i<s3;i++) {
      ret.push_back(p3[i]);
    }    
    return ret;
  }


  // Set Intersection.  Return elements in a that are also in b.
  // - If a has duplicate members, only 1 instance is returned.
  // - Defined as empty set if a or b are empty....
  // - Order of return vector not defined....
  template <class vtt> static vector<vtt> setIntersection(const vector<vtt> &a, 
						   const vector<vtt> &b) {
    vector<vtt> ret;
    //  Are either of inputs sete the empty set?
    int alen = a.size();
    int blen = b.size();
    if (alen == 0 || blen == 0) {
      return ret;
    }
    // Make map out of elements in a.  Should map from vtt -> int, with
    //   the int specifying the number of times the element was found
    //   (-1).  
    // Then run through elements in b and, for each such element:
    //   - Find the element in aMap with the same key.
    //   - Increment its value by 1.
    // Then run through aMpa and make a vector with all the elements with
    //   value >= 0.
    map<vtt, vtt> aMap = vectorToMap(a);    
    incrementMapInstances(aMap, b);
    ret = convertTouchedInstancesToVector(aMap, 1);
    return ret;
  }

  /*
    Convenience function:  Intersection of 3 sets....
  */
  template <class vtt> static vector<vtt> setIntersection3(const vector<vtt> &a, 
							   const vector<vtt> &b,
							   const vector<vtt> &c) {
    vector<vtt> i1 = setIntersection(a, b);
    vector<vtt> i2 = setIntersection(i1, c);
    return i2;
  }

  // Set Difference.  Return elements in a but not in b.  If a has duplicate
  //  members, only 1 copy of dups will be returned.
  template <class vtt> static vector<vtt> setDifference(const vector<vtt> &a, 
							const vector<vtt> &b) {
    vector<vtt> ret = a;
    int alen = a.size();
    int blen = b.size();
    if (alen == 0 || blen == 0) {
      return ret;
    }
    map<vtt, vtt> aMap = vectorToMap(a);
    for (int i=0;i<blen;i++) {
      aMap.erase(b[i]);
    }
    return mapToVector(aMap);
  }

  // Convert v into a Map.  Elements of v are converted into key, value pairs
  //   (with value set to 0).
  template <class vtt> static map<vtt, int> vectorToMap(const vector<vtt> &v) {
    map<vtt, int> ret;
    int vlen = v.size();
    for (int i=0;i<vlen;i++) {
      vtt tmp = v[i];
      ret[tmp] = (int)0;
    }
    return ret;
  }

  // Extract *values* in m and return in vector.
  // Order of resulting vector is not defined.
  template <class vtt> static vector<vtt> mapToVector(const map<vtt, int> &m) {
    vector<vtt> ret;
    typename map<vtt, int>::const_iterator ii;
    //map<vtt, int>::iterator ii;
    for(ii=m.begin(); ii!=m.end(); ii++)
      {
	vtt value = (*ii).first;
	ret.push_back(value);
      }
    return ret;
  }


  //
  //  Walk though vector v.  For each element in v:
  //  - Find the corresponding element (same key) in m.
  //  - Increment element->value.
  template <class vtt> static void incrementMapInstances(const map<vtt, int> &m, const vector<vtt> &v) {
    assert(false);
    /*
    vtt val;
    typename map<vtt, int>::const_iterator it;
    int vlen = v.size();
    for (int i=0;i<vlen;i++) {
      val = v[i];
      it = m.find(val);
      if (it != m.end()) {
	// Is val in map?  if so, increment the int part of the 2-ple.....
	it->second++;
      }
    }
    */
  } 

  //
  //  Walk through a map<vtt, int> m.  Make a vector containing all keys in m
  //    which have a value >= threshold....
  //  Notes:
  //  - Order of return vector not defined....
  template <class vtt> static vector<vtt> convertTouchedInstancesToVector(const map<vtt, int> &m, int threshold) {
    vector<vtt> ret;
    vtt val;
    int key;
    typename map<vtt, int>::const_iterator it;
    //map<vtt, int>::iterator it;
    for(it = m.begin();it != m.end();++it) {
      val = (*it).first;
      key = (*it).second;
      if (key >= threshold) {
	ret.push_back(val);
      } 
    }
    return ret;
  }

  // Sort <sortMe> into <into>.
  template <class vtt> static vector<vtt> sort(const vector<vtt> &sortMe) {
    vector<vtt> ret = sortMe;
    std::sort(ret.begin(), ret.end());
    return ret;
  }
 
  // Extract unique elements of <uniqMe> into <into>.
  // Returns sorted version of <uniqMe> with no duplicates.
  template <class vtt> static vector<vtt> unique(const vector<vtt> &uniqMe) {
    vector<vtt> sorted = sort(uniqMe);
    vector<vtt> ret;
    int len = sorted.size();
    vtt lastE, thisE;
    for (int i=0;i<len;i++) {
      thisE = sorted[i]; 
      if (i == 0) {
	ret.push_back(thisE);
      } else if (thisE != lastE) {
	ret.push_back(thisE);
      }
      lastE = thisE;
    }
    return ret;
  }


  //
  // Make new vector holding subset of <v> from <begin> to <end>, inclusive.
  //
  template <class vtt> static vector<vtt> subset(const vector<vtt> &v, int begin, int end) {
    vector<vtt> ret;
    for (int i=begin;i<=end;i++) {
      ret.push_back(v[i]);
    }
    return ret;
  }

  //
  // Make new vector holding subset of <v> with values where <lv> is true....
  //
  template <class vtt> static vector<vtt> subset(const vector<vtt> &v, const vector<char> &lv) {
    vector<vtt> ret;
    unsigned int len = v.size();
    for (unsigned int i=0;i<len;i++) {
      if (lv[i] == true) {
	ret.push_back(v[i]);
      }
    }
    return ret;
  }

  /*
    Human-readible IO.  Are there built-in primitives for doing 
    something like this in C++??
  */
  template <class vtt> static void print(const vector<vtt> &printMe, 
					 ostream &os) {
    int len = printMe.size();
    for (int i =0;i<len;i++) {
      os << printMe[i] << "  ";
    }
    os << std::endl;
  }

  /*
    Fill <fillMe> with indeces at which logicalValues == TRUE. 
  */
  static vector<int> which(const vector<char> &logicalValues) {
    int len = logicalValues.size();
    vector<int> ret(0);
    for (int i=0;i<len;i++) {
      char b = logicalValues[i];
      if (b == true) {
	ret.push_back(i);
      }
    }
    return ret;
  }

  /*
    Return the index of the 1st TRUE element in logicalValues, or -1 if
    no such value is found....
  */
  static int first(const vector<char> &logicalValues) {
    int len = logicalValues.size();
    for (int i=0;i<len;i++) {
      char b = logicalValues[i];
      if (b == true) return i;
    }
    return -1;
  }

  /*
    How many entries in logical values are true?
  */
  static int sum(const vector<char> &logicalValues) {
    int len = logicalValues.size();
    int ret = 0;
    for (int i=0;i<len;i++) {
      char b = logicalValues[i];
      if (b == true) ret++;
    }
    return ret;
  }

  /*
    Are any entries in <logicalValuies> true?
  */
  static char any(const vector<char> &logicalValues) {
    int len = logicalValues.size();
    for (int i=0;i<len;i++) {
      char b = logicalValues[i];
      if (b == true) return true;
    }
    return false;
  }

  // Return new vector with all integers in range a...b, inclusive.
  static vector<int> range(const int &a, const int &b) {
    if (b >= a) {
      return rangeAscending(a, b);
    }
    return rangeAscending(b, a);
  }

  // As range, but caller asserts that b >= a
  static vector<int> rangeAscending(const int &a, const int &b) {
    assert(b >= a);
    vector<int> ret;
    for (int t = a;t<=b;t++) {
      ret.push_back(t);
    }
    return ret;
  }

  // Fill elements 1...(len) with value val.
  template <class vtt> static void fill(vector<vtt> &fillMe, 
					uint len,
					const vtt &val) {
    for (uint i=0;i<len;i++) {
      fillMe[i] = val;
    }
  }  

  // Fill all elements in 1... current size() with value val.
  template <class vtt> static void fill(vector<vtt> &fillMe, 
					  const vtt &val) {
    uint len = fillMe.size();
    for (uint i=0;i<len;i++) {
      fillMe[i] = val;
    }    
  }

  template <class vtt> static void reallyFuckingResize(vector<vtt> &foo, 
						       int tgtSize,
						       const vtt defaultValue) {
    int sz = foo.size();
    if (sz > tgtSize) {
      foo.resize(tgtSize);
      return;
    }
    int numExtra = tgtSize - sz;
    for (int i=0;i<numExtra;i++) {
      foo.push_back(defaultValue);
    }
  }

  /*
    Copy src -> dest, possibly using default type conversion on elements....
  */
  template <class vtt1, class vtt2> static void copy(const vector<vtt1> &src,
						           vector<vtt2> &dest) {
    int slen = src.size();
    dest.empty();
    for (int i=0;i<slen;i++) {
      vtt1 valAsT1 = src[i];
      vtt2 valAsT2 = (vtt2)valAsT1;
      dest.push_back(valAsT2);
    }
  }
						       
};  // RVector


class RVectorNumeric : RVector {
 public:
  /* 
     Simple versions of R-like ifelse functionality.  To date, have not 
     implemented full R-like functionality allowing specification of 
     arbitrary expression for evaluation.  Instead, specifying small 
     set of functions with specific hard-coded logic that are necessary
     for current projects.
  */
  template <class vtt> static void replaceInfinite(vector<vtt> &dataV, 
						   const vtt &repValue) {
    int len = dataV.size();
    for (int i = 0;i<len;i++) {
      vtt val = dataV[i];
      if (isnan(val)) {
	dataV[i] = repValue;
      }
    }    
  }

  template <class vtt> static void replaceInfinite(vector<vtt> &dataV, 
						   const vector<char> &logicV, 
						   const vtt &repValue) {
    int len = dataV.size();
    for (int i = 0;i<len;i++) {
      if (logicV[i] == true) {
	dataV[i] = repValue;
      }
    }
  }

  // Specify which elements of dataV are infinite.  fillMe is passed
  //  as a parameter rather than constructed internally & returned to
  //  avoid potentially expensive copy operation....
  template <class vtt> static vector<char> isInfinite(const vector<vtt> &dataV) {
    int len = dataV.size();
    vector<char> ret;
    for (int i = 0;i<len;i++) {
      ret.push_back(isnan(dataV[i]));
    }
    return ret;
  }

  template <class vtt> static bool isInfinite(const vtt&v) {
    return isnan(v);
  }


  /*
    Strict >=, >, <=, < comparison operators for vectors.
    3 basic versions are provided:
    - Version which compares vector v1 against a single thresholdValue.
    - Version which compares vector v1 against another vector v2, assuming 
      that v1 and v2 are the same size, and
    - Version which compares vector v1 against another vector v1, which is 
      trimmed/promoted to be the same size as v1.
    Conceptually, can implement 1 & 2 using 3, but code below avoids doing
      so for efficiency reasons.
    Also, all versions are coded so as to avoid potentially expensive 
      copy operation for return value, at the cost of making their use
      syntax a big uglier.  E.g., you use:
        vector<bool> gtValues; 
        GT(v1, v2, gtValues);
      rather than:
        gtValues = GT(v1, v2);  OR
	gtValues = v1 > v2;
	
  */

  static const int K_COMPARISON_GT = 1;
  static const int K_COMPARISON_GE = 2;
  static const int K_COMPARISON_LT = 3;
  static const int K_COMPARISON_LE = 4;
  static const int K_COMPARISON_EQ = 5;
  static const int K_COMPARISON_NE = 6;

  template <class vtt> static bool scalarComparison(const vtt &v1, 
			       const vtt &v2, int operation) {
    bool tmp;
    switch (operation) {
    case K_COMPARISON_GT:
      tmp = (v1 > v2 ? true: false);
      break;
    case K_COMPARISON_GE:
      tmp = (v1 >= v2 ? true: false);
      break;
    case K_COMPARISON_LT:
      tmp = (v1 < v2 ? true: false);
      break;
    case K_COMPARISON_LE:
      tmp = (v1 <= v2 ? true: false);
      break;
    case K_COMPARISON_EQ:
      tmp = (v1 == v2 ? true: false);
      break;
    case K_COMPARISON_NE:
      tmp = (v1 != v2 ? true: false);
      break;
    default:
      assert(false);
    }
    return tmp;
  }

  template <class vtt> static bool fpScalarComparison(const vtt &v1, 
						      const vtt &v2, 
						      const vtt &epsilon, 
						      int operation) {
    bool tmp;
    switch (operation) {
    case K_COMPARISON_GT:
      tmp = fp_gt(v1, v2, epsilon);
      break;
    case K_COMPARISON_GE:
      tmp = fp_ge(v1, v2, epsilon);
      break;
    case K_COMPARISON_LT:
      tmp = fp_lt(v1, v2, epsilon);
      break;
    case K_COMPARISON_LE:
      tmp = fp_le(v1, v2, epsilon);
      break;
    case K_COMPARISON_EQ:
      tmp = fp_equal(v1, v2, epsilon);
      break;
    case K_COMPARISON_NE:
      tmp = fp_equal(v1, v2, epsilon);
      tmp = !tmp;
      break;
    }
    return tmp;
  }

  template <class vtt> static vector<char> vectorComparison(const vector<vtt> &dataV1,
							    const vector<vtt> &dataV2,
							    int operation) {
    int len = dataV1.size();
    assert(dataV1.size() == dataV2.size());
    vector<char> ret;
    for (int i=0;i<len;i++) {
      char tmp = scalarComparison(dataV1[i], dataV2[i], operation);
      ret.push_back(tmp);
    }
    return ret;
  }

  template <class vtt> static vector<char> fpVectorComparison(const vector<vtt> &dataV1,
							      const vector<vtt> &dataV2,
							      const vtt &epsilon,
							      int operation) {
    int len = dataV1.size();
    assert(dataV1.size() == dataV2.size());
    vector<char> ret;
    for (int i=0;i<len;i++) {
      char tmp = fpScalarComparison(dataV1[i], dataV2[i], epsilon, operation);
      ret.push_back(tmp);
    }
    return ret;    
  }

  template <class vtt> static vector<char> vectorToScalarComparison(const vector<vtt> &dataV1,
								      const vtt &scalar,
								      int operation) {
    int len = dataV1.size();
    vector<char> ret;
    for (int i=0;i<len;i++) {
      char tmp = scalarComparison(dataV1[i], scalar, operation);
      ret.push_back(tmp);
    }
    return ret;    
  }

  template <class vtt> static vector<char> fpVectorToScalarComparison(const vector<vtt> &dataV1,
								      const vtt &scalar,
								      const vtt &epsilon,
								      int operation) {
    int len = dataV1.size();
    vector<char> ret;
    for (int i=0;i<len;i++) {
      char tmp = fpScalarComparison(dataV1[i], scalar, epsilon, operation);
      ret.push_back(tmp);
    }
    return ret;    
  }

  template <class vtt> static vector<char> GT(const vector<vtt> &dataV, 
				      const vtt &threshValue) {
    return vectorToScalarComparison(dataV, threshValue, K_COMPARISON_GT);
  }

  template <class vtt> static vector<char> GE(const vector<vtt> &dataV, 
				      const vtt &threshValue) {
    return vectorToScalarComparison(dataV, threshValue, K_COMPARISON_GE);
  }

  template <class vtt> static vector<char> LT(const vector<vtt> &dataV, 
				      const vtt &threshValue) {
    return vectorToScalarComparison(dataV, threshValue, K_COMPARISON_LT);
  }  

  template <class vtt> static vector<char> LE(const vector<vtt> &dataV, 
				      const vtt &threshValue) {
    return vectorToScalarComparison(dataV, threshValue, K_COMPARISON_LE);
  }

  template <class vtt> static vector<char> EQ(const vector<vtt> &dataV, 
				      const vtt &threshValue) {
    return vectorToScalarComparison(dataV, threshValue, K_COMPARISON_EQ);
  }

  template <class vtt> static vector<char> NE(const vector<vtt> &dataV, 
				      const vtt &threshValue) {
    return vectorToScalarComparison(dataV, threshValue, K_COMPARISON_NE);
  }

  /*
    Return vector of indeces of elements of <dataV> for which various comparison
    oeprations return TRUE, e.g.:
    - which elements of <dataV> are equal to threshValue?
  */
  template <class vtt> static vector<int> whichEQ(const vector<vtt> &dataV, 
						  const vtt &threshValue) {
    vector<char> isEQ = EQ(dataV, threshValue);
    vector<int> ret = which(isEQ);
    return ret;
  }



  /*
    Add strict >=, >, <=, < comparison operators, against vector target.
    For convenience, it is assumed that dataV2 is at least as long as dataV2,
    and so no effort is made to "promote" dataV2 to that minimum length.
    Here, SS means "same size", as in dataV1 and dataV2 are assumed to have
    the same size before calling....
  */
  template <class vtt> static vector<char> GT_SS(const vector<vtt> &dataV1, 
					 const vector<vtt> &dataV2) {
    return vectorComparison(dataV1, dataV2, K_COMPARISON_GT);
  }

  template <class vtt> static vector<char> GE_SS(const vector<vtt> &dataV1, 
					 const vector<vtt> &dataV2) {
    return vectorComparison(dataV1, dataV2, K_COMPARISON_GE);
  }

  template <class vtt> static vector<char> LT_SS(const vector<vtt> &dataV1, 
					 const vector<vtt> &dataV2) {
    return vectorComparison(dataV1, dataV2, K_COMPARISON_LT);
  }

  template <class vtt> static vector<char> LE_SS(const vector<vtt> &dataV1, 
					 const vector<vtt> &dataV2) {
    return vectorComparison(dataV1, dataV2, K_COMPARISON_LE);
  }



  /*
    Finite precision >=, >, <=, < operators.
    These allow fuzzy comparison within some error range.
    Coded so as to avoid potentially expensive copy operation.
  */
  template <class vtt> static vector<char> FP_GE(const vector<vtt> &dataV,
					 const vtt &threshValue, 
					 const vtt&epsilon) {
    vector<char> ret = fpVectorToScalarComparison(dataV, threshValue, epsilon, K_COMPARISON_GE);
    return ret;
  }

  template <class vtt> static vector<char> FP_GT(const vector<vtt> &dataV,
					 const vtt &threshValue, 
					 const vtt&epsilon) {
    vector<char> ret = fpVectorToScalarComparison(dataV, threshValue, epsilon, K_COMPARISON_GT);
    return ret;
  }

  template <class vtt> static vector<char> FP_LE(const vector<vtt> &dataV,
					 const vtt &threshValue, 
					 const vtt&epsilon) {
    vector<char> ret = fpVectorToScalarComparison(dataV, threshValue, epsilon, K_COMPARISON_LE);
    return ret;
  }

  template <class vtt> static vector<char> FP_LT(const vector<vtt> &dataV,
					 const vtt &threshValue, 
					 const vtt&epsilon) {
    vector<char> ret = fpVectorToScalarComparison(dataV, threshValue, epsilon, K_COMPARISON_LT);
    return ret;
  }

  template <class vtt> static vector<char> FP_EQ(const vector<vtt> &dataV,
					 const vtt &threshValue, 
					 const vtt&epsilon) {
    vector<char> ret = fpVectorToScalarComparison(dataV, threshValue, epsilon, K_COMPARISON_EQ);
    return ret;
  }

  template <class vtt> static vector<char> FP_NE(const vector<vtt> &dataV,
					 const vtt &threshValue, 
					 const vtt&epsilon) {
    vector<char> ret = fpVectorToScalarComparison(dataV, threshValue, epsilon, K_COMPARISON_NE);
    return ret;
  }


  /*
    Finite precision comparison operators, this set for comparing
    2 vectors, rather than a vector & a scalar.
  */
  template <class vtt> static vector<char> FP_GE_SS(const vector<vtt> &dataV, 
					    const vector<vtt> &dataV2, 
					    const vtt& epsilon) {
    vector<char> ret = fpVectorComparison(dataV, dataV2, epsilon, K_COMPARISON_GE);
    return ret;
  }

  /*
    Finite precision comparison operators, for scalars.
  */
  template <class vtt> static bool fp_gt(const vtt &x, 
					 const vtt &y, 
					 const vtt &epsilon) {
    vtt diff = fabs(y - x);
    if ((x > y) && (diff > epsilon)) {
      return true;
    }  
    return false;
  }
  template <class vtt> static bool fp_ge(const vtt& x, 
					 const vtt& y, 
					 const vtt &epsilon) {
    bool ret1 = fp_equal(x, y, epsilon);
    bool ret2 = fp_gt(x, y, epsilon);
    bool ret = ret1 || ret2;
    return ret;
  }
  template <class vtt> static bool fp_lt(const vtt &x, 
					 const vtt &y, 
					 const vtt &epsilon) {
    vtt diff = fabs(y - x);
    if ((x < y) && (diff > epsilon)) {
      return true;
    }  
    return false;    
  }
  template <class vtt> static bool fp_le(const vtt& x, 
					 const vtt& y, 
					 const vtt &epsilon) {
    bool ret1 = fp_equal(x, y, epsilon);
    bool ret2 = fp_lt(x, y, epsilon);
    bool ret = ret1 || ret2;    
    return ret;
  }

  template <class vtt> static bool fp_equal(const vtt& x, 
					    const vtt& y, 
					    const vtt &epsilon) {
    vtt diff = fabs(y - x);
    bool ret;
    if (diff <= epsilon) 
      ret = true; 
    else 
      ret = false;
    return ret; 
  }

  
  // Return:
  // x < 0:  -1
  // x == 0:  0
  // x > 0:  +1
  template <class vtt> static int sign(const vtt& x) {
    if (x < 0) return -1;
    if (x == 0) return 0;
    return 1;
  }

  //
  //  Return x shrunk toward 0 by y, but not crossing 0.
  //
  template <class vtt> static vtt shrink_toward_zero(const vtt& x,
						     const vtt& y) {
    vtt ret;
    int signX = sign(x);
    if (x > 0) {
      ret = x - y;
    } else {
      ret = x + y;
    }

    int signRet = sign(ret);
    if (signRet != signX) {
      ret = 0;
    }
    return ret;
  }


  /*
    Arithmatic operations:
    - add, subtract, multiply, divide.
  */
  static const int K_OPERATION_ADD = 1;
  static const int K_OPERATION_SUBTRACT = 2;
  static const int K_OPERATION_MULTIPLY = 3;
  static const int K_OPERATION_DIVIDE = 4;

  // vector OP scalar (aka apply OP scalare to every element in vector) version.
  template <class vtt> static vector<vtt> vectorScalarArithmeticOperation(const vector<vtt> &dataV,
						       const vtt &m, 
						       int operation) {
    int len = dataV.size();
    vector<vtt> ret;
    for (int i=0;i<len;i++) {
      ret.push_back(scalarScalarArithmeticOperation(dataV[i], m, operation));
    }
    return ret;
  }

  // scalar OP scalar version 
  template <class vtt> static vtt scalarScalarArithmeticOperation(const vtt &o1, const vtt &o2,
							   int operation) {
    vtt ret;
    switch (operation) {
    case K_OPERATION_ADD:
      ret = o1 + o2;
      break;
    case K_OPERATION_SUBTRACT:
      ret = o1 - 02;
      break;
    case K_OPERATION_MULTIPLY:
      ret = o1 * o2;
      break;
    case K_OPERATION_DIVIDE:
      ret = o1 / o2;
      break;
    default:
      assert(false);
    }
    return ret;
  }

  //
  //  Return dataV with each element multiplied by m.
  //
  template <class vtt> static vector<vtt> multiply(const vector<vtt> &dataV,
						     const vtt &m) {
    return vectorScalarArithmeticOperation(dataV, m, K_OPERATION_MULTIPLY);
  }

  //
  //  Return dataV with each element divided by m.
  //
  template <class vtt> static vector<vtt> divide(const vector<vtt> &dataV,
						     const vtt &m) {
    return vectorScalarArithmeticOperation(dataV, m, K_OPERATION_DIVIDE);
  }

  /*
    Conversion to/from raw arrays of underlying type....
  */

  // No allocation of new array.  Copies results into <intoMe>.
  template <class vtt> static void raw(const vector<vtt> &dataV, vtt *intoMe) {
    int len = dataV.size();
    return raw(dataV, intoMe, 0, len-1);
  }

  // Allocates array of length dataV.size(), which caller must explicitly
  //   deallocate.
  template <class vtt> static vtt *raw(const vector<vtt> &dataV) {
    int len = dataV.size();
    return raw(dataV, 0, len-1);    
  }

  // Returns array of vtt holding dataV[begin] -> dataV[end] (inclusive).
  // Also allocates array.
  template <class vtt> static vtt *raw(const vector<vtt> &dataV, int beg, int end) {
    int len = end - beg + 1;
    vtt *ret = new vtt[len];
    for (int i=beg;i<=end;i++) {
      ret[i - beg] = dataV[i];
    }
    return ret;
  }  


  

  // Also no allocation of new array.
  template <class vtt> static void raw(const vector<vtt> &dataV,
			   vtt *intoMe, int beg, int end) {
    for (int i=beg;i<=end;i++) {
      intoMe[i - beg] = dataV[i];
    }
  }

  // Makes new vector.
  template <class vtt> static vector<vtt> fromRaw(vtt *fromMe,
						    int len) {
    vector<vtt> ret;
    for (int i=0;i<len;i++) {
      ret.push_back(fromMe[i]);
    }
    return ret;  
  }

  // Copies into contents to existing vector....
  template <class vtt> static void fromRaw(vtt *fromMe,
						  int len,
						  vector<vtt> &toMe) {
    toMe.resize(0);
    for (int i=0;i<len;i++) {
      toMe.push_back(fromMe[i]);
    }  
  }  


  // Serial maximum function, aka return the maximum value in dataV.
  // Returns -Inf if dataV has length 0.
  template <class vtt> static vtt max(const vector<vtt> &dataV) {
    vtt telem, ret;
    int len = dataV.size();
    if (len == 0) {
      return (vtt) std::numeric_limits<double>::min();
    }
    ret = dataV[0];
    for (int i=1;i<len;i++) {
      telem = dataV[i];
      if (telem > ret) {
	ret = telem;
      }
    }
    return ret;
  }

  // Return (serial) minimum value in dataV, or Inf if dataV is
  //   of length 0.
  template <class vtt> static vtt min(const vector<vtt> &dataV) {
    vtt telem, ret;
    int len = dataV.size();
    if (len == 0) {
      return (vtt) std::numeric_limits<double>::max();
    }
    ret = dataV[0];
    for (int i=1;i<len;i++) {
      telem = dataV[i];
      if (telem < ret) {
	ret = telem;
      }
    }
    return ret;
  }

  // Return the simple (equally weighted) mean value in dataV.
  // Defined as 0 for zero-length vectors.
  template <class vtt> static double mean(const vector<vtt> &dataV) {
    int len = dataV.size();
    if (len == 0) return 0.0;
    double total = 0;
    for (int i=0;i<len;i++) {
      total += (double)dataV[i];
    }
    double ret = (total/((double)len));
    return ret;
  }

  // Return the weighted mean value in dataV, where weights are specified
  //   in wtV.
  // Defined as 0 for zero-length vectors.
  // Assumes that dataV & wtV have same length.
  template <class vtt> static double wmean(const vector<vtt> &dataV,
					     const vector<vtt> &wtV) {
    int dlen = dataV.size();
    int wlen = wtV.size();
    assert(dlen == wlen);
    if (dlen == 0) return 0.0;
    double totalNum = 0.0, totalDenom = 0.0;
    for (int i=0;i<dlen;i++) {
      totalNum += (double)(dataV[i] * wtV[i]);
      totalDenom += (double) wtV[i];
    }
    double ret = totalNum/totalDenom;
    return ((vtt)ret);
  }

  // SD of elements in dataV.  Defined to be 0 for zero-length dataV.
  template <class vtt> static double sd(const vector<vtt> &dataV) {
    int len = dataV.size();
    if (len == 0) return 0.0;
    double sum = 0.0, sumsq = 0.0;
    for (int i=0;i<len;i++) {
      sum += (double)dataV[i];
      sumsq += dataV[i] * dataV[i];
    }
    double ret = pow(sumsq/len - pow(sum/len,2),0.5);
    return ret;
  }

  // Pearson correlation between dataV1 and dataVs.  
  // Defined to be 0 for zero-length vectors.
  // dataV1 and dataV2 are assumed to have same length.
  // Code adopted from online version of numerical recipes in c:
  // http://www.fizyka.umk.pl/nrbook/c14-5.pdf
  const static double TINY = 1e-20;
  template <class vtt> static double cor(const vector<vtt> &x, const vector<vtt> &y) {
    double yt, xt;
    double syy = 0.0, sxy = 0.0, sxx = 0.0, ay = 0.0, ax = 0.0;
    unsigned long j;
    unsigned long n = x.size();
    if (n == 0) return 0.0;

    // Compute averages of both x and y.
    ax = mean(x);
    ay = mean(y);

    // Compute correlation coefficient.
    for (j=0;j<n;j++) {
      xt = x[j] - ax;
      yt = y[j] - ay;
      sxx += xt * xt;
      syy += yt * yt;
      sxy += xt * yt;
    }

    double ret = sxy / (sqrt(sxx * syy) + TINY);
    return ret;
  }

  // Sum of elements in dataV.  Defined to be 0 for zero-length dataV.
  template <class vtt> static vtt sum(const vector<vtt> &dataV) {
    vtt ret = (vtt)0.0;
    int sz = dataV.size();
    for (int i=0;i<sz;i++) {
      ret += dataV[i];
    }
    return ret;
  }

  template <class vtt> static vector<vtt> abs(const vector<vtt> &dataV) {
    int len = dataV.size();
    vector<vtt> retV(len);
    for (int i=0;i<len;i++) {
      retV[i] = (vtt)(fabs((double)dataV[i]));
    }
    return retV;
  }

  // Make ascending sequence going from start -> end, with increments of step.
  // addLast controls whether the sequence should always end with <end>, even if 
  //   there does not exist an n such that end == start + n * step.
  // e.g. seq(0, 250, 100, false) ==> 0, 100, 200.
  //      seq(0, 250, 100, true)  ==> 0, 100, 200, 250.
  template <class vtt> static vector<vtt> seq(const vtt &start, const vtt &end,
						const vtt &step, bool addLast=false) {
    vector<vtt> retV(0);
    retV.reserve((int)((end - start)/step));
    vtt tmp;
    for (tmp=start;tmp<=end;tmp+=step) {
      retV.push_back(tmp);
    }
    if (addLast && ((tmp - step) != end)) {
      retV.push_back(tmp);
    }
    return retV;
  }

  // version of seq that avoids copy of return vector.
  template <class vtt> static void seq(vector<vtt> &retV, const vtt &start, const vtt &end,
						const vtt &step, bool addLast=false) {
    retV.resize(0);
    retV.reserve((int)((end - start)/step));
    vtt tmp;
    for (tmp=start;tmp<=end;tmp+=step) {
      retV.push_back(tmp);
    }
    if (addLast && ((tmp - step) != end)) {
      retV.push_back(tmp);
    }
  }

  /*
    Return the index of the 1st element of dataV that exceeds maxValue.
    Should return dataV.length iff all elements of dataV are <= maxValue.
    Assumes dataV is already sorted in ascending order.
  */
  template <class vtt> static int firstGT(const vector<vtt> &dataV,
					    const vtt &maxValue) {
    int i;
    int sz = dataV.size();
    for (i=0;i<sz;i++) {
      if (dataV[i] > maxValue) {
	return i;
      }
    }
    return i;
  }

  template <class vtt> static int firstGE(const vector<vtt> &dataV,
					    const vtt &maxValue) {
    int i;
    int sz = dataV.size();
    for (i=0;i<sz;i++) {
      if (dataV[i] >= maxValue) {
	return i;
      }
    }
    return i;
  }

  template <class vtt> static int firstLT(const vector<vtt> &dataV,
					    const vtt &maxValue) {
    int i;
    int sz = dataV.size();
    for (i=0;i<sz;i++) {
      if (dataV[i] < maxValue) {
	return i;
      }
    }
    return i;
  }

  template <class vtt> static int firstLE(const vector<vtt> &dataV,
					    const vtt &maxValue) {
    int i;
    int sz = dataV.size();
    for (i=0;i<sz;i++) {
      if (dataV[i] <= maxValue) {
	return i;
      }
    }
    return i;
  }
};    //RVectorNumeric


#endif  //ifndef __RUTILS_H__

