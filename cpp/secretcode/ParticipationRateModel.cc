#include "ParticipationRateModel.h"

/*********************************************************************
  ParticipationRateModel code
*********************************************************************/
ParticipationRateModel::ParticipationRateModel() {

}

ParticipationRateModel::~ParticipationRateModel() {

}


/*********************************************************************
  DefaultProvideLiquidityPRM
*********************************************************************/
DefaultProvideLiquidityPRM::DefaultProvideLiquidityPRM() :
  ParticipationRateModel()
{

}
 
DefaultProvideLiquidityPRM::~DefaultProvideLiquidityPRM() {

}

// Guess 2.5 participation rate for liquidity providing strategies only,
// Based on empirical results from live trading.
// As of April 2010.
// Should be updated as order placement strategies are added/updated.
bool DefaultProvideLiquidityPRM::estimateParticipationRate(int cid, Mkt::Trade dir, double priority, 
							   const TimeVal &startTV, const TimeVal &endTV,
							   double &fv) {
  fv = 0.025;    
  return true;
}
