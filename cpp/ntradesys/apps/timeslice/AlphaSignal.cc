#include "AlphaSignal.h"

#include <algorithm>
using namespace std;


/**********************************************************************
  PlaceHolderAlphaSignal.
**********************************************************************/
PlaceHolderAlphaSignal::PlaceHolderAlphaSignal(int cidsize, double dv) :
  _alphaV(cidsize, dv)
{
  
}

PlaceHolderAlphaSignal::~PlaceHolderAlphaSignal() {

}

bool PlaceHolderAlphaSignal::getAlpha(int cid, double &alpha) {
  int sz = _alphaV.size();
  if (cid < 0 || cid >= sz) {
    alpha = 0.0;
    return false;
  }
  alpha = _alphaV[cid];
  return true;
}

bool PlaceHolderAlphaSignal::setAlpha(int cid, double alpha) {
  int sz = _alphaV.size();
  if (cid < 0 || cid >= sz) {
    return false;
  }
  _alphaV[cid] = alpha;
  return true;
}

