// Write the cost.log file

#ifndef __COST_LOG_WRITER_H__
#define __COST_LOG_WRITER_H__

#include <string>
using std::string;

#include "TradeRequest.h"
#include "DataManager.h"
#include <cl-util/table.h>
#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>

using namespace clite::util;

class CostLogWriter :
  public OrderHandler::listener,
  public TradeRequestsHandler::listener
{

 public:
  CostLogWriter();
  virtual ~CostLogWriter() {}

 protected:
  
  factory<DataManager>::pointer      _dm;
  factory<debug_stream>::pointer     _logPrinter;
  factory<debug_stream>::pointer     _logPrinter2;

  virtual void update( const OrderUpdate& ou );
  virtual void update( const TradeRequest& tradeRequest );


};
#endif // __COST_LOG_WRITER_H__
