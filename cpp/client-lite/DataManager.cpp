#include "DataManager.h"
#include <Common/MktEnums.h>

#include <BookTools.h>
#include <cl-util/debug_stream.h>

#include <Client/lib3/bookmanagement/common/MarketQuoteHandler.h>
#include <Client/lib3/bookmanagement/common/Book.h>
#include <Client/lib3/bookmanagement/common/BookOrder.h>
#include <Client/lib3/bookmanagement/common/BookLevel.h>

#include <Client/lib3/bookmanagement/us/Itch4QuoteHandler.h>
#include <Client/lib3/bookmanagement/us/OBUltraQuoteHandler.h>
#include <Client/lib3/bookmanagement/us/ArcaQuoteHandler.h>
#include <Client/lib3/bookmanagement/us/Bats3QuoteHandler.h>
#include <Client/lib3/bookmanagement/us/EdgaQuoteHandler.h>
#include <Client/lib3/bookmanagement/us/EdgxQuoteHandler.h>
#include <Client/lib3/bookmanagement/us/BX4QuoteHandler.h>

#include <Client/lib3/ordermanagement/arca/ArcaTrader.h>
#include <Client/lib3/ordermanagement/nyse/NyseTrader.h>
#include <Client/lib3/ordermanagement/nasdaq/NasdaqTrader.h>
#include <Client/lib3/ordermanagement/bats/BatsTrader.h>
#include <Client/lib3/ordermanagement/boston/BostonTrader.h>
#include <Client/lib3/ordermanagement/edge/EdgxTrader.h>
#include <Client/lib3/ordermanagement/edge/EdgaTrader.h>
#include <Client/lib3/ordermanagement/common/OrderManager.h>
#include <Client/lib3/simulation/us/ArcaSimMarket.h>
#include <Client/lib3/simulation/us/BatsSimMarket.h>
#include <Client/lib3/simulation/us/BostonSimMarket.h>
#include <Client/lib3/simulation/us/EdgeSimMarket.h>
#include <Client/lib3/simulation/us/NasdaqSimMarket.h>
#include <Client/lib3/simulation/us/NyseSimMarket.h>

#include <Client/lib3/bookmanagement/us/BX4QuoteHandler.h>
#include <Client/lib3/bookmanagement/us/Itch4QuoteHandler.h>
#include <Client/lib3/bookmanagement/us/OBUltraQuoteHandler.h>
#include <Client/lib3/bookmanagement/us/EdgaQuoteHandler.h>
#include <Client/lib3/bookmanagement/us/EdgxQuoteHandler.h>
#include <Client/lib3/bookmanagement/us/Bats3QuoteHandler.h>
//#include <Client/lib3/bookmanagement/us/AmexUltraQuoteHandler.h>
#include <Common/TowerNet.h>
#include <Common/TowerPort.h>
#include <Client/lib2/TimedAggrLiveSource.h>
#include <Client/lib2/AggrDataFileSource.h>
#include <Client/lib2/LiveSource.h>

#include <boost/algorithm/string.hpp>
#include <cstdio>
#include <cstdlib>

using std::make_pair;
using std::vector;
using std::string;

using namespace clite;

using trc::compat::util::DateTime;

using holiday2::HDate;
using holiday2::HolidaySet;

int DataManager::OnEvent ( Event *e ) 
{ 
    checkTimes();
    
    ISLDEvent           *islde;
    Itch4Event          *i4e;
    NYSETradeEvent      *nte;
    UserEvent           *ue;
    SASEvent            *sae;
    TickEvent           *tick;
    Bats3Event          *bats3;
    ArcaTradeEvent      *at;
    
    switch (e->msgtype) {

        case TRADE_TICK:
        case SIAC_TICK:
            tick = dynamic_cast<TickEvent *>(e);
            if (tick && ci_[tick->Symbol()] != -1) {
                TapeUpdate tu(this, Ex::charToEx(tick->Exchange()), 
                        ci_[tick->Symbol()], tick->Size(), curtv(), tick->Px());
                th.send(tu);
            }
	    break;

        case BATS3_EXC:
        case BATS3_HDN:
            bats3 = dynamic_cast<Bats3Event *>(e);
            if (bats3 && ci_[bats3->Symbol()] != -1) {
                DataUpdate du(this);
                buildImpTick(du, ci_[bats3->Symbol()], bats3->Px(), bats3->ChgSize(), ECN::BATS, bats3->Timestamp());
                du.side = bats3->Type() == 0? Mkt::BID : Mkt::ASK;
                du.id = bats3->SeqNum();
                if (e->msgtype == BATS3_HDN) du.type = Mkt::INVTRADE;
                mh.send(du);
                break;
            }

        case ARCA_TRADE:
        	// GVNOTE: This definitely looks wrong. Why are we setting the type & side to
        	// VISTRADE and BID? Should also remove the buildImpTick function.
        	// GVNOTE: Looks like this was intentional?! See commit comment for 2009-08-05.
            at = dynamic_cast<ArcaTradeEvent *>(e);
            if (at && ci_[at->Symbol()] != -1) {
                DataUpdate du(this);
                buildImpTick(du, ci_[at->Symbol()], at->Px(), at->Size(), ECN::ARCA, at->Timestamp());
                du.type = Mkt::VISTRADE;
                du.side = Mkt::BID;
                du.id = 0;
                mh.send(du);
                break;
            }

        case ISLD_EXECUTED:
        case ISLD_HIDDENEXEC:
        case OPENVIEW_EXECUTED:
        case OPENVIEW_HIDDENEXEC:
        case ITCH3_EXEC:
        case ITCH3_CROSSEXEC:
        case ITCH3_HIDDENEXEC:
        case ITCH4_EXEC:
        case ITCH4_CROSSEXEC:
        case ITCH4_HIDDENEXEC:
            islde  = dynamic_cast<ISLDEvent *>(e);
            if (islde && ci_[islde->Symbol()] != -1 && islde->Type() < 2  ) {

                DataUpdate du(this);
                du.price = islde->Px();
                du.size = islde->Size();
                du.cid = ci_[islde->Symbol()];
                du.side = Mkt::Side(islde->Type());
                du.id = islde->RefNum();
                du.ecn = ECN::ISLD;

                du.tv = midnight_;
                du.tv.sec()  +=  islde->i2qs->i_msecs / 1000;
                du.tv.usec() += (islde->i2qs->i_msecs % 1000) * 1000;
                if (e->msgtype == ISLD_HIDDENEXEC 
                        || e->msgtype ==ITCH3_HIDDENEXEC 
                        || e->msgtype ==ITCH4_HIDDENEXEC 
                        || e->msgtype == OPENVIEW_HIDDENEXEC
                        )
                    du.type = Mkt::INVTRADE;
                else 
                    du.type = Mkt::VISTRADE;
                mh.send(du);
                break;
            }

        case ITCH4F_EXEC:
        case ITCH4F_CROSSEXEC:
        case ITCH4F_HIDDENEXEC:
            i4e = dynamic_cast<Itch4Event *>(e);
            if (i4e && ci_[i4e->Symbol()] != -1 && i4e->Type() < 2) {
                DataUpdate du(this);
                du.price = i4e->Px();
                du.size = i4e->Size();
                du.cid = ci_[i4e->Symbol()];
                du.side = Mkt::Side(i4e->Type());
                du.id = i4e->RefNum();
                du.ecn = ECN::ISLD;

                du.tv = i4e->Timestamp();
                if ( e->msgtype ==ITCH4_HIDDENEXEC 
                    || e->msgtype ==ITCH4F_HIDDENEXEC 
                   )
                    du.type = Mkt::INVTRADE;
                else 
                    du.type = Mkt::VISTRADE;
                mh.send(du);
                break;
            }

        case BX4_EXEC:
        case BX4_CROSSEXEC:
        case BX4_HIDDENEXEC:
        	// BSX uses Itch4.0, hence we create an Itch4Event
            i4e = dynamic_cast<Itch4Event *>(e);
            if (i4e && ci_[i4e->Symbol()] != -1 && i4e->Type() < 2) {
                DataUpdate du(this);
                du.price = i4e->Px();
                du.size = i4e->Size();
                du.cid = ci_[i4e->Symbol()];
                du.side = Mkt::Side(i4e->Type());
                du.id = i4e->RefNum();
                du.ecn = ECN::BSX;

                du.tv = i4e->Timestamp();
                if ( e->msgtype ==BX4_HIDDENEXEC )
                    du.type = Mkt::INVTRADE;
                else
                    du.type = Mkt::VISTRADE;
                mh.send(du);
                break;
            }

        case NYSE_TRADE:
            nte = dynamic_cast<NYSETradeEvent *>(e);
            if (nte && ci_[nte->Symbol()] != -1 ) {
                DataUpdate du(this);
                du.type = Mkt::VISTRADE;
                du.price = nte->Px();
                du.size = nte->Size();
                du.cid = ci_[nte->Symbol()];
                du.side = Mkt::BID;
                du.ecn = ECN::NYSE;
                du.tv = nte->Timestamp();
                mh.send(du);
                break;
            }

        case USER_MESSAGE:
            ue = dynamic_cast<UserEvent *>(e);
            if (ue) {
                UserMessage um(ue->Msg1(), ue->Msg2(), ue->Code(), ue->Strategy(), e->livetv);
                umh.send(um);
                break;
            }

        case SERVER_ALERT_MESSAGE:
        	// GVNOTE: Deal with Server Alert Messages later. Right now, just print as a critical
        	// error, so that we get an email about it.
            sae = dynamic_cast<SASEvent *>(e);
            if (sae) {
            	TAEL_PRINTF(dbg.get(), TAEL_CRITICAL, "Server Alert Message. Code: %d Message: %s",
            			    sae->Code(), sae->Msg());
                break;
            }

        default:
            break;
    }

    deliver();
    return 0;
}

void DataManager::addTimer ( Timer t ) {
    timerset::iterator it = timers.find(t);
    if (it == timers.end()) {
        timers.insert(t);
        nexttimes.push(TimeUpdate(t, t.nextAfter(curtv())));
    }
}

void DataManager::checkTimes() {
    if (nexttimes.empty()) return;
    TimeVal now = curtv();
    
    TimeVal nexttv;
    do {
        TimeUpdate const &next = nexttimes.top();
        nexttv = next.tv();
        if (nexttv <= now) {
            if (!next.timer().isOneoff()) {
                nexttimes.push(TimeUpdate(next.timer(), next.timer().nextAfter(nexttv)));
            }
            tmh.send(next);
            nexttimes.pop();
        }
    } while (nexttv <= now && !nexttimes.empty());
}

// GVNOTE: Should remove this function, as it is used in just 2 places, and there also it seems that
// we partially overwrite what this function does.
void DataManager::buildImpTick ( DataUpdate &d, int cid, double px, int sz, ECN::ECN ecn, TimeVal tv ) {
    int asz = getMarketSize(subBook(ecn), cid, Mkt::ASK, px);
    int bsz = getMarketSize(subBook(ecn), cid, Mkt::BID, px);
    Mkt::Side s;
    Mkt::DUType ty;
    if (asz != 0 && bsz == 0) {
        ty = Mkt::VISTRADE; s = Mkt::ASK;
    } else if (asz == 0 && bsz != 0) {
        ty = Mkt::VISTRADE; s = Mkt::BID;
    } else if (asz == 0 && bsz == 0) {
        ty = Mkt::INVTRADE; s = Mkt::BID;
    } else {
        TAEL_PRINTF(dbg.get(), TAEL_WARN, "DM::buildImpTick: both sizes nonzero cid %d @%f", cid, px);
        ty = Mkt::VISTRADE; s = Mkt::BID;
    }
    
    d.cid = cid;
    d.type = ty;
    d.size = sz;
    d.price = px;
    d.ecn = ecn;
    d.side = s;
    d.tv = tv;
}

void DataManager::onBookChange ( int book_id, lib3::BookOrder *bo, lib3::QuoteReason qr, int delta, bool done )
{
    checkTimes();
    DataUpdate du(this);
    du.cid = bo->cid;
    du.ecn = ECN::mmtoECN[bo->mm];
    if (du.ecn == ECN::UNKN) {
        TAEL_PRINTF(dbg.get(), TAEL_WARN, "Unknown MarketMaker %d", bo->mm);
    }

    du.type = Mkt::BOOK;
    du.size = delta;
    du.id = bo->refnum;
    du.side = Mkt::Side(bo->dir);
    du.price = bo->px;
    du.tv = bo->update_time;
    du.addtv = bo->add_time;

    mh.send(du);

    deliver();
}

void DataManager::CIChange ( CIndex *which ) {
    // cindices only get larger... or else.
}

// GVNOTE: Revisit this. Why don't we just look at position - size? Maybe because we
// should account for short positions based on trades done during the day?
bool DataManager::isShort ( int cid, int sz ) {
    return ((int)position(cid) - (int)(getMarketCumSize(orderBook(), cid, Mkt::ASK) + sz)) < 0;
}

// GVNOTE: Fix this. Assumes ARCA does not have invisible orders.
bool isInvisible ( lib3::Order const *o ) {
    using namespace lib3;
    switch (o->server_id) {
        case BATS_SERVER:
            return ((BatsOrder *)o)->invisible;
        case BOSTON_SERVER:
            return ((BostonOrder *)o)->invisible;
        case NASDAQ_SERVER:
            return (((NasdaqOrder *)o)->display == NasdaqOrder::INVISIBLE);
        default:
            return false;
    }
}

// GVNOTE: This function does not return the lib3::Order. Instead, it returns the
// custom_plugin, which is of type Order (defined in DataUpdates.h). Should probably
// switch to using lib3::Order directly, and return it here.
Order const *DataManager::getOrder ( int id ) {
    char const *acct = colo_accts[seqnum_ecn(id)].c_str();
    if (acct == 0) {
        TAEL_PRINTF(dbg.get(), TAEL_ERROR, "ID %d -> ECN %s has no account name!", id, ECN::desc(seqnum_ecn(id)));
        return 0;
    }
    // GVNOTE: Suppressing warnings for closed and unknown orders right now, since it would
    // clog dataman.log. Should change this back - i.e. remove 'true' below.
    lib3::Order *lo = om->get_order(acct, id, true);
    return  lo && lo->custom_plugin? (Order *)lo->custom_plugin:0;
}

Mkt::OrderResult DataManager::placeOrder ( int cid, ECN::ECN ecn, int size, double price,
        Mkt::Side dir, int timeout, bool invisible, int *seq , long clientOrderID, Mkt::Marking marking, const char* placementAlgo) {

	TAEL_PRINTF(dbg.get(), TAEL_INFO, "came in place order. will try to place order for %s on %s for %d",
			    symbol(cid), ECN::desc(ecn), ((dir == Mkt::BID) ? size: -1*size));
	//GVNOTE: Expand the Mkt::OrderResult enum to support more specific reject reasons
	if (stops[cid]) {
		TAEL_PRINTF(dbg.get(), TAEL_ERROR, "DataManager: trying to place order for halted symbol %s", symbol(cid));
		return Mkt::NO_ROUTE;
	}

    if (listen_only) {
    	TAEL_PRINTF(dbg.get(), TAEL_ERROR, "placeOrder called in listen_only mode");
    	return Mkt::NO_ROUTE;
    }
    if (!om || ecn == ECN::UNKN) {
    	if (!om) TAEL_PRINTF(dbg.get(), TAEL_ERROR, "returning from placeOrder without "
    			"contacting TS since om is null");
    	else TAEL_PRINTF(dbg.get(), TAEL_ERROR, "returning from placeOrder without "
    			"contacting TS since ecn is ECN::UNKN");
    	return Mkt::NO_REASON;
    }

    TradeType trade;
    if (dir == Mkt::BID) { trade = BUY; } 
    else { 
      if (marking==Mkt::LONG_SALE) trade=SELL;
      else if (marking==Mkt::SHORT_SALE) trade=SELL_SHORT;
      else if (isShort(cid, size)) { trade = SELL_SHORT; } 
      else { trade = SELL; }
    }
    
    if (tradesys == COLO && trade == SELL_SHORT && size > (locates(cid) - 100)) {
    	TAEL_PRINTF(dbg.get(), TAEL_ERROR, "returning from placeOrder without contacting TS "
    			   "since we are trying to short more than available locates (or are within "
    			   "100 shares of the limit) for %s", symbol(cid));
        return Mkt::UNSHORTABLE;
    }

    MarketMaker mm = ecnToMM[ecn];
    RejectReasons reason = RejectReasons(-1);

    lib3::Order *lo = 0;
    Order *o = Order::allocate();

    char routing = '\0';
    if (usepoplus && ecn == ECN::NYSE) {
      // Use PO+ routing in ARCA instead, since it has lower fee
      routing = 'O';
      ecn = ECN::ARCA;
    }
    int32_t newseq = next_seqnum(ecn);

    lib3::OrderType orderType = lib3::LIMIT;
    if (MOCmode) orderType = lib3::MKT_ON_CLOSE;

    int sendTimeout = 0;
    if (timeout > 0) {
    	sendTimeout = TIF_DAY;
    }
    if (MOCmode) sendTimeout = 0;

    if (tradesys == COLO || tradesys == SIMTRADE) {
        if (colo_accts[ecn].empty()) {
            TAEL_PRINTF(dbg.get(), TAEL_ERROR, "placeOrder: %s colo account name not set. Not sending to TS.", ECN::desc(ecn));
            return Mkt::NO_ROUTE;
        }

        lib3::NasdaqOrder *no = 0;
        lib3::ArcaOrder   *ao = 0;
        lib3::BatsOrder   *bo = 0;
        lib3::BostonOrder *so = 0;
        lib3::NyseOrder   *yo = 0;
        lib3::EdgaOrder   *eo = 0;
        lib3::EdgxOrder   *xo = 0;
        switch (ecn) {

            case ECN::ISLD:
                no = lib3::NasdaqOrder::allocate();
                no->init(newseq, colo_accts[ECN::ISLD].c_str(), cid, symbol(cid),
                        orderType, trade, price, size, sendTimeout, curtv(), o, invisible);
                lo = no;
                break;

            case ECN::BATS:
                bo = lib3::BatsOrder::allocate();
                bo->init(newseq, colo_accts[ECN::BATS].c_str(), cid, symbol(cid), 
                        orderType, trade, price, size, sendTimeout, curtv(), o, routing,
                        invisible);
                lo = bo;
                break;

            case ECN::BSX:
                so = lib3::BostonOrder::allocate();
                so->init(newseq, colo_accts[ECN::BSX].c_str(), cid, symbol(cid),
                        orderType, trade, price, size, sendTimeout, curtv(), o, routing,
                        invisible);
                lo = so;
                break;

            case ECN::ARCA:
                ao = lib3::ArcaOrder::allocate();
                ao->init(newseq, colo_accts[ECN::ARCA].c_str(), cid, symbol(cid),
                        orderType, trade, price, size, sendTimeout, curtv(), o, routing);
                lo = ao;
                break;
            
            case ECN::NYSE:
                yo = lib3::NyseOrder::allocate();
                yo->init(newseq, colo_accts[ECN::NYSE].c_str(), cid, symbol(cid),
                        orderType, trade, price, size, sendTimeout, curtv(), o);
                lo = yo;
                break;
		
            case ECN::EDGA:
                eo = lib3::EdgaOrder::allocate();
                eo->init(newseq, colo_accts[ECN::EDGA].c_str(), cid, symbol(cid),
                        orderType, trade, price, size, sendTimeout, curtv(), o);
                lo = eo;
                break;

            case ECN::EDGX:
                xo = lib3::EdgxOrder::allocate();
                xo->init(newseq, colo_accts[ECN::EDGX].c_str(), cid, symbol(cid),
                        orderType, trade, price, size, sendTimeout, curtv(), o);
                lo = xo;
                break;
		
            default:
                TAEL_PRINTF(dbg.get(), TAEL_ERROR, "placeOrder: %s colo not available. Not sending to TS.", ECN::desc(ecn));
                return Mkt::NO_ROUTE;
        }

    } else {
        TAEL_PRINTF(dbg.get(), TAEL_ERROR, "placeOrder: tradesys neither COLO nor SIMTRADE. Not sending to TS.");
        return Mkt::NO_ROUTE;
    }
    if (!om->trade(lo, &reason)) {
        return Mkt::reasonToResult(reason);
    }

    if(placementAlgo) {
		o->init(lo, clientOrderID, placementAlgo);
    } else {
    	o->init(lo, clientOrderID, "UNKNOWN");
    }
    TAEL_PRINTF(&reportlog, TAEL_INFO, "%s %s:%d %s %ld TS seqnum: %d", ci_[cid], placementAlgo, ((trade == BUY)? size: -1*size), ECN::desc(ecn), clientOrderID, newseq);
    o->plreq.init(o, this, Mkt::PLACING, Mkt::GOOD, -1, size, 0, Liq::other, true, 0, price, curtv(), clientOrderID);
    o->last = &(o->plreq);
    oh.send(o->plreq);
    if (seq) *seq = newseq;
    return Mkt::GOOD;
}

// GVNOTE: Only needed for the CrossTargetTrader i.e. when we want to trade manually.
// Should perhaps remove this if we are no longer going to use CrossTargetTrader?
Mkt::OrderResult DataManager::placeBatsOrder ( int cid, int size, double price,
        Mkt::Side dir, BatsRouteMod routing, int *seq, long clientOrderID, Mkt::Marking marking ) {
    if (listen_only) return Mkt::NO_ROUTE;
    if (!om) return Mkt::NO_REASON;

    RejectReasons reason = RejectReasons(-1);
    TradeType trade;
    if (dir == Mkt::BID) trade = BUY;
    else{
      if (marking==Mkt::LONG_SALE) trade=SELL;
      else if (marking==Mkt::SHORT_SALE) trade=SELL_SHORT;
      else if (isShort(cid, size)) { trade = SELL_SHORT; } 
      else { trade = SELL; }
    }

    if (tradesys == COLO && trade == SELL_SHORT && size > locates(cid)) {
        return Mkt::UNSHORTABLE;
    }

    lib3::Order *lo = 0;
    Order *o = Order::allocate();
    int32_t newseq = next_seqnum(ECN::BATS);
    lib3::OrderType orderType = lib3::LIMIT;
    if (MOCmode) orderType = lib3::MKT_ON_CLOSE;

    if (tradesys == SIMTRADE || tradesys == COLO) {
        if (colo_accts[ECN::BATS].empty()) return Mkt::NO_ROUTE;

        lib3::BatsOrder *bo = lib3::BatsOrder::allocate();
        
        // GVNOTE: The timeout is set to 0, which means its sent as an IOC order.
        // Do we really want this?
        bo->init(newseq, colo_accts[ECN::BATS].c_str(), cid, symbol(cid), 
                orderType, trade, price, size, 0, curtv(), o,
                (char) routing,
                false, // invisible
                false, // postonly
                false, // no_dart
                false // iso
                );
        lo = bo;
    } else {
        return Mkt::NO_ROUTE;
    }

    o->init(lo, clientOrderID, "UNKNOWN");

    if (!om->trade(lo, &reason)) {
        return Mkt::reasonToResult(reason);
    }

    o->plreq.init(o, this, Mkt::PLACING, Mkt::GOOD, -1, size, 0, Liq::other, true, 0, price, curtv(), clientOrderID);
    o->last = &(o->plreq);
    oh.send(o->plreq);
    if (seq) *seq = newseq;
    return Mkt::GOOD;
}



/*
Mkt::OrderResult DataManager::checkOrder ( int cid, ECN::ECN ecn, int size, double
        price, Mkt::Trade dir, int timeout, bool invisible, double *fees ) {
    //XXX: stub
    return Mkt::GOOD;
}
*/

bool DataManager::cancelOrder ( const Order *o ) {
    return cancelOrder(o->id());
}

void DataManager::cancelMarket ( int cid ) {
  cancelMarket( cid, Mkt::BID );
  cancelMarket( cid, Mkt::ASK );
}    

void DataManager::cancelMarket ( int cid, Mkt::Side side ) {
    Canceler c(this);
    reduceMarketCum(c, 0, orderBook(), cid, side);
}
void DataManager::cancelMarket ( int cid, Mkt::Side side, double px ) {
    Canceler c(this);
    reduceMarket(c, 0, orderBook(), cid, side, px);
}
void DataManager::cancelMarket ( int cid, Mkt::Side side, int l ) {
    Canceler c(this);
    reduceMarket(c, 0, orderBook(), cid, side, l);
}
void DataManager::cancelMarket ( int cid, ECN::ECN ecn ) {
  cancelMarket( cid, ecn, Mkt::BID );
  cancelMarket( cid, ecn, Mkt::ASK );
}
void DataManager::cancelMarket ( int cid, ECN::ECN ecn, Mkt::Side side ) {
    Canceler c(this);
    reduceMarketCum(c, 0, subOrderBook(ecn), cid, side);
}
void DataManager::cancelMarket ( int cid, ECN::ECN ecn, Mkt::Side side, double px ) {
    Canceler c(this);
    reduceMarket(c, 0, subOrderBook(ecn), cid, side, px);
}
void DataManager::cancelMarket ( int cid, ECN::ECN ecn, Mkt::Side side, int l ) {
    Canceler c(this);
    reduceMarket(c, 0, subOrderBook(ecn), cid, side, l);
}

bool DataManager::cancelOrder ( int id ) {
    if (listen_only) return false;
    char const *acct = colo_accts[seqnum_ecn(id)].c_str();
    if (acct == 0) {
        TAEL_PRINTF(dbg.get(), TAEL_ERROR, "ID %d -> ECN %s has no account name!", id, ECN::desc(seqnum_ecn(id)));
        return false;
    }
    // GVNOTE: Suppressing warnings for closed and unknown orders right now, since it would
    // clog dataman.log. Should change this back - i.e. remove 'true' below.
    // GVNOTE: Perhaps we should first check order's status in our internal order map, and
    // only then try to get/cancel it.
    lib3::Order *lo = om->get_order(acct, id, true);
    Order *o = (Order *) (lo? lo->custom_plugin:0);
    if (o == 0) {
    	TAEL_PRINTF(dbg.get(), TAEL_CRITICAL, "In cancelOrder, o = 0");
    }
    bool didcancel = false;
    if (o->action() != Mkt::CANCELING) {
        didcancel = om->cancel(acct, id);
        if (o && didcancel) {
            o->cxreq.init(o, this, Mkt::CANCELING, Mkt::GOOD,
                    o->lastUpdate().exchangeID(),
                    o->lastUpdate().sharesOpen(), //updating shares
                    o->lastUpdate().sharesFilled(), //shares filled
                    o->lastUpdate().liq(), !lo->is_prom,
                    o->lastUpdate().sharesCanceled(), //shares canceled
                    o->confirmed().thisPrice(), curtv(), o->orderID());
            o->last = &(o->cxreq);
            oh.send(o->cxreq);
        }
    }
    return didcancel;
}

void DataManager::cancelAll ( ) {
    om->cancel_all();

    // GVNOTE: Fix this function by doing the following:
    // XXX: loop through om->book to send cancel dispatch updates for all orders.
}

// GVNOTE: Make getPos(async) and getPos(cid, async) consistent. Right now one of
// them uses BSX and one of them checks if the margin server is configured.

// GVNOTE: XXX: We request positions from the trade servers, as well as from the margin server.
// We should probably have a check in here to make sure that the SOD position + current position
// on the trade servers is not very different from the margin/position server position.
bool DataManager::getPos ( bool async ) {
    if (!initialized_) {
        fprintf(stderr, "Get positions before initialization!");
        return false;
    }

    bool res = true;
    /* // GVNOTE: Since we query the position server below, we do not need to query the
     * // individual trade servers
    string const &arca = colo_accts[ECN::ARCA];
    string const &nyse = colo_accts[ECN::NYSE];
    string const &isld = colo_accts[ECN::ISLD];
    string const &bats = colo_accts[ECN::BATS];
    switch (tradesys) {
        case COLO:
            if (!arca.empty())
                res &= om->request_positions(lib3::ARCA_SERVER, arca.c_str(), async);
            if (!bats.empty())
                res &= om->request_positions(lib3::BATS_SERVER, bats.c_str(), async);
            if (!nyse.empty())
                res &= om->request_positions(lib3::NYSE_SERVER, nyse.c_str(), async);
            if (!isld.empty())
                res &= om->request_positions(lib3::NASDAQ_SERVER, isld.c_str(), async);
            break;
        case NONE:
        case SIMTRADE:
            return true;
    }
*/
//    if (configured("margin-server"))
//        return res && om->request_global_positions(async);

    //if (tradesys == DataManager::SIMTRADE) return true;

    if (configured("position-server")) {
    	return res && ps->request_all_positions("???????", om);
    }
    else return res;
}

bool DataManager::getPos ( int cid, bool async ) {
    if (!initialized_) {
        fprintf(stderr, "Get positions before initialization!");
        return false;
    }

    bool res = true;
    /* // GVNOTE: Since we query the position server below, we do not need to query the
     * // individual trade servers
    string const &arca = colo_accts[ECN::ARCA];
    string const &nyse = colo_accts[ECN::NYSE];
    string const &isld = colo_accts[ECN::ISLD];
    string const &bats = colo_accts[ECN::BATS];
    string const &bost = colo_accts[ECN::BSX];
    switch (tradesys) {
        case COLO:
            if (!arca.empty()) 
                res &= om->request_position(lib3::ARCA_SERVER, arca.c_str(), cid, async);
            if (!bats.empty())
                res &= om->request_position(lib3::BATS_SERVER, bats.c_str(), cid, async);
            if (!nyse.empty())
                res &= om->request_position(lib3::NYSE_SERVER, nyse.c_str(), cid, async);
            if (!isld.empty())
                res &= om->request_position(lib3::NASDAQ_SERVER, isld.c_str(), cid, async);
            if (!bost.empty())
                res &= om->request_position(lib3::BOSTON_SERVER, bost.c_str(), cid, async);
            break;
        case NONE:
        case SIMTRADE:
            return true;
    }
*/
//    if (configured("margin-server"))
//        return res && om->request_global_position(cid, async);
    if (configured("position-server")) {
        return res && ps->request_position("???????", ci_[cid], om);
    }
    else return res;
}

bool DataManager::getPositionsIncr ( int pos, int end_pos , bool* done ) {
  int end = std::min(end_pos, cidsize());
  bool ret = true;
  for (int i = pos; i < end; i++) {
    ret &= getPositionAsync(i);
  }

  if (done) {
    *done = (end_pos >= end);
  }

  return ret;
}

void DataManager::onFill ( lib3::Order *lo, lib3::FillDetails *fd ) {
    Order *o = (Order *) lo->custom_plugin;
    if (!o) {
        lo->custom_plugin = o = Order::allocate();
        o->init(lo, -1, "UNKNOWN");
    }
    // GVNOTE: Should send fd->fill_time instead of curtv(). But should also store curtv()
    // somewhere ... perhaps add a field in OrderUpdate?
    // Also, there's no way we can set the ECN for the update to fd->realmm. Should add
    // support for this, especially if we are using PO+ orders etc.
    // Create a fills file which has a format similar to the fills file on asetrade1.jc
    o->setRealEcn(fd->real_mm);
    o->flrsp.init(o, this, Mkt::FILLED, Mkt::GOOD, o->last->exchangeID(), fd->fill_size,
            o->last->sharesFilled() + fd->fill_size, Liq::fromHyp2(fd->liquidity), !lo->is_prom,
            o->last->sharesCanceled(),
            fd->fpx, curtv(), o->orderID());
    o->last = &(o->flrsp);
    oh.send(o->flrsp);
}

void DataManager::onCancel ( lib3::Order *lo, lib3::CancelDetails *cd ) {
    Order *o = (Order *) lo->custom_plugin;
    if (!o) {
        lo->custom_plugin = o = Order::allocate();
        o->init(lo, -1, "UNKNOWN");
    }
    o->cxrsp.init(o, this, Mkt::CANCELED, Mkt::GOOD, o->last->exchangeID(), cd->cancel_size,
            o->last->sharesFilled(), o->last->liq(), !lo->is_prom,
            o->last->sharesCanceled() + cd->cancel_size,
            lo->confirmed_px, curtv(), o->orderID());
    o->last = &(o->cxrsp);
    oh.send(o->cxrsp);
}

void DataManager::onConfirm ( lib3::Order *lo ) {
    Order *o = (Order *) lo->custom_plugin;
    if (!o) {
        lo->custom_plugin = o = Order::allocate();
        o->init(lo, -1, "UNKNOWN");
    }
    o->plrsp.init(o, this, Mkt::CONFIRMED, Mkt::GOOD, 
            lo->confirm_details->exchange_id, 
            lo->confirm_details->confirmed_size,
            0, Liq::other, !lo->is_prom, 0,
            lo->confirm_details->confirmed_px, curtv(), o->orderID());
    o->last = &(o->plrsp);
    oh.send(o->plrsp);
}

void DataManager::onReject ( lib3::Order *lo, lib3::RejectDetails *rd ) {
    Order *o = (Order *) lo->custom_plugin;
    if (!o) {
        lo->custom_plugin = o = Order::allocate();
        o->init(lo, -1, "UNKNOWN");
    }
    o->plrsp.init(o, this, Mkt::REJECTED, Mkt::reasonToResult(rd->reason), -1, 
            o->size_,
            0, Liq::other, !lo->is_prom, o->size_,
            o->price_, curtv(), o->orderID());
	if (!lo->is_prom) {
		numRejects++;
		if(numRejects > MAX_ALLOWED_REJECTS) {
			TAEL_PRINTF(dbg.get(), TAEL_CRITICAL, "Getting too many rejects. Symbol: %s, OrderID: %ld, "
					"seqnum = %d, ECN = %s, Reason: %s", ci_[o->cid()], o->orderID(), o->id(),
					ECN::desc(o->realecn()), Mkt::OrderResultDesc[Mkt::reasonToResult(rd->reason)]);
		}
	}
    o->last = &(o->plrsp);
    oh.send(o->plrsp);
}

void DataManager::onCancelReject ( lib3::Order *lo, lib3::CancelRejectDetails *cd ) {
    Order *o = (Order *) lo->custom_plugin;
    if (!o) {
        lo->custom_plugin = o = Order::allocate();
        o->init(lo, -1, "UNKNOWN");
    }
    o->cxrsp.init(o, this, Mkt::CXLREJECTED, o->last->error(), 
            o->last->exchangeID(), o->canceling().thisShares(),
            o->last->sharesFilled(), o->last->liq(), !lo->is_prom,
            o->last->sharesCanceled(),
            o->confirmed().thisPrice(), curtv(), o->orderID());
    o->last = &(o->cxrsp);
    oh.send(o->cxrsp);
}

void DataManager::onBreak ( lib3::BreakDetails *bd ) {
    //XXX: do more here.
    TAEL_PRINTF(dbg.get(), TAEL_ERROR, "DM::onBreak: %s broke %lu @%f", bd->symbol, bd->break_size, bd->break_px);
}

// GVNOTE: Called from OM's onPositionUpdate function, after we receive a response from
// the trade servers for our position query
void DataManager::onPositionUpdate ( const char *acct, int cid, int pos ) {
    //XXX: would like to detect external position changes.
    //     Update to HEAD; Steve has made this possible.
}

int DataManager::position ( int cid ) {
    return om->pos(cid);
}

int DataManager::locates ( int cid ) {
    return om->locates(cid);
}

bool DataManager::getLoc ( bool async ) {
    if (!initialized_) {
        TAEL_PRINTF(dbg.get(), TAEL_ERROR, "getLocates called before initialization");
        return false;
    }
    switch (tradesys) {
        //case SIMTRADE:
        case NONE:
            return true;
        case SIMTRADE:
        case COLO:
//            if (!configured("margin-server")) {
//                TAEL_PRINTF(dbg.get(), TAEL_ERROR, "getLocates called without margin-server");
            if (!configured("position-server")) {
                TAEL_PRINTF(dbg.get(), TAEL_ERROR, "getLocates called without position-server");
                return false;
            } else {
                return om->request_all_locates(async);
            }
    }
    return false;
}

void DataManager::onGlobalMismatch ( const char *acct, int cid, int newpos, int oldpos ) {
	if (std::abs(newpos - oldpos) > 100) {
		TAEL_PRINTF(dbg.get(), TAEL_CRITICAL, "Global Position Mismatch (from Margin Server)"
				" for account %8s on %8s: new:%6d old:%6d", acct, ci_[cid], newpos, oldpos);
	}
	else {
		TAEL_PRINTF(dbg.get(), TAEL_ERROR, "Global Position Mismatch (from Margin Server)"
				" for account %8s on %8s: new:%6d old:%6d", acct, ci_[cid], newpos, oldpos);
	}
}

void DataManager::onPositionMismatch(const char *acct, int cid, int newpos, int oldpos) {
	if (std::abs(newpos - oldpos) > 100) {
		TAEL_PRINTF(dbg.get(), TAEL_CRITICAL, "Position Mismatch (from Trade Server) for "
					"account %8s on %8s: new:%6d old:%6d", acct, ci_[cid], newpos, oldpos);
	}
	else{
		TAEL_PRINTF(dbg.get(), TAEL_ERROR, "Position Mismatch (from Trade Server) for "
					"account %8s on %8s: new:%6d old:%6d", acct, ci_[cid], newpos, oldpos);
	}
}

void DataManager::onPositionMismatch(const char *acct, int cid,
                                    int new_total, int new_ts, int new_yst, int new_adj,
                                    int old_total, int old_ts, int old_yst, int old_adj) {
	if (std::abs(new_total - old_total) > 100) {
		TAEL_PRINTF(dbg.get(), TAEL_CRITICAL, "Position Mismatch (from Position Server) for "
					"account %8s on %8s: new:%6d old:%6d", acct, ci_[cid], new_total, old_total);
	}
	else{
		TAEL_PRINTF(dbg.get(), TAEL_ERROR, "Position Mismatch (from Position Server) for "
					"account %8s on %8s: new:%6d old:%6d", acct, ci_[cid], new_total, old_total);
	}
}

// GVNOTE: Should probably add some logic here to remove the order from our book.
// Otherwise, we keep on querying for its status (I think this order would be
// marked as unknown?).
void DataManager::onNoConfirm ( lib3::Order *lo ) {
    TAEL_PRINTF(dbg.get(), TAEL_ERROR, "DM::onNoConfirm: %d", lo->seqnum);
}

// GVNOTE: Should probably add some logic here to remove the order from our book
// after a certain number of attempts to cancel it. Otherwise, we keep on trying
// to cancel it all day long.
void DataManager::onNoCancel ( lib3::Order *lo ) {
    TAEL_PRINTF(dbg.get(), TAEL_ERROR, "DM::onNoCancel: %d", lo->seqnum);
}

DataManager::DataManager ( ) : 
    Configurable(),
    reportlog(*(new tael::LoggerConfiguration((size_t) MAX_BINARY_BUFFER_FILE_SIZE)))
{ 
    construct(); 
}

DataManager::DataManager ( std::string &confname ) :
    Configurable(confname),
    reportlog(*(new tael::LoggerConfiguration((size_t) MAX_BINARY_BUFFER_FILE_SIZE)))
{ 
    construct(); 
}

void DataManager::construct ( ) {
    initialized_ = false;
    running_ = false;
    mbk = 0;
    om = 0;
    ecb = 0;
    es = 0;

    date_ = 0;
//    outfd_ = -1;

    sims = 0;
    colos = 0;
    mktbks.resize(ECN::ECN_size, 0);
    ordbks.resize(ECN::ECN_size, 0);
    colo_accts.resize(ECN::ECN_size);
    qhs.resize(ECN::ECN_size, 0);

    // by default, start in normal mode
    MOCmode = false;
    defSwitch("MOCmode", &MOCmode, "use market on close only");
    // Default value of dbglvl should really be TAEL_WARN. We set it to an integer since
    // otherwise Boost libraries complain.
    defOption("debug-level", &dbglvl, "data debugging verbosity ([min]0 to [max]10)", 6);
    defOption("report-log-file", &reportfile_, "info about order placement", "report.log");
    defOption("alpha-log-file", &alphalogfile_, "info about alpha", "alpha.log");
    defOption("out-file", &outfile_, "redirect stderr and stdout to here", "std.out");
    defOption("symbol", &symbols_, "symbols to run");
    defOption("symbol-file", &symfile_, "file of symbols to run.");
    defOption("live-data", &live, "Use live data");
    defOption("trade-type", &trade_type, "\"colo\", \"sim\", \"none\"", "none");
    defOption("listen-only", &listen_only, "Listen-all, no trading, no cxl-disconnect");
    defOption("margin-server", &margin_server, "MarginServer ACCOUNT@host:port");
    defOption("position-server", &position_server, "PositionServer ACCOUNT@host:port");
    defOption("sim-trade-file", &sim_config, "config filename for sim trade params");
    defOption("sim-output-dir", &sim_outdir, "output dir for simulator logs");
    defOption("colo-trade-file", &colo_config, "config filename for colo trade params");

//    defOption("itch3-date", &i3date_, "First day to use Itch3 data", 20070701);
//    defOption("itch4-date", &i4date_, "First day to use Itch4 data", 20090315);
//   defOption("itch4f-date", &i4fdate_, "First day to use Itch4F data", 20091001);
//    defOption("bats3-date", &b3date_, "First day to use Bats3 data", 20090601);
//    defOption("bats3f-date", &b3fdate_, "First day to use Bats3 data", 20090805);
    defOption("arca-trade-date", &arcatrdate_, "First day to use ARCATrades data", 20090401);
    defOption("nyse-trade-date", &nysetrdate_, "First day to use NYSETrades data", 20090601);
    defOption("start-date", &sdate_, "Date to start playback.");
    defOption("end-date", &edate_, "Date to end playback.", 20201231);
    defOption("base-data-dir", &datadir_, "Base dir for data files if not default Tower setup.", "");

    defOption("symbol-subset", &symbol_subsets, "symbol subset to use (default: 7 == all syms)");
    defSwitch("massive", &massive, "use massive (overrides symbol subset)");
    defOption("reply-channel", &reply_groups, "multicast gr.ou.p.ip:port to receive replies on");
    defOption("recvbuf-size", &recvbuf, "size (in KB) to set UDP socket buffers", 16*1024);
    defOption("sequence-jump", &seqinc_, "sequence number increment", 1u);
    defOption("sequence-offset", &seqoff_, "sequence number offset", 0u);

    defOption("data-on", &datastr_, "enable ticks/quotes on these ECNs [ISLD,ARCA,NYSE...]");
    defSwitch("exchange-trades", &useextrd, "Use SIAC and UTDF trade tick data");
    defOption("holiday-file", &holiday_file, "Holiday calendar file (/apps/hyp2/... if unspecified)");
    defSwitch("use-po+", &usepoplus, "Use PO+ ARCA orders for NYSE");
    // GVNOTE: Start using PO+ orders for NYSE once we have some more things in the slippage report
    // i.e. per exec server, per exchange slippage report.
    //defOption("use-po+", &usepoplus, "Use PO+ ARCA orders for NYSE", true);

    add_dispatch(&mh);
    add_dispatch(&oh);
    add_dispatch(&th);
    add_dispatch(&tmh);
    add_dispatch(&umh);

    ecnToMM.resize(ECN::ECN_size);

    ecnToMM[ECN::ISLD] = ISLD;
    ecnToMM[ECN::ARCA] = ARCA;
    ecnToMM[ECN::BATS] = BATS;
    ecnToMM[ECN::EDGA] = EDGA;
    ecnToMM[ECN::EDGX] = EDGX;
    // GVNOTE: Should remove LIME_NYSE from the list of ECNs we care about
    ecnToMM[ECN::NYSE] = LIME_NYSE;
    ecnToMM[ECN::BSX] = BSX;
    ecnToMM[ECN::AMEX] = EXCHG;

}

DataManager::~DataManager ( ) {
    //if (outfd_ != -1) close(outfd_);
	if(outfp) fclose(outfp);
}

Mkt::RunStatus DataManager::run ( ) {
    if (!initialized_) {
        TAEL_PRINTF(dbg.get(), TAEL_ERROR,
                "DM::run called without successful initialization.");
        return Mkt::FAILURE;
    }

    Mkt::RunStatus st = Mkt::COMPLETE;
    try {
        running_ = true;
        ecb->run();
        running_ = false;
    } catch (Stop &s) {
        running_ = false;
        st = s.status;
        switch ( s.status ) {
            case Mkt::FAILURE:
                TAEL_PRINTF(dbg.get(), TAEL_ERROR, "Callback loop stopped for failure: %s",
                        s.what.c_str());
                break;
            case Mkt::STOPPED:
                TAEL_PRINTF(dbg.get(), TAEL_INFO, "Callback loop stopped by program: %s",
                        s.what.c_str());
                break;
            case Mkt::COMPLETE:
                TAEL_PRINTF(dbg.get(), TAEL_INFO, "Callback loop finished with message: %s",
                        s.what.c_str());
                break;
        }
    }
    return st;
}

bool DataManager::setupSymbols ( ) {
    using namespace std;
    using namespace boost::algorithm;
    char buf[100];
    if (configured("symbol-file")) {
        for (vector<string>::const_iterator symfile = symfile_.begin();
                symfile != symfile_.end(); ++symfile) {
        	string symfilepath = string(getenv("EXEC_ROOT_DIR"))+ string("/") + *symfile;
            ifstream ifs(symfilepath.c_str(), ifstream::in);
            if (!ifs) {
                TAEL_PRINTF(dbg.get(), TAEL_ERROR, "Could not open read symbols from %s.", symfilepath.c_str());
                continue;
            }
            while (ifs.good()) {
                ifs.getline(buf, 100);
                symbols_.push_back(string(buf));
            }
        }
    }
    if (symbols_.empty()) {
        TAEL_PRINTF(dbg.get(), TAEL_ERROR, "No symbols specified.");
    }

    // GVNOTE: Why are we looking at the holiday-file here? This should probably be moved
    // to initialize().
    if (!configured("holiday-file")) holiday_file = "/apps/hyp2/live-opteron_rhel4/conf/holiday2/us.hldys";
    holidays.addFile(holiday_file.c_str());

    for (vector<string>::iterator it = symbols_.begin();
            it != symbols_.end();
            ++it) {
        vector<string> splitres;
        split(splitres, *it, is_any_of(" \t\r,"), token_compress_on);
        
        if (splitres.empty() || splitres[0][0] == '#') continue;
        double exp = 0.0;
        bool setexp = false;
        int ords = 0;
        int pos = 0;
        int ci = 0;
        SymbolRecord sr;
        
        
        string &sym = splitres[0];
        if (sym == "") { 
            TAEL_PRINTF(dbg.get(), TAEL_WARN, "DM::setupSymbols: empty symbol discarded.");
            continue;
        }
        switch (splitres.size()) {
            default:
                TAEL_PRINTF(dbg.get(), TAEL_INFO, "DM::setupSymbols: Extra text after symbol %s discarded.",
                        sym.c_str());
            case 4:
                setexp = true;
                exp = atof(splitres[3].c_str());
            case 3:
                ords = atoi(splitres[2].c_str());
            case 2:
                pos = atoi(splitres[1].c_str());
            case 1:
                if ((ci = cid(sym.c_str())) != -1) {
                    TAEL_PRINTF(dbg.get(), TAEL_WARN,
                            "DM::setupSymbols: symbol %s already added as cid %d, skipping",
                            sym.c_str(), ci);
                } else {
                    strncpy(sr.symbol, sym.c_str(), 8);
                    sr.symbol[8] = 0;
                    sr.position = pos;
                    sr.orders = ords;
                    sr.set_exposure = setexp;
                    sr.exposure = exp;
                    sr.cid = ci_.add(sr.symbol);
                    limits_.push_back(sr);
                    TAEL_PRINTF(dbg.get(), TAEL_INFO,
                            "DM::setupSymbols: Loading %s = cid %d: Limit %d pos, %d ords, %f exp (%s)",
                            sr.symbol, sr.cid, sr.position, sr.orders, sr.exposure, sr.set_exposure? "on":"off");
                }
        }
    }
    symbols_.clear();
    return true;
}

void DataManager::setLimits ( const SymbolRecord &sr ) {
    /*
    om->SetIndvMaxPosition(sr.cid,
            sr.position != 0? sr.position : maxposn_);
    om->SetIndvMaxOrders(sr.cid, 
            sr.orders != 0? sr.orders : maxords_);
    if (sr.set_exposure) om->SetIndvMaxMargin(sr.cid, sr.exposure);
    else om->ClearIndvMaxMargin(sr.cid);
    */
}
                                                                    
    /*
bool DataManager::preinit ( ) { 
    
    return true;
}
*/

const int DataManager::mbk_id = ECN::ECN_size + 1;

bool DataManager::initialize ( ) {
    //outfp = fopen(outfile_, "a+");
	outfp = freopen((string(getenv("EXEC_LOG_DIR")) + string("/") + outfile_).c_str(), "a+", stdout);
	outfp = freopen((string(getenv("EXEC_LOG_DIR")) + string("/") + outfile_).c_str(), "a+", stderr);

    if (!configured()) {
        fprintf(stderr, "DM::initialize: not configured!\n");
        return false;
    }

    numRejects = 0;
    dbg = clite::util::factory<clite::util::debug_stream>::get(std::string("dataman"));
    dbg->setThreshold(tael::Severity(dbglvl));
    int reportfd = open((string(getenv("EXEC_LOG_DIR")) + string("/") + reportfile_).c_str(), O_RDWR | O_CREAT | O_APPEND, 0640);
    if (reportfd > -1) {
      reportfld.reset(new tael::FdLogger(reportfd));
      reportlog.addDestination(reportfld.get());
    }

    ci_.addListener(this);

    if (!setupSymbols()) return false;
    // start halted
    stops.clear(); stops.resize(cidsize(), true);

    data_on.resize(ECN::ECN_size, false);
    if (!configured("data-on")) {
        TAEL_PRINTF(dbg.get(), TAEL_WARN, "DM::initialize(): no data sources enabled");
    } else {
        for (vector<string>::const_iterator it = datastr_.begin(); 
                it != datastr_.end(); ++it)
        {
            ECN::ECN ecn = ECN::ECN_parse(it->c_str());
            if (ecn == ECN::UNKN) {
                TAEL_PRINTF(dbg.get(), TAEL_ERROR, "DM::initialize(): can't parse \"--data-on %s\"", it->c_str());
                return false;
            } else {
                data_on[ecn] = true;
            }
        }
    }
    
    if (live) {

        adfs = 0;
        es = als = new TimedAggrLiveSource(1);
        DateTime dt(TimeVal::now);
        sdate_ = dt.getintdate();

    } else {

        if (!configured("start-date")) {
            TAEL_PRINTF(dbg.get(), TAEL_ERROR, "HDM::initialize(): Start date for historical playback not set.");
            return false;
        }

        als = 0;
        es = adfs = new AggrDataFileSource();
        adfs->setdate(sdate_);
        DateTime dt;
        dt.setintdate(sdate_);
    }

    date_ = sdate_;
    HDate hdate(date_);
    const char *hname = holidays.holidayName(hdate);
    if (!hname) hname = "Null Pointer Dereference Day";

    if (holidays.isFullHoliday(hdate)) {
        TAEL_PRINTF(dbg.get(), TAEL_WARN, "DM::initialize(): Happy %s! I'm running but no open/close timers today.", hname);
        mktopen_  = TimeVal();
        mktclose_ = TimeVal();
    } else {
        DateTime dt;
        dt.setintdate(date_);
        dt.settime(9,30,0);
        mktopen_ = dt.getTimeVal();
        TimeVal now = TimeVal::now + TimeVal(1,0);
        if (live && mktopen_ < now) {
            mktopen_ = now;
        }
        if (holidays.isHalfHoliday(hdate) ||
                (holidays.isCondHoliday(hdate) && 
                 (hdate.dayOfWeek() == HDate::FRIDAY || 
                  hdate.dayOfWeek() == HDate::MONDAY))) 
        {
            TAEL_PRINTF(dbg.get(), TAEL_INFO, "DM::initialize(): Happy %s! We're going home at 1:00.", hname);
            dt.settime(13,0,0);
        } else {
            dt.settime(16,0,0);
        }
        mktclose_ = dt.getTimeVal();
    }

    // GVNOTE: Not sure why we set the curtv_ (current time value)to midnight
    midnight_ = DateTime::getMidnight(date_);
    curtv_ = midnight_;

    ecb = new EventSourceCB(es);
    mbk = new lib3::AggrMarketBook(ci_, mbk_id, es->clock, dbg.get());
    mbk->set_auto_correct(false);
    mbk->add_listener(this);

    if (live  && !setup_live_data()) return false;
    if (!live && !setup_hist_data()) return false;

    if (trade_type != "none") {
        om = new lib3::OrderManager(ci_, es, ecb, dbg.get());
        for (int ecn = 0; ecn < ECN::ECN_size; ++ecn) {
            ordbks[ecn] = new lib3::SubOrderBook(ci_, mbk_id + ecn + 1, 
                    es->clock, listen_only, ecnToMM[ecn], dbg.get());
        }
        //pm = new lib3::PositionManager(ci_, es->clock, dbg.get());

        //om->add_position_manager(pm);
        om->add_listener(this);
    } else tradesys = NONE;

    if (trade_type == "colo") {
        if (!setup_colo_trade()) return false;
        else tradesys = COLO;
    }
    if (trade_type == "sim") {
        if (!setup_sim_trade()) return false;
        else tradesys = SIMTRADE;
    }

    if (!setup_callbacks()) { return false; }
    if (mktopen_ != TimeVal()) addTimer(marketOpen());
    if (mktclose_ != TimeVal()) addTimer(marketClose());

    initialized_ = true;
    return true;
}

WakeUpdate DataManager::wakeup_message ( ) { return WakeUpdate(curtv()); }

// This is the function in the coordinator, which is called to figure out
// whether to send a wakeup_message or not.
bool DataManager::advise_wakeup ( ) { return unreadData() == 0; }

int DataManager::unreadData ( ) { return live? als->queue_size() : 0 ; }

// GVNOTE: The following function should be cleaned up to remove the code to
// fall back to symbol_subsets (this would include removing the old port7 stuff)
bool DataManager::setup_live_data ( ) {

    PortLiveSource *itch3, *obultra, *siac, *arca, *arcatr, *trade, *bats, *edgx, *edga,
                   *utdf, *nyset, *bsx;//, *amex;

    if (massive && !symbol_subsets.empty()) {
        TAEL_PRINTF(dbg.get(), TAEL_ERROR, "setup_live_data: cannot use both massive and symbol-subset!");
        return false;
    }

    if (massive) symbol_subsets.push_back(0);
    else if (symbol_subsets.empty()) symbol_subsets.push_back(7);

    for (vector<int>::const_iterator subit = symbol_subsets.begin(); 
            subit != symbol_subsets.end(); ++subit)
    {
    	// We don't need to listen/set up data source in MOC mode.
    	if (MOCmode) break;

        int poff = *subit;

        // For each exchange for which we want data, subscribe to the quotes and trades feeds
        if (data_on[ECN::ISLD]) {
            itch3 = new PortLiveSource(ITCH4F_PORT_BASE + poff, &ci_);
            TAEL_PRINTF(dbg.get(), TAEL_INFO, "LDM::addPorts: setting up Itch4 port %d",
                    ITCH4F_PORT_BASE + poff);
            // GVNOTE: In setup_pls() if massive is true, we don't even look at the
            // mcast IP address fed into the function. So doesn't matter what
            // McastDataGroup[0] is. We just want to make sure that when using massive,
            // we use the *_PORT_BASE in the PortLiveSource constructor above.
            if (!setup_pls(itch3, poff == 7? McastITCH4f_7 : McastDataGroup[poff])) 
            { delete itch3; return false; }

            setup_book<lib3::Itch4QuoteHandler>(ECN::ISLD);
        }

        if (data_on[ECN::NYSE]) {
            obultra = new PortLiveSource(OBULTRA_PORT_BASE + poff, &ci_);
            TAEL_PRINTF(dbg.get(), TAEL_INFO, "LDM::addPorts: setting up OBUltra port %d",
                    OBULTRA_PORT_BASE + poff);
            if (!setup_pls(obultra, poff == 7? McastOBUltra_7 : McastDataGroup[poff])) 
            { delete obultra; return false; }

            nyset = new PortLiveSource(NYSETRADES_PORT_BASE + poff, &ci_);
            TAEL_PRINTF(dbg.get(), TAEL_INFO, "LDM::addPorts: setting up NyseTrade port %d",
                    NYSETRADES_PORT_BASE + poff);
            if (!setup_pls(nyset, poff == 7? McastNYSETk_7 : McastDataGroup[poff])) 
            { delete nyset; return false; }

            setup_book<lib3::OBUltraQuoteHandler>(ECN::NYSE);
        }

        if (data_on[ECN::ARCA]) {    
            arca = new PortLiveSource(ARCA2_PORT_BASE + poff, &ci_);
            TAEL_PRINTF(dbg.get(), TAEL_INFO, "LDM::addPorts: setting up Arca2 port %d",
                    ARCA2_PORT_BASE + poff);
            if (!setup_pls(arca, poff == 7? McastARCA2_7 : McastDataGroup[poff])) 
            { delete arca; return false; }

            arcatr = new PortLiveSource(ARCATRADES_PORT_BASE, &ci_);
            TAEL_PRINTF(dbg.get(), TAEL_INFO, "LDM::addPorts: setting up ArcaTrades port %d",
                    ARCATRADES_PORT_BASE);
            if (!setup_pls(arcatr, McastArcaTrades)) 
            { delete arcatr; return false; }
            if (massive) arcatr->mcastJoin(McastArcaTrades);

            setup_book<lib3::ArcaQuoteHandler>(ECN::ARCA);
        }

        if (data_on[ECN::BATS]) {
            bats = new PortLiveSource(BATS3F_PORT_BASE + poff, &ci_);
            TAEL_PRINTF(dbg.get(), TAEL_INFO, "LDM::addPorts: setting up Bats3 port %d",
                    BATS3F_PORT_BASE + poff);
            if (!setup_pls(bats, poff == 7? McastBATS3f_7 : McastDataGroup[poff])) 
            { delete bats; return false; }

            setup_book<lib3::Bats3QuoteHandler>(ECN::BATS);
        }

        // GVNOTE: Seems that we are setting up Lime EDGA/EDGX/BSX ports. Should
        // probably change this

        if (data_on[ECN::EDGA]) {
            edga = new PortLiveSource(EDGA2_PORT_BASE + poff, &ci_);
            TAEL_PRINTF(dbg.get(), TAEL_INFO, "LDM::addPorts: setting up Lime EDGA port %d",
                    EDGA2_PORT_BASE + poff);
            if (!setup_pls(edga, poff == 7? McastEDGA2_7 : McastDataGroup[poff])) 
            { delete edga; return false; }

            setup_book<lib3::EdgaQuoteHandler>(ECN::EDGA);
        }
        if (data_on[ECN::EDGX]) {
            edgx = new PortLiveSource(EDGX2_PORT_BASE + poff, &ci_);
            TAEL_PRINTF(dbg.get(), TAEL_INFO, "LDM::addPorts: setting up Lime EDGX port %d",
                    EDGX2_PORT_BASE + poff);
            if (!setup_pls(edgx, poff == 7? McastEDGX2_7 : McastDataGroup[poff])) 
            { delete edgx; return false; }

            setup_book<lib3::EdgxQuoteHandler>(ECN::EDGX);
        }
        if (data_on[ECN::BSX]) {
            bsx = new PortLiveSource(BX4_PORT_BASE + poff, &ci_);
            TAEL_PRINTF(dbg.get(), TAEL_INFO, "LDM::addPorts: setting up Lime EDGX port %d",
                    BX4_PORT_BASE + poff);
            if (!setup_pls(bsx, poff == 7? McastBX4_7 : McastDataGroup[poff])) 
            { delete bsx; return false; }

            setup_book<lib3::BX4QuoteHandler>(ECN::BSX);
        }

        if (useextrd) {
        	// Use UTDF and SIAC trade tick data. These are aggregator feeds across all exchanges.
            utdf = new PortLiveSource(UTDF_PORT_BASE + poff, &ci_);
            TAEL_PRINTF(dbg.get(), TAEL_INFO, "LDM::addPorts: setting up UTDF port %d",
                    UTDF_PORT_BASE + poff);
            if (!setup_pls(utdf, poff == 7? McastUTDF_7 : McastDataGroup[poff])) 
            { delete utdf; return false; }

            siac = new PortLiveSource(SIAC_PORT_BASE + poff, &ci_);
            TAEL_PRINTF(dbg.get(), TAEL_INFO, "LDM::addPorts: setting up SIAC port %d",
                    SIAC_PORT_BASE + poff);
            if (!setup_pls(siac, poff == 7? McastSIAC_7 : McastDataGroup[poff])) 
            { delete siac; return false; }
        }
    }

    
    if (configured("reply-channel")) {
        using namespace boost::algorithm;

        for (vector<string>::iterator i = reply_groups.begin(); i != reply_groups.end(); ++i) {

            vector<string> splitres;
            split(splitres, *i, is_any_of(":"));
            if (splitres.size() != 2 || atoi(splitres[1].c_str()) == 0) {
                TAEL_PRINTF(dbg.get(), TAEL_ERROR, "LDM::addPorts: couldn't parse reply group %s",
                        i->c_str());
            } else {
                int port = atoi(splitres[1].c_str());
                string &host = splitres[0];
                trade = new PortLiveSource(port, &ci_);
                TAEL_PRINTF(dbg.get(), TAEL_ERROR, "LDM::addPorts: setting up order port %s:%d",
                        host.c_str(), port);
                if (!setup_pls(trade, host.c_str())) { delete trade; return false; }
                // GVNOTE: If massive is true, then in setup_pls, it ignores the host address.
                // Hence we need to do the mcastJoin here explicitly. Perhaps we should separate
                // setup_pls into 2 functions - one for only adding the input pls to DM's als,
                // and one in which in addition, we do an mcastJoin as well.
                if (massive) trade->mcastJoin(host.c_str());
            }
        }
    }

    return true;
}

// GVNOTE: When we try to do a simulation, we exceed the limit of open fds since for
// each exchange that we have data on, and for each symbol that we care about, we open
// a file descriptor. Since we are unlikely to change this behavior, we should probably
// increase the number of allowed open file descriptors.
bool DataManager::setup_hist_data ( ) {

    bool addutdf = false;
    std::string 
  //      itch2dir   (datadir_ + "openview"),// data till 03/04/2009
  //      itch3dir   (datadir_ + "itch3"),   // data till 06/15/2009
  //      itch4dir   (datadir_ + "itch4"),   // data till 02/08/2010
        itch4dir   (datadir_ + "itch4f"),    // current data
  //      bats3dir   (datadir_ + "bats3"),   // data till 02/08/2010
        bats3fdir  (datadir_ + "bats3f"),    // current data
        obultradir (datadir_ + "obultra"),   // current data
        siacdir    (datadir_ + "siac"),      // current data
        arcatrdir  (datadir_ + "arcatrades"),// current data
        nysetrdir  (datadir_ + "nysetrades"),// current data
        edga2dir   (datadir_ + "edga2"),     // current data
        edgx2dir   (datadir_ + "edgx2"),     // current data
        amexdir    (datadir_ + "amexultra"), // current data
        bsxdir     (datadir_ + "bx4"),       // current data
        utdfdir    (datadir_ + "utdf");      // current data
    const char *datad   = datadir_ != ""? datadir_.c_str() : "/apps";
//    const char *itch2d  = datadir_ != ""? itch2dir.c_str() : "/apps/itch2";
//    const char *itch3d  = datadir_ != ""? itch3dir.c_str() : "/apps/itch3";
//    const char *itch4d  = datadir_ != ""? itch4dir.c_str() :
//        (sdate_ < i4fdate_? "/apps/itch4" : "/apps/itch4f");
    const char *itch4d  = datadir_ != ""? itch4dir.c_str() : "/apps/itch4f";
//    const char *bats3d  = datadir_ != ""? bats3dir.c_str() : "/apps/bats3";
    const char *bats3fd = datadir_ != ""? bats3fdir.c_str()  : "/apps/bats3f";
    const char *nysed   = datadir_ != ""? obultradir.c_str() : "/apps/obultra";
    const char *siacd   = datadir_ != ""? siacdir.c_str() : "/apps/siac";
    const char *utdfd   = datadir_ != ""? utdfdir.c_str() : "/apps/utdf";
    const char *arcatrd = datadir_ != ""? arcatrdir.c_str() : "/apps/arcatrades";
    const char *nysetrd = datadir_ != ""? nysetrdir.c_str() : "/apps/nysetrades";
    const char *amexd   = datadir_ != ""? amexdir.c_str() : "/apps/amexultra";
    const char *bsxd    = datadir_ != ""? bsxdir.c_str() : "/apps/bx4";
    const char *edga2d  = datadir_ != ""? edga2dir.c_str() : "/apps/edga2";
    const char *edgx2d  = datadir_ != ""? edgx2dir.c_str() : "/apps/edgx2";
    const char *tickd   = datadir_ != ""? datadir_.c_str() : "/apps/";

    const char *sym;

    if (data_on[ECN::ISLD]) setup_book<lib3::Itch4QuoteHandler>(ECN::ISLD);
    if (data_on[ECN::ARCA]) setup_book<lib3::ArcaQuoteHandler>(ECN::ARCA);
    if (data_on[ECN::BATS]) setup_book<lib3::Bats3QuoteHandler>(ECN::BATS);
    if (data_on[ECN::EDGA]) setup_book<lib3::EdgaQuoteHandler>(ECN::EDGA);
    if (data_on[ECN::EDGX]) setup_book<lib3::EdgxQuoteHandler>(ECN::EDGX);
    if (data_on[ECN::NYSE]) setup_book<lib3::OBUltraQuoteHandler>(ECN::NYSE);
    if (data_on[ECN::BSX])  setup_book<lib3::BX4QuoteHandler>(ECN::BSX);

    for (int cid = 0; cid < cidsize(); ++cid) {
        sym = symbol(cid);

        if (data_on[ECN::ISLD]) {
            /*if (sdate_ < i3date_)
                adfs->addSource(new HistOpenViewDataSource(sym, itch2d));
            else if (sdate_ < i4date_)
                adfs->addSource(new HistItch3DataSource(sym, itch3d)); 
            else*/
                adfs->addSource(new HistItch4DataSource(sym, itch4d));
        }

        if (data_on[ECN::ARCA]) {
            adfs->addSource(new HistArca2DataSource(sym, "arcat", datad));
            if (sdate_ >= arcatrdate_) {
                adfs->addSource(new HistArcaTradesDataSource(sym, arcatrd));
            } else {
                addutdf = true;
            }
        }

        if (data_on[ECN::BATS]) {
            adfs->addSource(new HistBats3fDataSource(sym, bats3fd));
        }

        if (data_on[ECN::EDGA]) {
            adfs->addSource(new HistEdgeDataSource(sym, edga2d, TOWER_EDGA));
        }
        if (data_on[ECN::EDGX]) {
            adfs->addSource(new HistEdgeDataSource(sym, edgx2d, TOWER_EDGX));
        }

        if (data_on[ECN::NYSE]) {
            adfs->addSource(new OBUltraDataSource(sym, nysed));
            if (sdate_ >= nysetrdate_) {
                adfs->addSource(new NYSETradeDataSource(sym, nysetrd));
            } else {
                addutdf = true;
            }
        }

        if (data_on[ECN::BSX]) {
            adfs->addSource(new HistBX4DataSource(sym, bsxd));
        }

        if (data_on[ECN::AMEX]) {
            adfs->addSource(new AMEXUltraDataSource(sym, amexd));
        }
        if (addutdf || useextrd) {
            adfs->addSource(new UTDFFileSource(sym, utdfd));
            adfs->addSource(new SIACRecFileSource(sym, siacd, 0, false));
        }
    }
    return true;
}

int trySetRecvBuf (Socket *s, int sz) {
    int res = 0;
    if (s->setIntSockOpt(SO_RCVBUF, sz) 
            && s->getIntSockOpt(SO_RCVBUF, &res))
        return res;
    else return -1;
}

bool DataManager::setup_pls ( PortLiveSource *pls, const char *addr ) {
    if (pls->Connected()) {
        if (!massive && !pls->mcastJoin(addr)) {
            TAEL_PRINTF(dbg.get(), TAEL_ERROR, "LDM::setup_pls: not massive and didn't mcastJoin to %s.", addr);
            return false;
        }

        int bufsz = recvbuf * 1024;
        int actualsz = trySetRecvBuf(pls->getSocket(), bufsz);
        if (actualsz < bufsz) {
            TAEL_PRINTF(dbg.get(), TAEL_ERROR, "LDM::setup_pls: Socket buffer cannot be set to %d (OS reports %d).\n\t\tCheck rmem_max and try again.", bufsz, actualsz);
            return false;
        }
        als->add(pls);
        if (massive) {
        	TAEL_PRINTF(dbg.get(), TAEL_INFO, "LDM::setup_pls: set up of PLS %s on fd #%d for massive succeeded.",
                        pls->name(), pls->getSocket()->getFD());
        } else {
            TAEL_PRINTF(dbg.get(), TAEL_INFO, "LDM::setup_pls: set up of PLS %s = %s on fd #%d succeeded.",
                        pls->name(), addr, pls->getSocket()->getFD());
        }
        return true;
    } else {
        TAEL_PRINTF(dbg.get(), TAEL_ERROR, "LDM::setup_pls: port connection failed.");
        return false;
    }
}

bool DataManager::parse_combined_server(const string &in, string *user, string *host, int *port) {
    using namespace boost::algorithm;

    vector<string> splitres;
    split(splitres, in, is_any_of("@:,"), token_compress_on);

    if (splitres.size() != 3)  {
        TAEL_PRINTF(dbg.get(), TAEL_ERROR, "%s: Not specified.", in.c_str());
        return false;
    }
    *user = splitres[0];
    *host = splitres[1];

    *port = atoi(splitres[2].c_str());
    return (*port > 0 && *port < 65536);
}

/*
bool DataManager::setup_margin_server ( ) {
    using namespace lib3;
    string account, host;
    int port;

    if (!configured("margin-server")) {
        TAEL_PRINTF(dbg.get(), TAEL_ERROR, "No margin server specified.");
        return false;
    }
    if (!parse_combined_server(margin_server, &account, &host, &port)) return false;

    MarginServerTCPTransmitter *margin_tx = 
        new MarginServerTCPTransmitter(host.c_str(), port, es->clock, dbg.get());
    if (!margin_tx->connect()) {
        TAEL_PRINTF(dbg.get(), TAEL_ERROR, "Couldn't connect to margin server %s@%s:%d", account.c_str(), host.c_str(), port);
        return false;
    }
    
    SingleThreadedScheduler<SockMessageStruct> *comm = 
        new SingleThreadedScheduler<SockMessageStruct>(margin_tx, dbg.get());
    ms = new MarginServer(account.c_str(), comm, es->clock, ecb, dbg.get());
    om->set_margin_server(ms);
    margin_acct = account;
    return true;
}
*/

bool DataManager::setup_position_server() {
	using namespace lib3;
    string account, host;
    int port;

    if (!configured("position-server")) {
        TAEL_PRINTF(dbg.get(), TAEL_ERROR, "No position server specified.");
        return false;
    }
    if (!parse_combined_server(position_server, &account, &host, &port)) return false;

    PositionServerTCPTransmitter *ps_tx =
		  new PositionServerTCPTransmitter(host.c_str(), port, es->clock, dbg.get());
    if (!ps_tx->connect()) {
        TAEL_PRINTF(dbg.get(), TAEL_ERROR, "Couldn't connect to position server %s@%s:%d", account.c_str(), host.c_str(), port);
        return false;
    }
    SingleThreadedScheduler<char> *ps_comm = new SingleThreadedScheduler<char>(ps_tx, dbg.get());
    ps = new PositionServer(ps_comm, ci_, dbg.get());
    om->set_position_server(ps);
    ps_acct = account;
    return true;
}

DataManager::colo_options::colo_options ( vector<string> const &hdr, vector<string> const &vals ) {
    using namespace clite::util;
    if (vals.size() != 4) {
        throw table_data_error("colo_options line does not have 4 elements.");
    }

    ecn = ECN::ECN_parse(vals[0].c_str());
    account = vals[1];
    host = vals[2];
    port = atoi(vals[3].c_str());

    if (ecn == ECN::UNKN || port < 1024 || port > 65535) {
        throw table_data_error("colo_options line has bad ECN or port.");
    }
}

DataManager::sim_options::sim_options ( vector<string> const &hdr, vector<string> const &vals ) {
    using namespace clite::util;
    if (vals.size() != 5) {
        throw table_data_error("sim_options line does not have 5 elements.");
    }

    ecn = ECN::ECN_parse(vals[0].c_str());
    seq_us    = atoi(vals[1].c_str());
    live_us   = atoi(vals[2].c_str());
    reply_us  = atoi(vals[3].c_str());
    cancel_us = atoi(vals[4].c_str());

    if (ecn == ECN::UNKN || 
            seq_us    < 1 || seq_us   > 999999 ||
            live_us   < 1 || live_us   > 999999 ||
            reply_us  < 1 || reply_us  > 999999 ||
            cancel_us < 1 || cancel_us > 999999) 
    {
        throw table_data_error("sim_options line has bad ECN or timing");
    }
}

bool DataManager::setup_colo_trade ( ) {
    using namespace lib3;

    if (!configured("colo-trade-file")) { 
        TAEL_PRINTF(dbg.get(), TAEL_ERROR, "colo trade file not configured!");
        return false;
    }

    
    typedef clite::util::file_table<colo_options> configmap;
    colos = new configmap(colo_config);
    TAEL_PRINTF(dbg.get(), TAEL_INFO, "Connecting to %lu colo servers", colos->size());

    for (configmap::const_iterator it = colos->begin(); it != colos->end(); ++it) {
        TAEL_PRINTF(dbg.get(), TAEL_INFO, "Connecting to %s: %s %s %d",
                ECN::desc(it->second.ecn), it->second.account.c_str(), 
                it->second.host.c_str(), it->second.port);

        switch (it->second.ecn) {
            case ECN::ARCA:
                setup_colo_trader<ArcaTCPTransmitter, ArcaTrader>(it->second);
                break;
            case ECN::BATS:
                setup_colo_trader<BatsTCPTransmitter, BatsTrader>(it->second);
                break;
            case ECN::BSX:
                setup_colo_trader<BostonTCPTransmitter, BostonTrader>(it->second);
                break;
            case ECN::EDGA:
                setup_colo_trader<EdgaTCPTransmitter, EdgaTrader>(it->second);
                break;
            case ECN::EDGX:
                setup_colo_trader<EdgxTCPTransmitter, EdgxTrader>(it->second);
                break;
            case ECN::ISLD:
                setup_colo_trader<NasdaqTCPTransmitter, NasdaqTrader>(it->second);
                break;
            case ECN::NYSE:
                setup_colo_trader<NyseTCPTransmitter, NyseTrader>(it->second);
                break;
            default:
                TAEL_PRINTF(dbg.get(), TAEL_ERROR, "setup_colo_trade: ECN %s not available for colo.",
                        ECN::desc(it->second.ecn));
                return false;
                break;
        }
    }
/*    if (!configured("margin-server")) {
        TAEL_PRINTF(dbg.get(), TAEL_WARN, "DM::setup_colo_trade: Careful! did not setup margin server!!");
    } else if (!setup_margin_server()) {
        return false;
    }*/
    if (!configured("position-server")) {
        TAEL_PRINTF(dbg.get(), TAEL_WARN, "DM::setup_colo_trade: Careful! did not setup position server!!");
    } else if (!setup_position_server()) {
        return false;
    }
    return true;
}

bool DataManager::setup_sim_trade ( ) {
    using namespace lib3;

    if (!configured("sim-trade-file")) { 
        TAEL_PRINTF(dbg.get(), TAEL_ERROR, "sim trade file not configured!");
        return false;
    }
    if (!configured("sim-output-dir")) {
        TAEL_PRINTF(dbg.get(), TAEL_ERROR, "sim output dir is missing.");
        return false;
    }

    const char *output_dir = sim_outdir.c_str();
    char output_file[256];

    snprintf(output_file, 256, "sim.%s.%d.actionlog", getenv("USER"), sdate_);
    sim_action_logger = clite::util::factory<clite::util::debug_stream>::get(std::string(output_file));

    snprintf(output_file, 256, "sim.%s.%d.execlog", getenv("USER"), sdate_);
    sim_exec_logger = clite::util::factory<clite::util::debug_stream>::get(std::string(output_file));

    typedef clite::util::file_table<sim_options> configmap;
    sims = new configmap(sim_config);

    for (configmap::const_iterator it = sims->begin(); it != sims->end(); ++it) {
    //    std::cout << "In setup_sim_trade. ECN = " << ECN::desc(it->second.ecn) << std::endl;
    //    if (!mktbks[it->second.ecn]) continue;
    //    std::cout << "In setup_sim_trade. ECN = " << ECN::desc(it->second.ecn) << std::endl;
        switch (it->second.ecn) {
            case ECN::ARCA:
                setup_sim_trader<ArcaSimMarket, ArcaTrader>(it->second);
                break;
            case ECN::BATS:
                setup_sim_trader<BatsSimMarket, BatsTrader>(it->second);
                break;
            case ECN::BSX:
                //GVNOTE: Disabling simulation on BSX, EDGA, EDGX
//                setup_sim_trader<BostonSimMarket, BostonTrader>(it->second);
                TAEL_PRINTF(dbg.get(), TAEL_ERROR, "setup_sim_trade: BSX not available for sim.");
                break;
            case ECN::EDGA:
//                setup_sim_trader<EdgaSimMarket, EdgaTrader>(it->second);
                TAEL_PRINTF(dbg.get(), TAEL_ERROR, "setup_sim_trade: EDGA not available for sim.");
                break;
            case ECN::EDGX:
//                setup_sim_trader<EdgxSimMarket, EdgxTrader>(it->second);
                TAEL_PRINTF(dbg.get(), TAEL_ERROR, "setup_sim_trade: EDGX not available for sim.");
                break;
            case ECN::ISLD:
                setup_sim_trader<NasdaqSimMarket, NasdaqTrader>(it->second);
                break;
            case ECN::NYSE:
//                setup_sim_trader<NyseSimMarket, NyseTrader>(it->second);
                TAEL_PRINTF(dbg.get(), TAEL_ERROR, "setup_sim_trade: NYSE not available for sim.");
                break;
            default:
                TAEL_PRINTF(dbg.get(), TAEL_ERROR, "setup_sim_trade: ECN %s not available for sim.",
                        ECN::desc(it->second.ecn));
                return false;
                break;
        }
    }
    if (!configured("position-server")) {
        TAEL_PRINTF(dbg.get(), TAEL_WARN, "DM::setup_colo_trade: Careful! did not setup position server!!");
    } else if (!setup_position_server()) {
        return false;
    }
    return true;
}

// GVNOTE: Do we really need to listen to all these sources? We should probably remove
// old sources like ITCH3/4 etc., and things we don't trade on like BX4.
bool DataManager::setup_callbacks ( ) {
    ecb->addListener(USER_MESSAGE, this, 50);
    ecb->addListener(SERVER_ALERT_MESSAGE, this, 50);
    ecb->addListener(TIME_MESSAGE, this, 50);
    ecb->addListener(IGNORED_MESSAGE, this, 50);

/*  // GVNOTE: I believe these are not used, since we don't define a handler for these in the
 *  // onEvent() method. Moreover, we don't need these since we are using massive.
    ecb->addListener(ARCA_QUOTE, this, 50);
    ecb->addListener(BATS_QUOTE, this, 50);
    ecb->addListener(ISLD_ADD, this, 50);
    ecb->addListener(ISLD_CANCELLED, this, 50);
    ecb->addListener(ITCH3_ADD, this, 50);
    ecb->addListener(ITCH3_CANCEL, this, 50);
*/

    // We don't need to listen to data if we are operating in MOC mode. So don't
    // set up the following callbacks.
    if (MOCmode) return true;

    ecb->addListener(TRADE_TICK, this, 50);
    ecb->addListener(SIAC_TICK, this, 50);
    ecb->addListener(ITCH4_EXEC, this, 50);
    ecb->addListener(ITCH4_CROSSEXEC, this, 50);
    ecb->addListener(ITCH4_HIDDENEXEC, this, 50);
    ecb->addListener(ITCH4F_EXEC, this, 50);
    ecb->addListener(ITCH4F_CROSSEXEC, this, 50);
    ecb->addListener(ITCH4F_HIDDENEXEC, this, 50);
    ecb->addListener(ITCH3_EXEC, this, 50);
    ecb->addListener(ITCH3_CROSSEXEC, this, 50);
    ecb->addListener(ITCH3_HIDDENEXEC, this, 50);
    ecb->addListener(ISLD_EXECUTED, this, 50);
    ecb->addListener(ISLD_HIDDENEXEC, this, 50);
    ecb->addListener(NYSE_TRADE, this, 50);
    ecb->addListener(BATS_TICK, this, 50);
    ecb->addListener(BATS3_EXC, this, 50);
    ecb->addListener(BATS3_HDN, this, 50);
    ecb->addListener(ARCA_TRADE, this, 50);
    ecb->addListener(BX4_EXEC, this, 50);
    ecb->addListener(BX4_CROSSEXEC, this, 50);
    ecb->addListener(BX4_HIDDENEXEC, this, 50);
    return true;
}
