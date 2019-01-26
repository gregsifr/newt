#include "CentralOrderRepo.h"
#include "HFUtils.h"

const int BUF_SIZE = 512;
static char buffer[256];

int CentralOrderRepo::nextPlacerId = 0;

/******************************************************
  CentralOrderRepo code
******************************************************/
CentralOrderRepo::CentralOrderRepo():
  _mapTable()
{
  _dm = factory<DataManager>::find(only::one);
  if( !_dm ) 
    throw std::runtime_error( "Failed to get DataManager from factory (in CentralOrderRepo<OrderRecord>::CentralOrderRepo)" );
  _logPrinter = factory<debug_stream>::get( std::string("trader") );
  _mapTable.resize( _dm->cidsize() );

  // get the queues it's respnsible for, add them to DM
  factory<PlacementsHandler>::pointer placementsHandler = factory<PlacementsHandler>::get( only::one );
  factory<CancelsHandler>::pointer    cancelsHandler =    factory<CancelsHandler>::get( only::one );
  _dm->add_dispatch( placementsHandler.get() );
  _dm->add_dispatch(    cancelsHandler.get() );

  _dm->add_listener( this );
}

void CentralOrderRepo::update( const OrderPlacementSuggestion& placementMsg ) {
  if( addOrderRecord(placementMsg) )
    return;
  // If failed to add record, simply report this suspicious behavior
  TAEL_PRINTF(_logPrinter.get(), TAEL_ERROR, "ERROR: In CentralOrderRepo::update(OrderPlacementSuggestion), failed to add into the repository "
		       "an OrderRecord with Id %d", placementMsg.getOrderId() );
  return;
}

void CentralOrderRepo::update( const OrderCancelSuggestion& cxlMsg ) {
  // I cannot use the code of "getOrderRecord" because it returns a pointer-to-const which does not allow updating the OrderRecord
  int orderId = cxlMsg._orderId;
  const Order* order = _dm->getOrder( orderId );
  if( order == NULL ) {
    TAEL_PRINTF(_logPrinter.get(), TAEL_ERROR, "ERROR: In CentralOrderRepo::update(CancelMessage), can't find the order the cxl- message is "
			 "referring to (orderId=%d)", orderId );
    return;
  }

  int    cid   = order->cid();
  map<int,OrderRecord>::iterator it = _mapTable[cid].find( orderId );
  // I expect to find all orders which show in DM also here in CentralOrderRepo
  if( it == _mapTable[cid].end() ) {
    order -> snprint( buffer, BUF_SIZE );
    TAEL_PRINTF(_logPrinter.get(), TAEL_ERROR, "%-5s ERROR: In CentralOrderRepo::update(CancelMessage), can't find the order the cxl- message is "
			 "referring to: %s", _dm->symbol(cid), buffer );
    return;
  }
  it->second.setCancelReason( cxlMsg._reason );
}

void CentralOrderRepo::update( const OrderUpdate& ou ) {
  
  int cid=ou.cid();
   
  //  if (ou.action()==Mkt::CONFIRMED){
  //  map<int,OrderRecord>::iterator it = _mapTable[cid].find( ou.id() );
  //  OrderRecord* cord = &(it->second);
  //  if (cord==NULL)
  //    return;
  //  cord->setState(new OrderStateRecord(ou));
  //  return;
  //}
  if( ou.state()!=Mkt::DONE )
    return;

  map<int,OrderRecord>::iterator it = _mapTable[ ou.cid() ].find( ou.id() );
  if( it == _mapTable[ou.cid()].end() ) {
    TAEL_PRINTF(_logPrinter.get(), TAEL_WARN, "%-5s WARNING: Couldn't find an order with id=%d in CentralOrderRepo when got a done message",
			 _dm->symbol(ou.cid()), ou.id() );
    return;
  }
  //OrderRecord* cord = &(it->second);
  //if (cord->state() !=NULL) {
  //  delete cord->state();
  //  cord->setState(NULL);
  //}
  _mapTable[ ou.cid() ].erase( it );
}

const OrderRecord* CentralOrderRepo::getOrderRecord( int orderId ) const {
  const Order* order = _dm->getOrder( orderId );
  if( order == NULL ) {
    TAEL_PRINTF(_logPrinter.get(), TAEL_WARN, "WARNING: Couldn't find an order with id=%d in CentralOrderRepo::getOrderRecord", orderId );
    return NULL;
  }
  int cid = order->cid();

  map<int,OrderRecord>::const_iterator it = _mapTable[cid].find( orderId );
  // I expect to find all orders which show in DM also here in CentralOrderRepo
  if( it == _mapTable[cid].end() ) {
    order->snprint( buffer, BUF_SIZE );
    TAEL_PRINTF(_logPrinter.get(), TAEL_ERROR, "%-5s ERROR: CentralOrderRepo::getOrderRecord can't find an order which does appear in DM: %s",
			 _dm->symbol( cid ), buffer );
    return NULL;
  }
  return &(it->second);
}

const OrderRecord* CentralOrderRepo::getOrderRecord( int cid, int orderId ) const {
  map<int,OrderRecord>::const_iterator it = _mapTable[cid].find( orderId );
  // I expect to find all orders which show in DM also here in CentralOrderRepo
  if( it == _mapTable[cid].end() ) {
    TAEL_PRINTF(_logPrinter.get(), TAEL_ERROR, "%-5s ERROR: CentralOrderRepo::getOrderRecord can't find an order that does appear in DM: (id=%d)",
			 _dm->symbol( cid ), orderId );
    return NULL;
  }
  return &(it->second);
}

/// Get the OrderRecords of the specified componentId and tradeLogicId and side
vector<const OrderRecord*> CentralOrderRepo::getOrderRecords( int cid, Mkt::Side side, 
							      int tradeLogicId, int componentId ) const {

  vector<const OrderRecord*> ret;
  map<int,OrderRecord>::const_iterator it;
  for( it=_mapTable[cid].begin(); it!=_mapTable[cid].end(); ++it ) {
    const OrderRecord* ordRec = &(it->second);
    const Order *order = _dm->getOrder( ordRec->orderId() );
    if( ordRec->tradeLogicId()==tradeLogicId && ordRec->componentId()==componentId &&
	order->side() == side )
      ret.push_back( ordRec );
  }
  return ret;
}


/// Get the OrderRecords of the specified componentId and tradeLogicId
vector<const OrderRecord*> CentralOrderRepo::getOrderRecords( int cid, int tradeLogicId, int componentId ) const {

  vector<const OrderRecord*> ret;
  map<int,OrderRecord>::const_iterator it;
  for( it=_mapTable[cid].begin(); it!=_mapTable[cid].end(); ++it ) {
    const OrderRecord* ordRec = &(it->second);
    if( ordRec->tradeLogicId()==tradeLogicId && ordRec->componentId()==componentId )
      ret.push_back( ordRec );
  }
  return ret;
}

/// Get the OrderRecords of the specified componentId 
vector<const OrderRecord*> CentralOrderRepo::getOrderRecords( int cid, int componentId ) const {

  vector<const OrderRecord*> ret;
  map<int,OrderRecord>::const_iterator it;
  for( it=_mapTable[cid].begin(); it!=_mapTable[cid].end(); ++it ) {
    const OrderRecord* ordRec = &(it->second);
    if( ordRec->componentId()==componentId )
      ret.push_back( ordRec );
  }
  return ret;
}

/// Get the OrderRecords of the specified componentId 
vector<const OrderRecord*> CentralOrderRepo::getOrderRecords( int cid ) const {

  vector<const OrderRecord*> ret;
  map<int,OrderRecord>::const_iterator it;
  for( it=_mapTable[cid].begin(); it!=_mapTable[cid].end(); ++it ) {
    const OrderRecord* ordRec = &(it->second);
    ret.push_back( ordRec );
  }
  return ret;
}

bool CentralOrderRepo::addOrderRecord( const OrderPlacementSuggestion& placementMsg ) {
  int orderId = placementMsg.getOrderId();
  const Order* order = _dm -> getOrder( orderId );
  if( order == NULL ) {
    TAEL_PRINTF(_logPrinter.get(), TAEL_ERROR, "ERROR: CentralOrderRepo::addOrderRecord asked to add an OrderRecord with orderId (%d) DM doesn't "
			 "know about", orderId );
    return false;
  }
  OrderRecord ordRec( order, placementMsg );
  int cid = order->cid();

  std::pair<int,OrderRecord> p(orderId, ordRec); // This creates a copy of ordRec inside p, right?
  // std::map.insert returns a pair of <iterator, bool>. If orderId is already a key in the map, there's no insertion and the bool is false
  bool ret = _mapTable[cid].insert(p).second; 
  return ret;
}

int CentralOrderRepo::totalOutstandingSize( int cid, Mkt::Side side, int componentId ) const {
  int totalSize = 0;
  map<int,OrderRecord>::const_iterator it;
  for( it=_mapTable[cid].begin(); it!=_mapTable[cid].end(); ++it) {
    const OrderRecord& ordRec = it->second;
    if( ordRec.componentId() != componentId ) continue;
    int orderId = ordRec.orderId();
    const Order* order = _dm->getOrder( orderId );
    if( order==NULL ) {
      TAEL_PRINTF(_logPrinter.get(), TAEL_ERROR, "%-5s ERROR: In CentralOrderRepo::totalOutstandingSize(,,) encountered an OrderRecord with "
			   "id (%d) DM doesn't know of", _dm->symbol(cid), orderId );
      continue;
    }
    if( order->side() != side ) continue;
    totalSize += order->sharesOpen();
  }
  return totalSize;
}

int CentralOrderRepo::totalOutstandingSize( int cid, Mkt::Side side, int componentId, int tradeLogicId ) const {
  int totalSize = 0;
  map<int,OrderRecord>::const_iterator it;
  for( it=_mapTable[cid].begin(); it!=_mapTable[cid].end(); ++it ) {
    const OrderRecord& ordRec = it->second;
    if( ordRec.componentId() != componentId || ordRec.tradeLogicId() != tradeLogicId ) continue;
    int orderId = ordRec.orderId();
    const Order* order = _dm->getOrder( orderId );
    if( order == NULL ) {
      TAEL_PRINTF(_logPrinter.get(), TAEL_ERROR, "%-5s ERROR: In CentralOrderRepo::totalOutstandingSize(,,,) encountered an OrderRecord with "
			   "id (%d) DM doesn't know of", _dm->symbol(cid), orderId );
      continue;
    }
    if( order->side() != side ) continue;
    totalSize += order->sharesOpen();
  }
  return totalSize;
}

int CentralOrderRepo::totalOutstandingSizeNotCanceling( int cid, Mkt::Side side, int componentId, int tradeLogicId ) const {
  int totalSize = 0;
  map<int,OrderRecord>::const_iterator it;
  for( it=_mapTable[cid].begin(); it!=_mapTable[cid].end(); ++it ) {
    const OrderRecord& ordRec = it->second;
    if( ordRec.componentId() != componentId || ordRec.tradeLogicId() != tradeLogicId ) continue;
    int orderId = ordRec.orderId();
    const Order* order = _dm->getOrder( orderId );
    if( order == NULL ) {
      TAEL_PRINTF(_logPrinter.get(), TAEL_ERROR, "%-5s ERROR: In CentralOrderRepo::totalOutstandingSize(,,,) encountered an OrderRecord with "
			   "id (%d) DM doesn't know of", _dm->symbol(cid), orderId );
      continue;
    }
    if( order->isCanceling() ) continue;
    if( order->side() != side ) continue;
    totalSize += order->sharesOpen();
  }
  return totalSize;
}

int CentralOrderRepo::totalOutstandingSize( int cid, Mkt::Side side, int componentId, 
					    int tradeLogicId, int componentSeqNum ) const {
  int totalSize = 0;
  map<int,OrderRecord>::const_iterator it;
  for( it=_mapTable[cid].begin(); it!=_mapTable[cid].end(); ++it ) {
    const OrderRecord& ordRec = it->second;
    if( ordRec.componentId() != componentId || ordRec.tradeLogicId() != tradeLogicId || 
	ordRec.componentSeqNum() != componentSeqNum) {
      continue;
    }
    int orderId = ordRec.orderId();
    const Order* order = _dm->getOrder( orderId );
    if( order == NULL ) {
      TAEL_PRINTF(_logPrinter.get(), TAEL_ERROR, "%-5s ERROR: In CentralOrderRepo::totalOutstandingSize(,,,) encountered an OrderRecord with "
			   "id (%d) DM doesn't know of", _dm->symbol(cid), orderId );
      continue;
    }
    if( order->side() != side ) continue;
    totalSize += order->sharesOpen();
  }
  return totalSize;
}

/// Return the total size of outstanding orders placed by a particular component and which are more/equally aggressive than a given price
int CentralOrderRepo::totalOutstandingSizeMoreEqAggresiveThan( int cid, Mkt::Side side, double px, int componentId ) const {
  int totalSize = 0;
  map<int,OrderRecord>::const_iterator it;
  for( it=_mapTable[cid].begin(); it!=_mapTable[cid].end(); ++it ) {
    const OrderRecord& ordRec = it->second;
    if( ordRec.componentId() != componentId ) continue;
    int orderId = ordRec.orderId();
    const Order* order = _dm->getOrder( orderId );
    if( order==NULL ) {
      TAEL_PRINTF(_logPrinter.get(), TAEL_ERROR, "%-5s ERROR: In CentralOrderRepo::totalOutstandingSize(,,) encountered an OrderRecord with "
			   "id (%d) DM doesn't know of", _dm->symbol(cid), orderId );
      continue;
    }
    if( order->side() != side ) continue;
    if( HFUtils::lessAggressive(side, order->price(), px) ) continue;
    totalSize += order->sharesOpen();
  }
  return totalSize;
}

