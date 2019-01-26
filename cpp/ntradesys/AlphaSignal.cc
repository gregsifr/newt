/*
  AlphaSignal.cc
  Code for initial attempt at describing alpha/retrurn predicting signal.
*/

#include "AlphaSignal.h"

#include <algorithm>
using namespace std;

/**********************************************************************
  AlphaSignal.
**********************************************************************/
AlphaSignal::AlphaSignal() {

}

AlphaSignal::~AlphaSignal() {

}

/**********************************************************************
  PlaceHolderAlphaSignal.
**********************************************************************/
PlaceHolderAlphaSignal::PlaceHolderAlphaSignal(int cidsize, double dv) :
  AlphaSignal(),
  _alphaV(cidsize, dv)
{
  
}

PlaceHolderAlphaSignal::~PlaceHolderAlphaSignal() {

}

bool PlaceHolderAlphaSignal::getAlpha(int cid, double &fv) {
  int sz = _alphaV.size();
  if (cid < 0 || cid >= sz) {
    fv = 0.0;
    return false;
  }
  fv = _alphaV[cid];
  return true;
}

bool PlaceHolderAlphaSignal::setAlpha(int cid, double fv) {
  int sz = _alphaV.size();
  if (cid < 0 || cid >= sz) {
    return false;
  }
  _alphaV[cid] = fv;
  return true;
}

