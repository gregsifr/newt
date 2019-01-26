#include "DataUpdates.h"
#include <cstdio>
#include <cstdlib>
#include <DataManager.h>
#include <Client/lib3/ordermanagement/arca/ArcaTrader.h>

bool operator== ( const DataUpdate &a, const DataUpdate &b ) {
    return a.type == b.type 
        && a.ecn == b.ecn
        && a.side == b.side
        && a.cid == b.cid
        && a.size == b.size
        && a.id == b.id;
}

bool operator!= ( const DataUpdate &a, const DataUpdate &b ) {
    return !(a == b);
}

int TapeUpdate::snprint ( char *s, int n ) const {
    return dm?
        snprintf(s, n, "Tape %010ld.%06ld: %5s %4s %6d $%8.2f",
                tv_.sec(), tv_.usec(), dm->symbol(cid_), Ex::desc(ex_), size_, px_)
    :
        snprintf(s, n, "Tape %010ld.%06ld: #%4d %4s %6d $%8.2f",
                tv_.sec(), tv_.usec(), cid_, Ex::desc(ex_), size_, px_)
    ;
}

int DataUpdate::snprint ( char *s, int n ) const {
    return dm?
        snprintf(s, n, "Data %010ld.%06ld: %5s %5s %4s - %3s %+6d %8.2f [%ld]",
                tv.sec(), tv.usec(), Mkt::DUTypeDesc[type], 
                dm->symbol(cid), ECN::desc(ecn),
                Mkt::SideDesc[side], size, price, id )
        :
        snprintf(s, n, "Data %010ld.%06ld: %5s %5d %4s - %3s %+6d %8.2f [%ld]",
                tv.sec(), tv.usec(), Mkt::DUTypeDesc[type], 
                cid, ECN::desc(ecn),
                Mkt::SideDesc[side], size, price, id )
        ;
}
int OrderUpdate::snprint ( char *s, int n ) const {
    return dm?
        snprintf(s, n, "Order %10ld.%06ld: %5s %4s %4s %4d@%8.2f (%4d/%4d %8.2f) %6s/%7s %4ds %c [%d/E:%ld]",
                tv_.sec(), tv_.usec(), 
		 dm->symbol(cid_), ECN::desc(ecn()),
                Mkt::TradeDesc[dir_], upshs_, uppx_, sharesOpen(), size_, price_, 
                Mkt::OrderActionDesc[action_], Mkt::OrderResultDesc[error_], 
                timeout_, inv_?'I':'V', id_, exid_)
        :
        snprintf(s, n, "Order %10ld.%06ld: #%3d# %4s %4s %4d@%8.2f (%4d/%4d %8.2f) %6s/%7s %4ds %c [%d/E:%ld]",
                tv_.sec(), tv_.usec(), cid_,
		 ECN::desc(ecn()),
                Mkt::TradeDesc[dir_], upshs_, uppx_, sharesOpen(), size_, price_, 
                Mkt::OrderActionDesc[action_], Mkt::OrderResultDesc[error_], 
                timeout_, inv_?'I':'V', id_, exid_)
        ;
}

void OrderUpdate::init ( Order const *o, DataManager *dm_, Mkt::OrderAction act, Mkt::OrderResult err,
        int64_t exid, int upshs, int fillshs, Liq::Liq l, bool mine, int cxlshs, double uppx, TimeVal tv, long orderID ) {
    dm = dm_;
    action_ = act;
    error_ = err;
    exid_ = exid;
    upshs_ = upshs;
    cxlshs_ = cxlshs;
    fillshs_ = fillshs;
    uppx_ = uppx;
    fee_ = fees_ = 0.0;
    tv_ = tv;
    liq_ = l;
    mine_ = mine;
    orderID_ = orderID;

    ecn_ = o->realecn();
    dir_ = o->dir();
    id_ = o->id();
    cid_ = o->cid();
    timeout_ = o->timeout();
    price_ = o->price();
    size_ = o->size();
    inv_ = o->invisible();
    placementAlgo_ = o->placementAlgo;
}

bool Order::isCanceling() const {
  // Is there a cxl-request for the whole open size?
  if( !canceling() || canceling().thisShares()<canceling().sharesOpen() ) return false;
  // From now on we know we already sent a cxl-request for the full open size
  if( !canceled() || canceled().action() != Mkt::CXLREJECTED ) return true;
  // And now we know we got a cxl-rejection. Is it later than the cxl-request?
  if( canceled().tv() > canceling().tv() ) return false;
  return true;
}

Order *Order::allocate ( ) { return lib3::Mempool<Order>::getInstance()->allocate(); }

void Order::init ( lib3::Order const *lo, long orderID, const char* placeAlgo ) {
	if(placeAlgo) {
		placementAlgo.assign(placeAlgo);
	} else {
		placementAlgo.assign("UNKNOWN");
	}
	orderID_ = orderID;
    lo_ = lo;
    id_ = lo->seqnum;
    cid_ = lo->cid;
    timeout_ = lo->timeout;
    price_ = lo->entry_px;
    size_ = lo->entry_size;
    inv_ = isInvisible(lo);
    ecn_ = ECN::mmtoECN[lo->mmid];
    realecn_ = ECN::mmtoECN[lo->mmid];
    dir_ = (Mkt::Trade) lo->dir;
    mine_ = !lo->is_prom;
    last = &plreq;
    poplus_ = false;
    if (ecn_==ECN::ARCA){
        const lib3::ArcaOrder *ao = dynamic_cast<const lib3::ArcaOrder*>(lo);
        if (ao) {
            poplus_ = (ao->routing == 'O');
      }
    }
    plreq.init(this, 0, Mkt::NO_UPDATE, Mkt::GOOD, -1, 0, 0, Liq::other, mine_, 0, 0.0, TimeVal(0, 0), orderID);
    plrsp.init(this, 0, Mkt::NO_UPDATE, Mkt::GOOD, -1, 0, 0, Liq::other, mine_, 0, 0.0, TimeVal(0, 0), orderID);
    cxreq.init(this, 0, Mkt::NO_UPDATE, Mkt::GOOD, -1, 0, 0, Liq::other, mine_, 0, 0.0, TimeVal(0, 0), orderID);
    cxrsp.init(this, 0, Mkt::NO_UPDATE, Mkt::GOOD, -1, 0, 0, Liq::other, mine_, 0, 0.0, TimeVal(0, 0), orderID);
    flrsp.init(this, 0, Mkt::NO_UPDATE, Mkt::GOOD, -1, 0, 0, Liq::other, mine_, 0, 0.0, TimeVal(0, 0), orderID);
}

int Order::snprint ( char *buf, int n ) const {
    return snprintf(buf, n, "Order %11d %4s #%3d# %4s %4s %4d@%8.2f %4d %c",
		    id_, Mkt::OrderStateDesc[state()], cid_, ECN::desc(ecn()), Mkt::TradeDesc[dir_],
            size_, price_, timeout_, inv_? 'I':'V');
}


int UserMessage::snprint ( char *s, int n ) const {
    return snprintf(s, n, "UMsg c:%d s:%d m:\"%.32s\" n:\"%.32s\"", code_, strategy_, msg1_, msg2_);
}


int TimeUpdate::snprint ( char *s, int n ) const {
    if (timer_.isOneoff()) {
        return snprintf(s, n, "Tm Oneoff %10lu.%06lu @%10lu.%06lu\n",
                timer_.oneoff().sec(), timer_.oneoff().usec(), tv_.sec(), tv_.usec());
    } else {
        return snprintf(s, n, "Tm Every %5lu.%06lu +%5lu.%06lu @%10lu.%06lu\n",
                timer_.period().sec(), timer_.period().usec(),
                timer_.phase().sec(), timer_.phase().usec(),
                tv_.sec(), tv_.usec());
    }
}

