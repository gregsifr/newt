#ifndef __HFUTILS_H__
#define __HFUTILS_H__

#include "Markets.h"
#include "DataManager.h"

class DataManager;
class AlphaSignal;

class HFUtils {
 public:

  // Is px1 more (less) aggressive than px2?	 
  static bool moreAggressive(Mkt::Side side, double px1, double px2);
  static bool lessAggressive(Mkt::Side side, double px1, double px2);
  static bool geAggressive(Mkt::Side side, double px1, double px2);
  static bool leAggressive(Mkt::Side side, double px1, double px2);

  /*
    Price increment/decrement aggressiveness functions.
  */
  // Return nprice that is more (less) aggressive that price by increment.
  // Increments price by increment in specified direction.
  // Somewhat confusingly named.  Should probably be increment/decrement, rather than
  //   makeMore/makeLess.
  static double makeMoreAggressive(Mkt::Side side, double price, double increment);
  static double makeLessAggressive(Mkt::Side side, double price, double increment);

  /*
    Price rounding (in aggressiveness space) functions.
  */
  // Round price to nearest increment of mpv, in specified direction (less/more aggressive).
  // Given a price, find the closest even increment of mpv that has LE/GE aggressiveness
  //   than the specified initial price.
  static double roundLEAggressive(Mkt::Side side, double price, double mpv);
  static double roundGEAggressive(Mkt::Side side, double price, double mpv);
  static double roundClosest(double price, double mpv);

  /*
    Price truncation (in aggressiveness space) functions.
  */
  // Resest price relative to a specified aggressiveness limit, and *also*
  //   make sure that price is an even increment of mpv.
  // Given a price px, return the closest even increment of mpv that is 
  //   strictly less/more aggressive than limit.
  static double pushToLessAggressive(Mkt::Side side, double price, double limit, double mpv);
  static double pushToMoreAggressive(Mkt::Side side, double price, double limit, double mpv);
  // Given a price px, return the closest even increment of mpv that is 
  //   not more aggressive / not less aggressive than limit px.
  static double pushToLEAggressive(Mkt::Side side, double price, double limit, double mpv);
  static double pushToGEAggressive(Mkt::Side side, double price, double limit, double mpv);

  /*
    TimeStamp comparison functions.
  */
  // is tv1 within epsilon of tv2.
  static bool tsFuzzyGE(const TimeVal &tv1, const TimeVal &tv2);
  // is tv1 <= (tv2 + epsilon).
  static bool tsFuzzyLE(const TimeVal &tv1, const TimeVal &tv2);
  // is tv1 = tv2, to within epsilon.
  static bool tsFuzzyEQ(const TimeVal &tv1, const TimeVal &tv2);
  // tv2 - tv1, in milliseconds.
  static double milliSecondsBetween(const TimeVal &tv1, const TimeVal &tv2);

  /*
    Simplified book query functions.
  */
  // Top-level CBBO price(s), ignoring odd-lots.
  // best inside price, ignoring odd lots.
  static bool bestPrice(DataManager *dm, int cid, Mkt::Side side, double &px);
  static bool bestMid(DataManager *dm, int cid, double &px, AlphaSignal *fvSignal = NULL);
  static bool bestSize(DataManager *dm, int cid, Mkt::Side side, size_t &sz);
  static void bestPrices(DataManager *dm, Mkt::Side side, double defaultValue, vector<double> &fv);
  static void bestMids(DataManager *dm, double defaultValue, vector<double> &fv);
  static void bestPrices(DataManager *dm, Mkt::Side side, vector<double> &defaultValues, vector<double> &fv);
  static void bestMids(DataManager *dm, vector<double> &defaultValues, vector<double> &fv, AlphaSignal *fvSignal = NULL);

  static bool getTradeableMarket(DataManager*dm, int cid, Mkt::Side side, int level,
				 size_t minSize, double *price, size_t *size);

  // Round from double to closest integer.
  static int roundDtoI(double x) { return int(x + 0.5); }


  /*
    Odd-lot vs round-lot utility functions.
  */
  static bool isOddLot(int size) { return (size>=0 && size<100); }
  static bool isRoundLot(int size) { return size >= 100; }
  static bool isNYSEOddLot(int size) { return (size%100) != 0; }
  static bool isNYSERoundLot(int size) { return (size%100) == 0; }
  static int roundSizeForNYSE(int numShares) { return numShares / 100 * 100; }

  /*
    Utility functions for breaking bigger orders up into smaller bit-sized chunks.
  */
  /// Break order up into specified chunk-size, plus possibly a single odd lot.
  static void     chunkOrderShares( int numShares, int chunkSize, vector<int> &chunks );  

  static void setupLogger(tael::Logger *dbg, tael::LoggerDestination *dest);

  /// Global Constants for Exec Engine.
  /// How long (in milliseconds) to allow book to be locked/crossed before cancelling orders.
  const static int INVALD_MKT_CANCEL_MS = 500;
};

#endif   // __HFUTILS_H__

