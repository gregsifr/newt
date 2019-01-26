#ifndef __LIB3_SUB_ORDER_BOOK_H__
#define __LIB3_SUB_ORDER_BOOK_H__

#include <Client/lib3/bookmanagement/common/OrderBook.h>

using trc::compat::util::TimeVal;

namespace lib3 {

class SubOrderBook : public OrderBook {
    protected:
    MarketMaker mm;

    public:
    SubOrderBook(CIndex &ci, int book_id, TimeVal &clock, bool prom, MarketMaker mm, tael::Logger *dbg) :
      OrderBook(ci, book_id, clock, prom, dbg), mm(mm) {}

    virtual void onAdd(Order *bo, bool done) {
      if (bo->mmid == mm) {
	this->Book<Order>::onAdd(bo, done);
      }
    }

    virtual void onCancel(Order *bo, size_t old_size, bool done) {
      if (bo->mmid == mm) {
	this->Book<Order>::onCancel(bo, old_size, done);
      }
    }

    virtual void onExec(Order *bo, size_t old_size, bool done) {
      if (bo->mmid == mm) {
	this->Book<Order>::onExec(bo, old_size, done);
      }
    }

    virtual void onCxr(Order *bo, size_t old_size, double old_px, bool done) {
      if (bo->mmid == mm) {
	this->Book<Order>::onCxr(bo, old_size, old_px, done);
      }
    }

    virtual void onRemove(Order *bo, size_t old_size, bool done) {
      if (bo->mmid == mm) {
	this->Book<Order>::onRemove(bo, old_size, done);
      }
    }

    virtual void onNoOp(Order *bo) {
      if (bo->mmid == mm) {
	this->Book<Order>::onNoOp(bo);
      }
    }

  };
};

#endif // __LIB3_SUB_ORDER_BOOK_H__
