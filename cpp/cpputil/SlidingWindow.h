/*
  Class that implements a sliding window (on a type that has defined comparison operators).
  Optimized for query time, not space efficiency.

*/

#ifndef __SLIDINGWINDOW_H__
#define __SLIDINGWINDOW_H__

#include <cstddef>
#include <iostream>
#include <queue>

/*
  Notes:
  - Intended for use with types that have correctly defined comparison operator.
  - Assumes that elements are inserted/pushed in sorted order.
*/
/*
  Declaration
 */
template <class T>
class SlidingWindow {
  queue<T> _queue;
 public:
  SlidingWindow();
  virtual ~SlidingWindow();
 
  // Number of elements in window.
  int size();
  // 1st element inserted & still present - oldest element in time.
  T& front();
  // Most recent element inserted & still present - most recent element in time.
  T& back();
  // Add new element to queue - becomes new *back element*.
  void push(T& val);
  // Removes all elements that are < val.
  // Technically - starts with most recently inserted element, and advances,
  //   removing elements, until it sees an element that is not < val.
  // Returns number of elements deleted by this call to advance operation.
  int advance(T & val);
};

/*
  Definition
 */
template<class T>
SlidingWindow<T>::SlidingWindow( ) :
  _queue() 
{

}

template<class T>
SlidingWindow<T>::~SlidingWindow( ) {

}

template<class T>
int SlidingWindow<T>::size( ) {
  return _queue.size();
}

template<class T>
T& SlidingWindow<T>::front( ) {
  return _queue.front();
}

template<class T>
T& SlidingWindow<T>::back( ) {
  return _queue.back();
}

template<class T>
void SlidingWindow<T>::push(T& val ) {
  _queue.push(val);
}

template<class T>
int SlidingWindow<T>::advance(T& val ) {
  int sz = _queue.size();
  int ret = 0;
  for (int i=0;i<sz;i++) {
    T &fval = _queue.front();
    if (fval >= val) break;
    _queue.pop();
    ret++;
  }
  return ret;
}
#endif   //  __SLIDINGWINDOW_H__
