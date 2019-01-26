#include "CrossComponent.h"

#include "HFUtils.h"
#include "AlphaSignal.h"
#include <cl-util/float_cmp.h>

#include <cmath>

using namespace clite::util;

const ECN::ECN ECNS_TO_PRIORITIES_BY_FEE[] = {ECN::ISLD, ECN::BATS, ECN::EDGA, ECN::ARCA, ECN::NYSE, ECN::BSX};
const int N_ECNS_PRIORITISED = sizeof(ECNS_TO_PRIORITIES_BY_FEE) / sizeof(ECN::ECN);

vector< vector<ECN::ECN> > CrossComponent::_ecnsSortedByFees; 
bool CrossComponent::_ecnsSortedByFeesInitialized = false;

const int IOC_TIMEOUT = 0;

CrossComponent::CrossComponent( int waitMilliSec, AlphaSignal *fvSignal )
  : TradeLogicComponent(),
    _stocksState( factory<StocksState>::get(only::one) ),
    _tradeConstraints( factory<TradeConstraints>::get(only::one) ),
    _exchangeT( factory<ExchangeTracker>::get(only::one) ),
    _feeCalc( factory<FeeCalc>::get(only::one) ),
    _waitMilliSec(waitMilliSec),
    _priorityScale(1.0),
    _fvSignal(fvSignal),
    _enabled(true)
{
  _lastOPTime.resize( _dm->cidsize(), 0 );

  initializeVectorOfSortEcns();
}

/*
  Logic:
  In current version of DataManager code, query aggregate book is relatively cheap operation.
    but querying sub-books can be quite expensive.  Thus, as a speed optimization, we first
    check priority vs spread + fees at an aggregate level.
  #1) Compute cbid, cask, and inside spread (using round-lots only).
  #2) Combine this with estimate of best-case fees.
  ==> Compare with priority.  If priority too low, return.
  May be able to cross.  Do detailed analysis
*/
void CrossComponent::suggestOrderPlacements( int cid, Mkt::Side side, int tradeLogicId, 
					     int numShares, double priority,
					     vector<OrderPlacementSuggestion> &suggestions ) {
  suggestions.clear();
  if (!_enabled)
    return;
  if( numShares <= 0 )
    return;

  SingleStockState *ss = _stocksState->getState(cid);
  if( !ss->haveNormalMarket() || !ss->haveNormalExclusiveMarket() )
    return;

  // Have issued crossing order for stock too recently. Don't try to trade it in current wakeup context.
  if( HFUtils::milliSecondsBetween(_lastOPTime[cid], _dm->curtv()) < _waitMilliSec ) 
    return;

  // Check if 1/2 spread + common TC are > priority. 
  // (common TC = those fees per share that do not depend on ECN, but are variable over time, 
  //  i.e. not including fixed costs like Brokerage fee)
  // Priority not high enough ==> no OrderPlacementSuggestions.
  // Using EXCLUSIVE market, because we shouldn't account our own orders when calculating the bbo
  // - Changed this when including signal/fv in cross calculation.
  // - Book based signal includes our own orders when computing fv.  Thus, should use
  //   total mid, not exclusive mid.
  //double mid = ss->getExclusiveMid();
  double mid = ss->mid();
  // (common TC = those fees per share that do not depend on ECN and on size of specific order)
  //    Priority not high enough ==> no OrderPlacementSuggestions.
  Mkt::Side crossSide =( side == Mkt::BID ? Mkt::ASK : Mkt::BID );
  double    crossPrice = ss->bestPrice( crossSide );

  // Adjust mid for signal, if present.
  double alpha = 0.0;
  if (_fvSignal == NULL || !_fvSignal->getAlpha(cid, alpha)) {
    alpha = 0.0;
  }

  // Problem computing signal.  Return.
  if (isnan(alpha)) {
    return;
  }

  double fv = mid * (1.0 + alpha);
  double rawCrossCost;
  if (side == Mkt::BID) {
    rawCrossCost = crossPrice - fv;
  } else {
    rawCrossCost = fv - crossPrice;
  }

  // Estimate px to cross from true mid --> side.
  double brokerageFeePerSh = _feeCalc->brokerageFee( 1 ); // this is calculation per-share
  double commonTC = (rawCrossCost + brokerageFeePerSh)/mid;  // common TC: fees per share that don't depend on ECN and on order-size


  if( cmp<6>::LT(priority/_priorityScale,commonTC) )
    return;
  
  // Populate bestSizes and bestPrices with CBBO top-level bid/ask prices + sizes, per ECN.
  // This version should:
  // - Include only quotes at CBBO, not per-ecn BBO.
  // - Indicate that an ecn has no volume by setting bestSizes[e] to 0.
  double      temp_px;
  size_t      temp_sz;
  int totalSize = 0;
  vector<int> bestSizes( ECN::ECN_size, 0 );
  for( int i=0; i<ECN::ECN_size; i++ ) {
    ECN::ECN ecn = ECN::ECN(i);
    if( !_tradeConstraints->canPlace(cid,ecn,crossPrice) ) continue;
    if( !getMarket(_dm->subBook(ecn), cid, crossSide, 0, &temp_px, &temp_sz) ) continue;
    if( HFUtils::lessAggressive(crossSide, temp_px, crossPrice) ) continue;
    bestSizes[ i ] = temp_sz;
    totalSize += temp_sz;
  }

  Mkt::Tape tape      = _exchangeT -> getTape( cid );

  int lotSize = 100;
  int totalRoundedLots = totalSize / lotSize;
  ECN::ECN adjustedECN = ECN::UNKN;
  int adjustmentSize = 0;
  // We only need to reduce the size if we have a total size equal to some number (> 1) of round lots
  if ((totalSize > lotSize) && (totalRoundedLots*lotSize == totalSize)) {
	for(int i=_ecnsSortedByFees[tape].size()-1; i >= 0; i--) {
	  ECN::ECN ecn = _ecnsSortedByFees[tape][i];
	  if(bestSizes[ecn] >= lotSize) {
		  bestSizes[ecn] -= lotSize;
		  adjustedECN = ecn;
		  adjustmentSize = lotSize;
		  break;
	  }
	}
  }

  // Break total order up into N round-lots + possibly 1 odd lot.
  vector<int> chunkSizes;
  HFUtils::chunkOrderShares(numShares, 100, chunkSizes);
  
  // Walk through and assign each chunk to optimal ECN.
  vector<int> orderSizes( ECN::ECN_size, 0);
  for( unsigned int i=0; i<chunkSizes.size(); i++ ) {
    int chunkSz = chunkSizes[i];
    ECN::ECN chosenECN = chooseECN( cid, chunkSz, tape, bestSizes );
    if( chosenECN == ECN::UNKN ) 
      continue;         // was break.
    double removeLiqFee = _feeCalc->takeLiqFee( chosenECN, tape );   // FeeCalc expresses fees as positive numbers
    // SEC and NASD fees are for sell/short only, but in order to avoid buy-bias, we take them as half for each side
    double secFeePerSh = 0.5 * _feeCalc->getSECFee( chunkSz*crossPrice ) / chunkSz; 
    double nasdFeePerSh = 0.5 * _feeCalc->getNASDFee( chunkSz ) / chunkSz; // this is calculation per-share
    double totalTC = commonTC + (removeLiqFee + secFeePerSh + nasdFeePerSh)/mid;
    if( cmp<6>::LT(priority/_priorityScale,totalTC) ) 
      continue;         // was break;
    orderSizes[ chosenECN ] += chunkSz;
    bestSizes[  chosenECN ] -= chunkSz;
  }
  
  // Generate an OrderPlacementSuggestion for each ECN with non-zero order size.
  // By convention, we allocate all sub-orders generated as part of the same cross
  //   attempt the same component sequence number.
  int csn = allocateSeqNum();
  for( int i=0; i<ECN::ECN_size; i++ ) {
    if( orderSizes[i] == 0 ) continue;
    OrderPlacementSuggestion suggestion( cid, ECN::ECN(i), side, orderSizes[i], crossPrice, IOC_TIMEOUT, 
					 tradeLogicId, _componentId, csn, OrderPlacementSuggestion::CROSS,
					 _dm->curtv(), ss->bestPrice(Mkt::BID), ss->bestPrice(Mkt::ASK),
					 priority);
    suggestions.push_back( suggestion );
  }
  
  // If suggested any orders, mark _lastOPTime[cid].
  if( suggestions.size() > 0 ) {
    // FOR DEBUGGING:
    //   Info on sizes trying to cross against.
    TAEL_PRINTF(_logPrinter.get(), TAEL_INFO, "%-5s Cross: Sizes of (opposite) best level: Total: %d ISLD: %d, BATS: %d, ARCA: %d, "
			 "EDGA: %d, NYSE: %d",
			 _dm->symbol(cid), getMarketSize(_dm->masterBook(), cid, crossSide, crossPrice), 
			 bestSizes[ECN::ISLD] + orderSizes[ECN::ISLD], 
			 bestSizes[ECN::BATS] + orderSizes[ECN::BATS], 
			 bestSizes[ECN::ARCA] + orderSizes[ECN::ARCA], 
			 bestSizes[ECN::EDGA] + orderSizes[ECN::EDGA], 
			 bestSizes[ECN::NYSE] + orderSizes[ECN::NYSE]);
    if (adjustmentSize > 0) {
    	TAEL_PRINTF(_logPrinter.get(), TAEL_INFO, "adjustment: %d, adjusted ECN: %s", adjustmentSize, ECN::desc(adjustedECN));
    }
    //    Info on mid, signal, fv, crossPrice, priority.
    TAEL_PRINTF(_logPrinter.get(), TAEL_INFO, "%-5s Cross: Signal Info: MID%.3f  CPX%.2f  ALP%.6f  FV%.4f  CMNTC%.6f  PRI%.6f",
			 _dm->symbol(cid), mid, crossPrice, alpha, fv, commonTC, priority );
    
    _lastOPTime[ cid ] = _dm->curtv();
  }

  return;
}

/// Among ECNs with sufficient size, choose the one with lowest take-liqudity-fees.
/// Return ECN::UNKN if no ECN is good
ECN::ECN CrossComponent::chooseECN( int cid, int size, Mkt::Tape tape, const vector<int>& ecnSizes ) {

  vector<char> possibleEcns( ECN::ECN_size, true );

  // Don't try to trade odd lots on ARCA or NYSE.
  if( HFUtils::isNYSEOddLot(size) ) possibleEcns[ECN::NYSE] = false;
  if( HFUtils::isOddLot(size)     ) possibleEcns[ECN::ARCA] = false;

  // Limit destinations to those with sufficient number of shares for this chunk.
  for( unsigned int i=0; i<ecnSizes.size(); i++ )
    possibleEcns[i] = possibleEcns[i] && (ecnSizes[i] >= size);

  // From among allowed destinations, choose one with lowest take-liquidity fees.
  for( unsigned int i=0; i<_ecnsSortedByFees[tape].size(); i++ ) {
    ECN::ECN ecn = _ecnsSortedByFees[tape][i];
    if( possibleEcns[ecn] ) return ecn;
  }
  return ECN::UNKN;
}

/// for each tape, this keeps a vector of ECNs sorted by increasing remove-liquidity fees 
void CrossComponent::initializeVectorOfSortEcns() {

  if( _ecnsSortedByFeesInitialized )
    return;
  else
    _ecnsSortedByFeesInitialized = true;

  _ecnsSortedByFees.resize( Mkt::NUM_TAPES );
  
  vector<ECN::ECN> basicVec( N_ECNS_PRIORITISED );
  for( int i=0; i<N_ECNS_PRIORITISED; i++ )
    basicVec[i] = ECNS_TO_PRIORITIES_BY_FEE[i];
  
  vector<double> liquidityFees(ECN::ECN_size);

  for( int i=0; i<Mkt::NUM_TAPES; i++ ) {
    Mkt::Tape tape = (Mkt::Tape)i;
    for( int j=0; j<ECN::ECN_size; j++ ) 
      liquidityFees[j] = _feeCalc->takeLiqFee( (ECN::ECN)j, tape );
    // A simple bubble-sort (recall this function is called only once, upon initialization)
    _ecnsSortedByFees[i] = basicVec;
    int j=0; // the beginning of the unsorted area in the vector
    while( j < N_ECNS_PRIORITISED-1 ) {
      // check whether _ecnsSortedByFees[i][j] should come after _ecnsSortedByFees[i][j+1]
      // (i.e. if (remove_fee1 > remove_fee2)
      ECN::ECN ecn1 = _ecnsSortedByFees[i][j];
      ECN::ECN ecn2 = _ecnsSortedByFees[i][j+1];
      if( cmp<5>::GT(liquidityFees[ecn1], liquidityFees[ecn2]) ) {
	// ==> swap
	_ecnsSortedByFees[i][j]   = ecn2;
	_ecnsSortedByFees[i][j+1] = ecn1;
	j = std::max( 0, j-1 );
      }
      else 
	j++;
    }
    // Print the result
    int BUF_SIZE = 1024;
    char buffer[BUF_SIZE];
    int nChars = snprintf( buffer, BUF_SIZE, "CrossComponent::initializeVectorOfSortEcns: The sorted ECNs for %s are:", 
			  Mkt::TapeDesc[tape] );
    for( int j=0; j<N_ECNS_PRIORITISED; j++ )
      nChars += snprintf( buffer+nChars, BUF_SIZE, " %s", ECN::desc(_ecnsSortedByFees[i][j]) );
    TAEL_PRINTF(_logPrinter.get(), TAEL_INFO, "%s", buffer );
  }
}
