#include "PriorityCutoffComponent.h"

const double PriorityCutoffComponent::DEFAULT_PRIORITY = 0.000010;    // 0.1 bps.
const double PriorityCutoffComponent::ADVERSE_FILL_PL  = 0.000035;    // 0.35 bps.

/************************************************************
  ImplShortFallComponent code.
************************************************************/
PriorityCutoffComponent::PriorityCutoffComponent(AlphaSignal *stSignal) :
  PriorityComponent(stSignal)
{
  _tlMIM = factory<TakeLiquidityMarketImpactModel>::get(only::one);
  if( !_tlMIM )
    throw std::runtime_error( "Failed to get TakeLiquidityMarketImpactModel from factory (in PriorityCutoffComponent::PriorityCutoffComponent)" );  

  _avgTIM = factory<AverageTradingImpactModel>::get(only::one);
  if (!_avgTIM) 
    throw std::runtime_error( "Failed to get AverageTradingImpactModel from factory (in PriorityCutoffComponent::PriorityCutoffComponent)" );
 
  _plTIM = factory<ImbTradingImpactModel>::get(only::one);
  if (!_plTIM) 
    throw std::runtime_error( "Failed to get ImbTradingImpactModel from factory (in PriorityCutoffComponent::PriorityCutoffComponent)" );

  _plPRM = factory<DefaultProvideLiquidityPRM>::get(only::one);
  if (!_plTIM) 
    throw std::runtime_error( "Failed to get DefaultProvideLiquidityPRM from factory (in PriorityCutoffComponent::PriorityCutoffComponent)" );

  _rmiTracker = factory<RealizedMarketImpactTracker>::get(only::one);
  if (!_plTIM) 
    throw std::runtime_error( "Failed to get RealizedMarketImpactTracker from factory (in PriorityCutoffComponent::PriorityCutoffComponent)" );

  _spdT = factory<SpdTracker>::get(only::one);
  if (!_spdT) 
    throw std::runtime_error( "Failed to get SpdTracker from factory (in PriorityCutoffComponent::PriorityCutoffComponent)" );

  _volumeT = factory<IBVolumeTracker>::get(only::one);
  if (!_volumeT) 
    throw std::runtime_error( "Failed to get IBVolumeTracker from factory (in PriorityCutoffComponent::PriorityCutoffComponent)" );

  _dm->add_listener(this);
}

PriorityCutoffComponent::~PriorityCutoffComponent() {

}

/*
  On receipt of a new trade-request:
  - Print estimated TC & mkt-impact summary info.  Example: 
  TIMESTAMP  DELL  0  EST-MI-TL-SLOW %.8f  EST-MI-PL %.8f  EST-MID-TRAJ %.8f  EST-END-TRAJ %.8f  
*/
void PriorityCutoffComponent::update(const TradeRequest &tr) {
  /* 
     Print info about new trade-request, plus transaction costs estimates for it.
   */
  char buf[256];
  double midTraj, endTraj, pRate;
  MarketImpactEstimate miTLSlow, miPL;
  tr.snprint(buf, 256);
  // Print TR summary.
  TAEL_PRINTF(_logPrinter.get(), TAEL_WARN, "%-5s  CID%i  TRADE-REQ  :  %s",
		  _dm->symbol(tr._cid), tr._cid, buf); 

  /*
   Print summary of how what RealizedMarketImpactTracker thinks resulting (stopped/continued)
     trade-request is, 
  */
  TradeRequest mergedTR = _rmiTracker->lastTR(tr._cid);
  mergedTR.snprint(buf, 256);
  TAEL_PRINTF(_logPrinter.get(), TAEL_WARN, "%-5s  CID%i  MERGED-TR  :  %s",
		  _dm->symbol(mergedTR._cid), mergedTR._cid, buf);  
 

  // Estimate order participation rate - for 100% liquidity providing strategy only.
  TimeVal curtv = _dm->curtv();
  Mkt::Trade dir = (mergedTR._targetPos >= mergedTR._initPos? Mkt::BUY : Mkt::SELL);
  int additionalSize = tr.unsignedSize();
  //int trSize = mergedTR.unsignedSize();
  estimatePRate(tr._cid, dir, mergedTR._priority, curtv, curtv, pRate);

  // Estimate market-impact of taking slowly taking liquidity.
  // - Estimate uses trailing average market-impact #s, not 
  //   market impact #s from current market state.
  // - Estimate is likely to be an *over* estimate, as it assumes
  //   liquidity is taken 100 shares at a time, and that total cost
  //   is multiplicative in # of lots traded.  This effectively ignores
  //   likely concave (downward) shape of market impact function for
  //   taking liquidity in larger blocks than single round lots. 
  // - Estimate is for # of shs in more recent leg of trade-request.
  //   not for entire trade-request including already done parts.
  _avgTIM->marketImpactTrading(tr._cid, abs(_rmiTracker->shsTL(tr._cid)), 
			       additionalSize, dir, pRate, miTLSlow);

  // Estimate market-impact of providing liquidity slowly.
  // - Estimate is again for part of trae request that has not yet been filled,
  //   not for entire trade-request.
  _plTIM->marketImpactTrading(tr._cid, abs(_rmiTracker->shsPL(tr._cid)), 
			      additionalSize, dir, pRate, miPL);

  // Estimate expected 50% completion time & estimated 100% completion time for trade request.
  // - These estimates are for the incremental part of the TR that has not yet been filled.
  TimeVal estMidTV, estEndTV;
  estimateCompletionTime(tr._cid, additionalSize, pRate * 2.0, curtv,  estMidTV);    // ESTIMATE TR 1/2 LIFE TIME;
  estimateCompletionTime(tr._cid, additionalSize, pRate,       curtv,  estEndTV);    // ESTIMATE TR END TIME;

  // Estimate realized alpha (clean of our trading) to end of 100% completion time trajectory.
  estimateSignalProfile(tr._cid, dir, tr._priority, tr._recvTime, curtv, estEndTV, endTraj);
  // Estimate realized alpha (clean of our trading) to 1/2 life of request.
  estimateSignalProfile(tr._cid, dir, tr._priority, tr._recvTime, curtv, estMidTV, midTraj);
  
  // Estimate average transaction costs across entire request.
  double avgTC;
  estimateTransactionCosts(tr._cid, additionalSize, dir, tr._priority, tr._recvTime, curtv, avgTC);

  // Print summary info with guesstimates of market impact & alpha trajectory for request.
  TAEL_PRINTF(_logPrinter.get(), TAEL_WARN, "%-5s  CID%i  IC-TC-EST  :  PTGT %i  IPOS %i  TPOS %i  EST-MI-TL-SLOW %.6f  EST-MI-PL %.6f  EST-MID-TRAJ  %.6f  EST-END-TRAJ  %.6f  EST-AVG-TC %.6f",
		      _dm->symbol(tr._cid), tr._cid, tr._previousTarget, tr._initPos, tr._targetPos, 
		      miTLSlow.permanentImpact(), miPL.permanentImpact(), midTraj, endTraj, avgTC);    
}

/*
  Choose priority for orders placed with specified components.
  - For liquidity providing components, just return original tlPriority.
  - For liquidity-taking components, run expected transaction costs (for remainder of
    current trade-request on specified stock):
    - With 100% liquidity providing strategy.
    - With liquidity providing strategy PLUS liquidity-taking with under current 
      market conditions (especially market impact).
    - If liquidity-providing only strategy appears better, return some small priority # which
      basically says take liquidity IFF really sure that stock is about to tick-up.
    - If liquidity-taking appears OK, return what would have been returned by
      PriorityComponent::componentPriority - aka tlPriority/2.0 (hack).
   Note:
   - For initial production deployment (May 2010), we cap return priority at
     0.5 * tlPriority.  This effectively constrains trading logic to not be
     more aggressive than original PriorityComponent logic.
   - After we get moire comfortable with underlying models, may want to
     relax this constraint (r.g. change to 0.5 * aSpd) to allow system
     to be more aggressive for high priority orders.
    
*/
double PriorityCutoffComponent::componentPriority(int cid, Mkt::Side side, int tradeLogicId, 
						  int numShares, double tlPriority, 
						  OrderPlacementSuggestion::PlacementReason pr) {
  /* 
     Liquidity-taking 
  */
  if (pr != OrderPlacementSuggestion::CROSS && pr != OrderPlacementSuggestion::TAKE_INVISIBLE) {
    return tlPriority;
  }
  if (pr == OrderPlacementSuggestion::TAKE_INVISIBLE) {
    // Note:  TAKE_INVIS component seems to need work.  Taking priority down to 0.0 bps, aka
    //   use very sparingly.
    return 0.0;
  }

  /*
    Liquidity-providing
  */     

  // Semi-plausible return value if unable to cross.
  double defaultRet = std::min(DEFAULT_PRIORITY, tlPriority/2.0);
    
  //
  // Calculate trailing average spread for stock.  Used for sanity-checking 
  //   priority calculations below.
  //
  double aSpd;
  if (!_spdT->getASpd(cid, aSpd)) {
    return defaultRet;
  }


  //
  // Estimate market-impact of crossing for # of visibly displayed top-level shares.
  // 
  // How large is top-level visible liquidity across all (connected) ECNs on crossSide?
  int crossSize, remainingSize;
  size_t crossSizeT;
  double crossPrice;
  Mkt::Side crossSide =( side == Mkt::BID ? Mkt::ASK : Mkt::BID );
  if (!getMarket(_dm->masterBook(), cid, crossSide, 0, &crossPrice, &crossSizeT)) {
    return defaultRet;
  }
  // For how many shares would we try to cross.
  crossSize = std::min(numShares, (int)crossSizeT);
  remainingSize = numShares - crossSize;
  if (crossSize <= 0) {
    return defaultRet;
  }

  // Estimate market impact, for crossing, at cross price, for crossSize.
  // - Use ISLD as ECN, as this generally has the highest displayed volumes from 
  //   among connected US equity ECNs.  This is somewhat arbitrarily & can probably 
  //   be slightly wrong when crossing to other ECNs, but probably not enough to really matter.
  // - miTL should hold market impact as absoluetly signed quantity (not signed relative
  //   to trade direction), aka a negative # should represent a negative expected return
  //   on the stock, and a positive # should represent a positive expected return on the
  //   stock.
  MarketImpactEstimate miTL;
  if (!_tlMIM->marketImpactFill(cid, ECN::ISLD, crossSize, crossPrice, crossSide, 0, false, miTL)) {
    return defaultRet;
  }

  // Estimate participation rate for 100% liquidity providing strategy only.
  // Note:  Using curtv twice is a hack that assumes that guesstimated participation
  //   rate is invariant during normal market hours.
  double pRate;
  TimeVal curtv = _dm->curtv();
  Mkt::Trade dir = (side == Mkt::BID ?  Mkt::BUY : Mkt::SELL);
  double tsign = (side == Mkt::BID ?  1.0 : -1.0);
  estimatePRate(cid, dir, tlPriority, curtv, curtv, pRate);                       // always >= 0.

  // Estimate expected completion time on trade request - with liquidity providing
  //   strategy only.
  // Also estimate expected completion time for trade-request if crossShares are
  //   taken out now.
  // Estimated completion time is linear in # shs & participation rate.  Therefore,
  //   it can be estimated using the remaining # of shs, and the current time.
  TimeVal endTVPL;
  TimeVal endTVTL;
  if (!estimateCompletionTime(cid, numShares,     pRate, curtv, endTVPL) ||       // end time - 100% PL;
      !estimateCompletionTime(cid, remainingSize, pRate, curtv, endTVTL)) {       // end time - some TL.
    return defaultRet;
  }

  // Estimate LT signal profile on trade request (clean of market impact of our trading).
  // - To end of trade request, with 100% liquidity providing.
  // - To end of trade-request, with mix of liquidity taking & providing.
  // These signal profile estimates should be absolutely signed, not signed relative to
  //   trade direction, aka a positive # should represent a positive expected return,
  //   and a negative # should represent a negative expected return.
  // These signal profiles should be estimated including elapsed time since the 
  //   trade request was received.
  double signalProfilePL;
  double signalProfileTL;
  TradeRequest tr = _rmiTracker->lastTR(cid);
  if (!estimateSignalProfile(cid, dir, tlPriority, tr._recvTime, curtv, endTVPL, signalProfilePL) ||  
      !estimateSignalProfile(cid, dir, tlPriority, tr._recvTime, curtv, endTVTL, signalProfileTL)) {
    return defaultRet;
  }

  // Estimate market-impact of providing liquidity over expected life of trade-request, 
  //   with no crossing.
  // - To end of trade-request, with 100% liquidity providing.
  // - To end of trade-request, with mix of liquidity taking & providing.
  // These market impact estimates should be absolutely signed, not signed relative to
  //   trade direction, aka a positive # should represent a positive expected return,
  //   and a negative # should represent a negative expected return.
  MarketImpactEstimate miPLEnd, miPLPLEnd, miPLTLEnd;
  if (!estimateMarketImpactPL(cid, numShares, dir, pRate, miPLPLEnd) ||
      !estimateMarketImpactPL(cid, remainingSize, dir, pRate, miPLTLEnd)) {
    return defaultRet;
  }
  
  // Estimate expected transaction costs for last crossSize shares, for 100% 
  //   liquidity-providing strategy only.  This cost can be estimated as:
  // - avg signalProfile on last crossSize shares (between endTVPL and endTVTL).  PLUS
  // - avg market impact on last crossSize shares (ditto).  PLUS
  // - some guesstimate of expected TC for liquidity providing strategies 
  //   with no market-impact, on a zero-alpha basket.
  // Note:  avgSignalProfilePL and avgMIPL calculations below assume that
  //   signal profile and MI are realzied linearly in trading volume.  Something
  //   like a sqrt function is probably more realistic, which implies that 
  //   the calculation below slightly *underestimates* transaction costs for 
  //   100% PL strategy.
  double avgSignalProfilePL = (signalProfileTL + signalProfilePL)/2.0;
  double avgMIPL = (miPLPLEnd.permanentImpact() + miPLTLEnd.permanentImpact())/2.0;
  double tcPL = ADVERSE_FILL_PL + (tsign * avgSignalProfilePL) + (tsign * avgMIPL);
  // Should hold expected trsanaction costs per share, for the last crossShares shares,
  //   (EW average across all these shares), in a 100% liquiidty-providing strategy.
  

  // Calculate a penalty for liquidity-taking reflecting the market-impact of taking
  //   liquidity immediately, spread across all of the shares remaining in the trade-request
  //   *after* crossSize shares are taken.
  double penaltyTL = tsign * miTL.permanentImpact() * ((double)remainingSize/(double)crossSize);

  // Calculate maximum priority at which tcPL is <= penaltyPL.
  double maxPriority = tcPL - penaltyTL;

  // Trim at sensible ranges:
  // - Truncate below at DEFAULT_PRIORITY bps .
  // - Truncate above at 50% of trailing average spread.
  double retPriority = std::max(maxPriority, DEFAULT_PRIORITY);     // minimum value - DEFAULT_PRIORITY.
  retPriority = std::min(retPriority, 0.5 * tlPriority);            // maximum value - 0.5 * tlPriority

  /*
    ALERT - ALERT - for debugging only.
  */
  /* Do not log in production
  double fv = 0.0, crossCost = 0.0;
  if (HFUtils::bestMid(_dm.get(), cid, fv, _signal)) {
    crossCost = tsign * (crossPrice - fv)/fv;
  } 
  TAEL_PRINTF(_logPrinter, TAEL_INFO, "%-5s  CID %i  PRI-DCSN  :  numshs %i  sigprofPL %.6f  mktimpPL %.6f  tcPL %.6f  crossshs %i  mktimpTL %.6f  penaltyTL %.6f  maxPri %.6f  retPri %.6f  crossPrice %.4f  fv %.4f  crossCost %.6f",
		      _dm->symbol(cid), cid, numShares, avgSignalProfilePL, avgMIPL, tcPL, 
		      crossSize, miTL.permanentImpact(), penaltyTL, maxPriority, retPriority,
		      crossPrice, fv, crossCost);   
  */

  // And return....
  return retPriority;  
}

/*
  Given an order for <numShares> shares, in stock <cid>, expected to be filled
    at average participation rate <pate>, estimate completion time for order.
  - Can ignore rounding effects, e.g. from tendency to trade round lots, and
    from wait time in queues for liqudiity-providing orders.
*/
bool PriorityCutoffComponent::estimateCompletionTime(int cid, int numShares, double pRate,
						     const TimeVal &curtv, TimeVal &fv) {
  // Estimate average trading volume, per-second.
  double avgVolumePerSecond;
  if (!_volumeT->getVolumeAverage(cid, avgVolumePerSecond)) {
    return false;
  }
  avgVolumePerSecond /= ((double)_volumeT->sampleMilliSeconds())/1000.0;

  // Estimate our share.
  double ourVolumePerSecond = avgVolumePerSecond * pRate;
 
  // Estimate completion time, in seconds.
  double completionSeconds = ((double)numShares)/ourVolumePerSecond;

  // Populate fv with curtv + calculated # of seconds.
  long completionSecondsLong = (long)floor(completionSeconds);
  long completionUSec = (long) (completionSeconds - (double)completionSecondsLong) * 1e6;
  TimeVal addMe(completionSecondsLong, completionUSec);
  fv = curtv;
  fv += addMe;
    
  // Done.
  return true;
}

/*
  Estimate *total* alpha in trade request from start of request to subsequent time.
  Initial version - is a *hack*:
  - We assume that:
    - Specified priority represents 20 minute expected LT signal profile.
    - Expected signal profile grows a square-root function of time
      relative to that 20-minute interval.
  Notes:
  - This should be replaced with a *real* LT alpha profile model.
  - signal profile should be signed quantity, with positive # indicating
    expected positive return, and negative # indicating expected negative
    return.  Aka, should be signed absolutely, not signed in direction of
    trade.
*/
bool PriorityCutoffComponent::estimateSignalProfile(int cid, Mkt::Trade dir, double priority, 
						    const TimeVal &startTV,
						    const TimeVal &endTV, double &fv) {
  double tsign = (dir == Mkt::BUY ? 1.0: -1.0);
  double msDiff = HFUtils::milliSecondsBetween(startTV, endTV);
  double numBlocks = msDiff / (20 * 60 * 1000);
  fv = pow(numBlocks, 0.5) * priority * tsign;
  return true;
}

bool PriorityCutoffComponent::estimateSignalProfile(int cid, Mkt::Trade dir, double priority, 
						    const TimeVal &startTV, const TimeVal &curTV,
						    const TimeVal &endTV, double &fv) {
  double totalAlpha, realizedAlpha;
  if (!estimateSignalProfile(cid, dir, priority, startTV, endTV, totalAlpha) ||
      !estimateSignalProfile(cid, dir, priority, startTV, curTV, realizedAlpha)) {
    fv = 0.0;
    return false;
  } 
  fv = totalAlpha - realizedAlpha;
  return true;
}


/*
  Estimate participation rate - for 100% liquidity provision strategy.
*/
bool PriorityCutoffComponent::estimatePRate(int cid, Mkt::Trade dir, double priority, 
					      const TimeVal &startTV, const TimeVal &endTV,
					      double &fv) {
  return _plPRM->estimateParticipationRate(cid, dir, priority, startTV, endTV, fv);
}


bool PriorityCutoffComponent::estimateMarketImpactPL(int cid, int numShares, Mkt::Trade dir,
						     double pRate, MarketImpactEstimate &fv) {
  int previousShares = _rmiTracker->shsPL(cid);
  if (dir != Mkt::BUY) {
    previousShares *= -1;
  }
  return _plTIM->marketImpactTrading(cid, previousShares, numShares, dir, pRate, fv);
}

bool PriorityCutoffComponent::estimateMarketImpactPL(int cid, int numShares, Mkt::Trade dir,
						     double pRate, vector<MarketImpactEstimate> &fv) {
  int previousShares = _rmiTracker->shsPL(cid);
  if (dir != Mkt::BUY) {
    previousShares *= -1;
  }
  return _plTIM->marketImpactTrading(cid, previousShares, numShares, dir, pRate, fv);
}

/*
  Estimate average transaction costs, per $ traded for the specified request.
  Assumes:
  - Request is for specified stock in specified direction.
  - numShares of request are remaining (total request size >= numShares).
  - Request was received at time startTV with specified priority.
  Notes:
  - Should compute expected transaction costs per $ traded, aka 0.0001 for 1 bps 
    of transaction costs per $ trades.
  - Cost should be a positive # for positive costs, e.g. 0.0001 would be 1 bps of
    cost per $ notional.
  - Cost should be volume-weighted average across all shares specified in request.
  - Cost should include:
    - Signal profile.
    - Market impact.
    - Adverse fill.
    - ECN rebates/fees.
  - Includes a bunch of assumptions about how priority is selected and sub-components
    trade.
*/
bool PriorityCutoffComponent::estimateTransactionCosts(int cid, int additionalSize, Mkt::Trade dir,
						       double priority, 
						       const TimeVal &startTV, const TimeVal &curtv, 
						       double &fv) {
  fv = 0.0;
  double pRate, signalProfile, thisTC = 0.0, totalTC = 0.0;
  double tsign = (dir == Mkt::BUY ?  1.0 : -1.0);

  if (additionalSize <= 0) {
    fv = 0.0;
    return true;
  }

  // Guesstimate participation rate.  Assumed constant over life of order.
  if (!estimatePRate(cid, dir, priority, curtv, curtv, pRate)) {
    return false;
  }

  // Break up order into round-lot sized chunks
  vector<int> chunkSizes;
  HFUtils::chunkOrderShares(additionalSize, 100, chunkSizes);

  // Estimate cumulative market-impact at each chunk.
  // Conceptually, this estimation could be done separately per chunk
  //   in the loop (below), but that leads to an O(N^2) solution.
  vector<MarketImpactEstimate> chunkMIPLV;
  if (!estimateMarketImpactPL(cid, additionalSize, dir, pRate, chunkMIPLV)) {
    return false;
  }

  // Walk through each chunk.
  int sz = chunkSizes.size();
  int thisSize = 0, totalSize = 0;
  MarketImpactEstimate miPL;
  TimeVal chunkEndTV;
  for (int i=0; i<sz; i++) {
    // this chunk size, and total chunk size.
    thisSize = chunkSizes[i];
    totalSize += thisSize;

    // Estimate completion time of ith chunk.
    if (!estimateCompletionTime(cid, totalSize,     pRate, curtv, chunkEndTV)) {
      return false;
    }
    // Estimate signal profile to that completion time.
    if (!estimateSignalProfile(cid, dir, priority, startTV, curtv, chunkEndTV, signalProfile)) {
      return false;
    }

    // Calculated for each chunk, above.
    miPL = chunkMIPLV[i];

    // Estimate transaction costs for ith chunk:
    // - signal profile to completion time.
    // - market impact to completion time.
    // - adverse fill.
    thisTC = ADVERSE_FILL_PL + (tsign * miPL.permanentImpact()) + (tsign * signalProfile);
    // Scale for lot size
    thisTC *= (double)thisSize;
    totalTC += thisTC;
  }

  fv = totalTC / totalSize;
  return true;
}
