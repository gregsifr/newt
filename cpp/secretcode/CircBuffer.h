#ifndef _CIRCBUFFER_H
#define _CIRCBUFFER_H

/*
   Template class implementing a circular buffer.
   Also includes functionality for getting mean & stdev of 
     elements in buffer.
*/
#include <cstddef>
#include <iostream>

#include "math.h"

template<class T>
class CircBuffer {
  public:
    //Constructor that creates vector of size initSize
    CircBuffer( int initSize );

    ~CircBuffer();

    // Number of elements currently in buffer.
    int size() const { return num; }

    void add( const T & elem );
    void clear();
    bool getAvg( T *avgVal ) const;
    bool getAvg( unsigned int numElem, T *avgVal ) const;
    bool getStdev( T *stdVal) const;
    bool getStdev( unsigned int numElem, T *stdVal ) const;
    inline bool getLast ( T* lastVal) const;                      // Last element.
    inline bool getLast ( unsigned int offset, T* lastVal ) const; // Element at last position, plus offset, counting backwards.
                                                   //   e.g. offset = 0 --> last element.
                                                   //        offset = 1 --> element inserted immediately before that.

    // Dumps representation of buffer contents to stdout....
    void print() const;

    // Estimate pearson correlation of contents of 2 CircBuffers.
    // - Both buffers should have same size.
    // - Uses all elements in both buffers.
    static bool getCor(const CircBuffer<T> *x, const CircBuffer<T> *y, T* fv); 

    // Estimate pearson correlation using trailing <numEleme> elements only.
    // - Both buffers should have at least that many elements.
    static bool getCor(const CircBuffer<T> *x, const CircBuffer<T> *y, unsigned int numElem, T* fv); 
  private:
    int num; //number of elements in buffer
    int wIdx; //index to be written
    T *buff;
    int maxSize;
    T _sum;
    T _sumsq;
};

template<class T>
CircBuffer<T>::CircBuffer( int initSize ) :
  num(0), wIdx(0), maxSize(initSize), _sum(0), _sumsq(0) 
{
  buff = new T[initSize];
  for (int i=0;i<initSize;i++)
    buff[i]=0;
}

template<class T>
CircBuffer<T>::~CircBuffer() {
  if (buff != NULL) delete [] buff;
  maxSize=0;
  num=0;
  wIdx=0;
}

template<class T> 
void CircBuffer<T>::add( const T & elem ) {
  _sum-=buff[wIdx];
  _sumsq-=buff[wIdx]*buff[wIdx];
  buff[wIdx] = elem;
  wIdx++;
  _sum+=elem;
  _sumsq+=elem*elem;
  if (wIdx == maxSize) {
    wIdx = 0;
  }
  if (num < maxSize) {
    num++;
  }
}

template<class T> 
bool CircBuffer<T>::getLast( T* lastVal) const {
  *lastVal = -1.0;
  if ( num == 0)
    return(false);
  if (wIdx==0)
    *lastVal = buff[maxSize-1];
  else
    *lastVal = buff[wIdx-1];
  return true;
}

template<class T> 
bool CircBuffer<T>::getLast( unsigned int offset, T* lastVal) const {
  *lastVal = -1.0;
  if (offset >= (unsigned int)num) { 
    return false;
  }
  int idx = wIdx - 1 - offset;
  if (idx < 0) {
    idx += maxSize;
  }
  *lastVal = buff[idx];
  return true;
}

template<class T>
void CircBuffer<T>::clear() {
  wIdx = 0;
  num = 0;
}


template<class T>
bool CircBuffer<T>::getAvg( T *avgVal ) const {
  if (num >0){
    *avgVal = _sum/num;
    return(true);
  }
  return(false);
    
}


template<class T>
bool CircBuffer<T>::getAvg( unsigned int numElem, T *avgVal ) const {
  T sumElem = 0;
  bool ret = false;
  if ((int) numElem <= num) {
    int rIdx = wIdx - 1;
    for (unsigned int i=0; i<numElem; i++) {
      if (rIdx < 0) {
        //The read index is going off the beginning of the
        //buffer so move it back to the end
        rIdx = rIdx + maxSize;
      }
      sumElem += buff[rIdx];
      rIdx--;
    }
    *avgVal = sumElem / numElem;
    ret = true;
  }  
  return ret;
}


//Computes the standard deviation of all the elements
//Computed stdev value is returned in stdVal
template<class T>
bool CircBuffer<T>::getStdev( T *stdVal ) const {
  if (num >0){
    *stdVal = pow(_sumsq/num- pow(_sum/num,2),0.5);
    return(true);
  }
  return(false);
}


//Computes the standard deviation of the past numElem elements
//Returns true if the stdev can be calculated
//        false otherwise (i.e. not enough elements)
//Stdev value is returned in stdVal
template<class T>
bool CircBuffer<T>::getStdev( unsigned int numElem, T *stdVal ) const {
  T sumElem = 0;
  T avgElem = -1;
  bool ret = false;
  if ((int) numElem <= num) {
    int rIdx = wIdx - 1;
    unsigned int i;
    for (i=0; i<numElem; i++) {
      if (rIdx < 0) {
        rIdx = rIdx + maxSize;
      }
      sumElem += buff[rIdx];
      rIdx--;
    }
    avgElem = sumElem / numElem;

    rIdx = wIdx -1;
    sumElem = 0;
    for (i=0; i<numElem; i++) {
      if (rIdx < 0) {
        rIdx = rIdx + maxSize;
      }
      sumElem += (buff[rIdx] - avgElem) *
                 (buff[rIdx] - avgElem);
      rIdx--;
    }
    *stdVal = pow(sumElem / numElem, 0.5);
    ret = true;
  }  
  return ret;
}

template<class T>
void CircBuffer<T>::print() const {
  T val;
  std::cout << "------------------CircBuffer-------------------" << std::endl;
  std::cout << "num " << num << "  wIdx" << wIdx << "  maxSize" << maxSize << "  sum " << _sum 
	    << "  _sumsq " << _sumsq << std::endl;
  unsigned int sz = (unsigned int) size();
  for (unsigned int i = sz - 1 ; i > 0 ; i--) {
    getLast(i, &val);
    std::cout << val << ",  ";
  }
  if (sz > 0) {
    getLast(0, &val);
    std::cout << val << "  " << std::endl;
  }
}

const static double TINY = 1e-20;
template<class T>
bool CircBuffer<T>::getCor(const CircBuffer<T> *x, const CircBuffer<T> *y, T* fv) {
    T yt, xt;
    T syy = 0.0, sxy = 0.0, sxx = 0.0, ay = 0.0, ax = 0.0;
    unsigned long j;
    unsigned long n = x.size();
    if (n == 0) {
      fv = 0;
      return false;
    }

    // Compute averages of both x and y.
    x.getAvg(ax);            //ax = mean(x); 
    y.getavg(ay);            //ay = mean(y);

    // Compute correlation coefficient.
    for (j=0;j<n;j++) {
      // xt = x[j] - ax;
      // yt = y[j] - ay;
      x->getLast(j, &xt);  
      y->getLast(j, &yt);
      xt -= ax;
      yt -= ay;
      sxx += xt * xt;
      syy += yt * yt;
      sxy += xt * yt;
    }

    double ret = sxy / (sqrt(sxx * syy) + TINY);
    fv = ret;
    return true;  
}

template<class T>
bool CircBuffer<T>::getCor(const CircBuffer<T> *x, const CircBuffer<T> *y, unsigned int numElem, T* fv) {
    T yt, xt;
    T syy = 0.0, sxy = 0.0, sxx = 0.0, ay = 0.0, ax = 0.0;
    unsigned long j;
    unsigned long n = numElem;
    if (n == 0 || n > (unsigned int)x->size() || n > (unsigned int)y->size()) {
      fv = 0;
      return false;
    }

    // Compute averages of both x and y.
    x->getAvg(numElem, &ax);            //ax = mean(x); 
    y->getAvg(numElem, &ay);            //ay = mean(y);

    // Compute correlation coefficient.
    for (j=0;j<n;j++) {
      // xt = x[j] - ax;
      // yt = y[j] - ay;
      x->getLast(j, &xt);  
      y->getLast(j, &yt);
      xt -= ax;
      yt -= ay;
      sxx += xt * xt;
      syy += yt * yt;
      sxy += xt * yt;
    }

    double ret = sxy / (sqrt(sxx * syy) + TINY);
    *fv = ret;
    return true;  
}

#endif  //_CIRCBUFFER_H
