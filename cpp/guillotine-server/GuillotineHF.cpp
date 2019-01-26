#include "GuillotineHF.h"
#include "GuillotineTCP.h" //NSARKAS: Introduced this so that we have well defined clientId constants to pass around instead of ad hoc constants
#include <TradeConstraints.h>
#include <BookTools.h>
#include <Chunk.h>
#include <PNLTracker.h>
#include <TCTracking.h>
#include <CrossComponent.h>
#include <TakeInvisibleComponent.h>
#include <ModelTracker.h>
#include "FollowLeaderJoinQueueBackup.h"

using std::list;
using std::string;

namespace guillotine {
    namespace server {

int msg_handler::to_cid ( const string &s, int clientId ) {
	// GVNOTE: Changed this to 8 from 7, because I changed symbol in struct SymbolRecord
	// (DataManager.h) to contain 9 characters instead of 8 (including the ending null).
    if (s.size() > 8) {
        c->send_symerror(s, clientId);
        return -1;
    } else {
        int cid = c->dm->cid(s.c_str());
        if (cid == -1) {
            c->send_symerror(s, clientId);
        }
        return cid;
    }
}

// this could be handled entirely on the frondend thread?
// At least it gives the client an indication that they've talked to 
// the trading thread.
void msg_handler::operator() ( const typed::connect &req ) {
    typed::server s;
    s.name = c->namestr;
    s.symbols = c->symlist;
    s.clientId = req.clientId;
    c->my_rsps.push_back(typed::response(s));
}

// these are queueable actions.
// GVNOTE: Will have to add orderID info to ExecutionEngine ...
void msg_handler::operator() ( const typed::trade &req ) {
	std::string sym = req.symbol;
	int cid = to_cid(sym, req.clientId);
	if (cid != -1) {
		if (c->stops[cid]) {
			c->send_halterror(sym, req.clientId);
		} else {
			double bid = 0.0, ask = 0.0;
			getMarket(c->dm->masterBook(), cid, Mkt::BID, 0, &bid, 0);
			getMarket(c->dm->masterBook(), cid, Mkt::ASK, 0, &ask, 0);

			int oldtarget = c->trd->getTargetPosition(cid);
			TAEL_PRINTF(&c->costlog, TAEL_INFO, "REQ %s currPos: %d qty: %d at aggr %.3f (%.2f,%.2f) [orderID: %ld]",
				sym.c_str(), c->dm->position(cid), req.qty, req.aggr, bid, ask, req.orderID);

			equal_cid eq(cid);
			c->actions.erase(
					std::remove_if(c->actions.begin(), c->actions.end(),
						boost::apply_visitor(eq)),
					c->actions.end());

			c->actions.push_back(do_trade(cid, req.qty, req.aggr,
						req.aggr * 1000000000 +
						std::abs(req.qty) * (bid + ask),
						req.orderID, req.short_mark, req.clientId));
		}
	}
}

void msg_handler::operator() ( const typed::resume &req ) {
    if (req.all) {
        int numLocates = 0;
        for (int cid = 0; cid < c->dm->cidsize(); ++cid) {
            if (c->dm->locates(cid) > 0) {
            	numLocates++;
            }
        }
        if ((numLocates > MIN_BORROW_RATIO * (c->dm->cidsize())) || (c->dm->getTradeSystem() == DataManager::SIMTRADE)) {
			for (int cid = 0; cid < c->dm->cidsize(); ++cid) {
				c->stops[cid] = false;
				c->dm->stops[cid] = false;
				c->send_info(cid, req.clientId);
			}
        } else {
        	TAEL_PRINTF(&c->log, TAEL_FATAL, "Not enough locates found! %d / %d locates found", numLocates, c->dm->cidsize());
        	char error_msg[50];
        	sprintf(error_msg, "Not enough locates found! %d / %d locates found", numLocates, c->dm->cidsize());
        	throw std::runtime_error(error_msg);
        }
    } else {
        for (list<string>::const_iterator sym = req.symbols.begin();
                sym != req.symbols.end(); ++sym)
        {
            int cid = to_cid(*sym, req.clientId);
            if (cid != -1) {
                c->stops[cid] = false;
				c->dm->stops[cid] = false;
                c->send_info(cid, req.clientId);
            }
        }
    }
}
void msg_handler::operator() ( const typed::stop &req ) {
    if (req.all) {
        for (int cid = 0; cid < c->dm->cidsize(); ++cid) {
            setup_stop(cid, req.clientId);
        }
    } else {
        for (list<string>::const_iterator sym = req.symbols.begin();
                sym != req.symbols.end(); ++sym)
        {
            int cid = to_cid(*sym, req.clientId);
            if (cid != -1) {
                setup_stop(cid, req.clientId);
            }
        }
    }
}
void msg_handler::operator() ( const typed::halt &req ) {
    if (req.all) {
        for (int cid = 0; cid < c->dm->cidsize(); ++cid) {
            c->stops[cid] = true;
            c->dm->stops[cid] = true;
            setup_stop(cid, req.clientId);
        }
    } else {
        for (list<string>::const_iterator sym = req.symbols.begin();
                sym != req.symbols.end(); ++sym)
        {
            int cid = to_cid(*sym, req.clientId);
            if (cid != -1) {
                c->stops[cid] = true;
                c->dm->stops[cid] = true;
                setup_stop(cid, req.clientId);
            }
        }
    }
}

void msg_handler::operator() ( const typed::status &req ) {
    if (req.all) {
        for (int cid = 0; cid < c->dm->cidsize(); ++cid) {
            c->send_info(cid, req.clientId);
        }
    } else {
        for (list<string>::const_iterator sym = req.symbols.begin();
                sym != req.symbols.end(); ++sym)
        {
            int cid = to_cid(*sym, req.clientId);
            if (cid != -1) {
                c->send_info(cid, req.clientId);
            }
        }
    }
}

// When you call "stop", be sure not to immediately start trading again.
// Queued "do_trade" actions will be cleared here, because stops are always
// sorted higher priority than trades.

void msg_handler::setup_stop ( int cid, int clientId ) {
    int pos = c->dm->position(cid);
    int oldtarget = c->trd->getTargetPosition(cid);
    double bid = 0.0, ask = 0.0;
    getMarket(c->dm->masterBook(), cid, Mkt::BID, 0, &bid, 0);
    getMarket(c->dm->masterBook(), cid, Mkt::ASK, 0, &ask, 0);
    TAEL_PRINTF(&c->costlog, TAEL_INFO, "REQ %s pos: %d (qtyLeft: %d) (%.2f,%.2f) #stop",
            c->dm->symbol(cid), pos, oldtarget - pos, bid, ask);

    equal_cid eq(cid);
    // note: "remove_if" is unintuitive.
    c->actions.erase(
            std::remove_if(c->actions.begin(), c->actions.end(),
                boost::apply_visitor(eq)),
            c->actions.end());

    c->actions.push_back(do_stop(cid, 
                std::abs(pos - oldtarget) * (bid + ask), clientId));
}

void apply_action::operator() ( const do_stop &s ) {
    c->trd->stop(s.cid, s.clientId);
}

void apply_action::operator() ( const do_trade &s ) {
  c->trd->trade(s.cid, s.qty, s.aggr * 0.0001, s.orderID, s.clientId, (Mkt::Marking)s.short_mark);
}

void hfcontext::send_info ( int cid, int qty, double prio, int clientId ) {
    typed::info i;
    i.clientId = clientId;
    int short_mark = trd->getShortMarking(cid);
    i.symbol = dm->symbol(cid);

    i.qtyLeft = qty;
    i.position = dm->position(cid);
    i.locates = dm->locates(cid);
    i.aggr = prio * 10000;
    i.halt = stops[cid];
    i.time_sec = dm->curtv().sec();
    i.time_usec = dm->curtv().usec();
    if (!getMarket(dm->masterBook(), cid, Mkt::BID, 0, &i.bid, &i.bidsz)) {
        i.bid = 0.0; i.bidsz = 0;
    }
    if (!getMarket(dm->masterBook(), cid, Mkt::ASK, 0, &i.ask, &i.asksz)) {
        i.ask = 0.0; i.asksz = 0;
    }

    my_rsps.push_back(typed::response(i));
}

void hfcontext::send_info ( int cid, int clientId ) {
    int target = trd->getTargetPosition(cid);
    int qty = target - dm->position(cid);
    double prio = trd->getPriority(cid);
    send_info(cid, qty, prio, clientId);
}

void hfcontext::send_symerror ( const std::string &sym, int clientId ) {
    typed::error e;
    e.symbol = sym;
    e.reason = typed::error::unknown_symbol;
    e.info = "I can't trade that";
    e.clientId = clientId;
    my_rsps.push_back(typed::response(e));
}

void hfcontext::send_halterror ( const std::string &sym, int clientId ) {
    typed::error e;
    e.symbol = sym;
    e.reason = typed::error::halt;
    e.info = "That symbol is halted";
    e.clientId = clientId;
    my_rsps.push_back(typed::response(e));
}

void hflistener::update ( const WakeUpdate &w ) {

    if (!c->my_rsps.empty()) 
        c->rspx.try_swap(c->my_rsps);

    if (c->reqx.try_swap(c->my_reqs)) {
        msg_handler handler(c);
        std::for_each(c->my_reqs.begin(), c->my_reqs.end(), boost::apply_visitor(handler));
    } else {
        //TAEL_PRINTF(&c->log, TAEL_INFO, "Didn't pick up requests.");
    }
    if (!c->my_reqs.empty()) {
        c->my_reqs.clear();
        action_gt gt; // gt == greater than
        std::sort(c->actions.begin(), c->actions.end(), boost::apply_visitor(gt));
    }
    if (!c->actions.empty() && c->dm->curtv() - c->last_act > TimeVal(0, c->act_rate *1000)) {
        apply_action a(c);
        
        boost::apply_visitor(a, c->actions.front());
        c->actions.pop_front();
        c->last_act = c->dm->curtv();
    }
    if (!c->has_reset_) {
        TAEL_PRINTF(&c->log, TAEL_INFO, "HF: resetting all targets to positions.");
        for (int i = 0; i < c->dm->cidsize(); ++i)
            c->trd->stop(i);

        c->has_reset_ = true;
        c->last_act = c->dm->curtv();
    }
}

void hflistener::update ( const OrderUpdate &ou ) {
    char buf[256];
    ou.snprint(buf, 255);
    //char *liqstr;
    char liqchar;
    std::string strat("U");
    TAEL_PRINTF(&c->log, TAEL_INFO, "%s", buf);
    if (ou.action() == Mkt::FILLED && ou.mine()) {
        int cid = ou.cid();
        double bid = 0.0, ask = 0.0;
        typed::fill f;
        
        f.clientId = ClientInfo::BROADCAST;

        switch (ou.liq()) {
            case Liq::add: f.liquidity = typed::fill::add; liqchar = 'A'; /*liqstr = "add";*/ break;
            case Liq::remove: f.liquidity = typed::fill::remove; liqchar = 'R'; /*liqstr = "remove";*/ break;
            default: f.liquidity = typed::fill::other; liqchar = 'O'; /*liqstr = "other";*/ break;
        }

        if (ou.placementAlgo_.compare("JOIN_QUEUE") == 0) strat = "J";
        if (ou.placementAlgo_.compare("FLW_LEADER") == 0) strat = "L";
        if (ou.placementAlgo_.compare("STEP_UP_L1") == 0) strat = "T";
        if (ou.placementAlgo_.compare("CROSS     ") == 0) strat = "X";
        if (ou.placementAlgo_.compare("TAKE_INVSB") == 0) strat = "I";
        if (ou.placementAlgo_.compare("FLW_SOB   ") == 0) strat = "S";
        if (ou.placementAlgo_.compare("MKTONCLOSE") == 0) strat = "M";
        f.strat = strat;
        f.orderID = ou.orderID();
        f.symbol = c->dm->symbol(cid);
        f.exchange = ECN::desc(ou.ecn());
        f.time_sec = c->dm->curtv().sec();
        f.time_usec = c->dm->curtv().usec();
        ++(c->fillIx[cid]);
        f.qtyLeft = c->trd->getTargetPosition(cid) - c->dm->position(cid);
        f.fill_size = (ou.side() == Mkt::BID? 1:-1) * ou.thisShares();
        f.fill_price = ou.thisPrice();
        c->fillCash[cid] += ou.thisPrice() * (double)ou.thisShares();
        c->fillShs[cid] += ou.thisShares();

        c->my_rsps.push_back(typed::response(f));
        getMarket(c->dm->masterBook(), cid, Mkt::BID, 0, &bid, 0);
        getMarket(c->dm->masterBook(), cid, Mkt::ASK, 0, &ask, 0);
        c->fillCounter++;
        long curr_time = ((long) f.time_sec)*1000 + (f.time_usec/1000);
        c->fillsOutStream << "F|" << c->_date << "|" << f.symbol << "|" << curr_time
        		<< "|" << f.fill_size << "|" << f.fill_price << "|" << ECN::desc(ou.ecn())
                << "|" << liqchar << "|" << f.orderID << "|" << (ou.placementAlgo_).c_str() << std::endl;
        TAEL_PRINTF(&c->costlog, TAEL_INFO, "FILL %s %s %d@%.2f (%.2f,%.2f) %c (orderID: %ld, seqnum: %d) Algo: %s",
                c->dm->symbol(cid), ECN::desc(ou.ecn()),
                f.fill_size, f.fill_price, bid, ask, liqchar, f.orderID, ou.id(), (ou.placementAlgo_).c_str());
    }
    else if (ou.action() == Mkt::FILLED && c->float_fills && !ou.mine()) {
        int cid = ou.cid();
        double bid = 0.0, ask = 0.0;
        getMarket(c->dm->masterBook(), cid, Mkt::BID, 0, &bid, 0);
        getMarket(c->dm->masterBook(), cid, Mkt::ASK, 0, &ask, 0);

        int oldtarget = c->trd->getTargetPosition(cid);
        int delta = ou.thisShares() * (ou.side() == Mkt::BID? +1 : -1);
        double aggr = c->trd->getPriority(cid);
        Mkt::Marking mark = c->trd->getShortMarking(cid);
        //NSARKAS maybe we would like to provide a meaningful orderid and clientid at this point?
        c->trd->tradeTo(cid, oldtarget + delta, aggr, (long) -2, ClientInfo::NO_CLIENT_ID, mark);
        TAEL_PRINTF(&c->costlog, TAEL_INFO, "REQ %s %d %d %d %.3f %.2f %.2f # implicit update due to %d fill",
                c->dm->symbol(cid), oldtarget, oldtarget + delta, c->dm->position(cid),
                aggr, bid, ask, delta);
    }
}

void hflistener::update ( const TimeUpdate &tu ) {
    if (tu.timer() == c->sync_timer) {
        if (c->request_syncs) {
            TAEL_PRINTF(&c->log, TAEL_INFO, "Reloading positions and locates, 'cause it's been a while...");
            bool done;
            bool pos = c->dm->getPositionsIncr(0, 50, &done);
            bool loc = c->dm->getLocatesAsync();
            TAEL_PRINTF(&c->log, TAEL_INFO, "    Get %d positions %s, get locates %s", 50, pos? "succeeded":"failed",
                    loc? "succeeded":"failed");
            if (not done) {
                TAEL_PRINTF(&c->log, TAEL_INFO, "Setting up delayed position call");
                c->sync_cid_counter = 50;
                c->sync_minor_timer = Timer(TimeVal(), TimeVal(5, 0));
                c->dm->addTimer(c->sync_minor_timer);
            }
        }
    } 
    else if (tu.timer() == c->dm->marketClose()) {
    	if (c->dm->isMOCmode()) return;
        for (int cid = 0; cid < c->dm->cidsize(); ++cid) {
            c->handler.setup_stop(cid, 0);
        }
    }
    else if (tu.timer() == c->sync_minor_timer) {
        if (c->request_syncs) {
            TAEL_PRINTF(&c->log, TAEL_INFO, "Delayed position reload");
            bool done;
            bool pos = c->dm->getPositionsIncr(c->sync_cid_counter, 50, &done);
            TAEL_PRINTF(&c->log, TAEL_INFO, "    Get %d positions %s", 50, pos? "succeeded":"failed");
            if (not done) {
                TAEL_PRINTF(&c->log, TAEL_INFO, "Setting up delayed position call");
                c->sync_cid_counter += 50;
                c->sync_minor_timer = Timer(TimeVal(), TimeVal(5, 0));
                c->dm->addTimer(c->sync_minor_timer);
            }
        }
    }
    else if (tu.timer() == c->dm->marketOpen()){
      // Get locates over TCP at market open 
      // Margin Server gets its locates ~ 9:15-9:20
      // the req @ startup might have returned all 0s
      c->dm->getLocates();
    }
    
}

void hflistener::update ( const TradeRequest & trq ) {
    c->send_info(trq._cid, trq._targetPos - c->dm->position(trq._cid), trq._priority, trq._clientId);
}

void hflistener::update ( const UserMessage &um ) {
    if (um.strategy() != c->stratcode && um.strategy() != c->stratcode + c->stratoff)
        return;

    if (um.code() == c->stopcode) {
        TAEL_PRINTF(&c->log, TAEL_INFO, "HF: Stop code %d received.", um.strategy());
        c->dm->stop("Stop code sent.");
        return;
    }

    if (um.code() == c->ratecode) {
        int rate = atoi(um.msg1());
        if (rate < 0 || rate > 1000) {
            TAEL_PRINTF(&c->log, TAEL_INFO, "HF Rate Code ignored: %d out of bounds", rate);
        } else {
            TAEL_PRINTF(&c->log, TAEL_INFO, "HF Rate Code set: %d", rate);
            c->act_rate = rate;
        }
        return;
    }

    if (um.code() == c->reloadcode) {
        TAEL_PRINTF(&c->log, TAEL_INFO, "Reloading positions and locates due to request code...");
        bool pos = c->dm->getPositions();
        bool loc = c->dm->getLocates();
        TAEL_PRINTF(&c->log, TAEL_INFO, "    Get positions %s, get locates %s", pos? "succeeded":"failed",
                loc? "succeeded":"failed");
        return;
    }

    if (um.code() == c->ecncode) {
        bool set = (um.msg1()[0] == '+');
        ECN::ECN ecn = ECN::ECN_parse(um.msg1() + 1);
        if (ecn == ECN::UNKN) return;
	
        factory<TradeConstraints>::get(only::one)->set(ecn, set);
	if( !set ) c->trd->cancelAllEcn( ecn );
	if (c->dm->POPlusMode() && (ecn==ECN::ARCA)){
	  factory<TradeConstraints>::get(only::one)->set(ECN::NYSE, set);
	  if( !set ) c->trd->cancelAllEcn( ECN::NYSE );
	}
        TAEL_PRINTF(&c->log, TAEL_INFO, "HF ECN Code received: %s -> %s %s",
                um.msg1(), set? "enable":"disable", ECN::desc(ecn));
    }
    
    if (um.code() == c->capcode) {
        int cap = atoi(um.msg1());
	 if (cap < 0 || cap > 10000) {
            TAEL_PRINTF(&c->log, TAEL_INFO, "HF Cap Code ignored: %d out of bounds", cap);
	 }else{
	   factory<CapacityTracker>::get(only::one)->set(cap);
	   TAEL_PRINTF(&c->log, TAEL_INFO, "HF CAP Code received: %s -> %d",
			 um.msg1(), cap);
	 }
    }
    
    if (um.code() == c->pcrcode) {
      int cap = atoi(um.msg1());
      if (cap < 0 || cap > 100) {
	TAEL_PRINTF(&c->log, TAEL_INFO, "HF PCR Code ignored: %d out of bounds", cap);
      }else{
      factory<CapacityTracker>::get(only::one)->setpcr(cap);
      TAEL_PRINTF(&c->log, TAEL_INFO, "HF PCR Code received: %s -> %d",
		    um.msg1(), cap);
      }
    }
    
    // user-message print PNL summary info.
    if (um.code() == c->pnlcode) {
      factory<PNLTracker>::pointer pnl = factory<PNLTracker>::find(only::one);
      if (pnl) {
	pnl->printPNL(TAEL_INFO);
      }
    }    

    // user-message print TC summary info.
    if (um.code() == c->tccode) {
      factory<TROPFTracker>::pointer tc = factory<TROPFTracker>::find(only::one);
      if (tc) {
	string prefix("");
	tc->printTCSummaryInfo(TAEL_INFO, prefix);
      }
    }
    if (um.code() == c->inviscode) {
      factory<TakeInvisibleComponent>::pointer invc = factory<TakeInvisibleComponent>::find(only::one);
      if (invc){
	int en = atoi(um.msg1());
	invc->setEnable(en>0);
	TAEL_PRINTF(&c->log, TAEL_INFO, "HF Invis Code received: %s -> %s",
		      um.msg1(), (en>0)? "enable":"disable");
      }
    }
    /*
    if (um.code() == c->crosscode) {
      factory<CrossComponent>::pointer cc = factory<CrossComponent>::find(only::one);
      if (cc){
	int wait = atoi(um.msg1());
	cc->setWait(wait);
      }
    }
    */
    if (um.code() == c->crosscode) {
      factory<CrossComponent>::pointer cc = factory<CrossComponent>::find(only::one);
      if (cc){
	int en = atoi(um.msg1());
	cc->setEnable(en>0);
	TAEL_PRINTF(&c->log, TAEL_INFO, "HF Cross Code received: %s -> %s",
		    um.msg1(), (en>0)? "enable":"disable");
      }
    }
    
    if (um.code() == c->modelcode) {
      factory<ModelTracker>::pointer _modelTracker= factory<ModelTracker>::get(only::one);
      if (_modelTracker){
	int en = atoi(um.msg1());
	if ((en<0)||(en>1)){
	  TAEL_PRINTF(&c->log, TAEL_INFO, "HF Model enable Code ignored: %d out of bounds", en);
	} else{
	  _modelTracker->enable(en>0);
	   TAEL_PRINTF(&c->log, TAEL_INFO, "HF Model Code received: %s -> %s",
		    um.msg1(), (en>0)? "enable":"disable");
	}
	
      }
    }
    
    if (um.code() == c->pcrcode) {
      int cap = atoi(um.msg1());
      if (cap < 0 || cap > 100) {
	TAEL_PRINTF(&c->log, TAEL_INFO, "HF PCR Code ignored: %d out of bounds", cap);
      }
      factory<CapacityTracker>::get(only::one)->setpcr(cap);
      TAEL_PRINTF(&c->log, TAEL_INFO, "HF PCR Code received: %s -> %d",
		    um.msg1(), cap);
    }
    
    
    if (um.code() == c->chunkcode) {
      factory<Chunk>::pointer cc = factory<Chunk>::find(only::one);
      if (cc){
	int chunksz = atoi(um.msg1());
	if ((chunksz<1) || (chunksz>20)){
	  TAEL_PRINTF(&c->log, TAEL_INFO, "HF Chunk Code ignored: %d out of bounds", chunksz*100);
	}else{
	  cc->setChunkSize(chunksz*100);
	}
	
      }
    }
    if (um.code() == c->scalecode) {
      factory<TakeInvisibleComponent>::pointer invc = factory<TakeInvisibleComponent>::find(only::one);
      double scale = atof(um.msg1());
      if ((scale<=0) || (scale>100)){
	TAEL_PRINTF(&c->log, TAEL_INFO, "HF Scale Code ignored: %f out of bounds", scale);
      }else{
	if (invc){
	  invc->setScale(scale);
	}
	factory<CrossComponent>::pointer cc = factory<CrossComponent>::find(only::one);
	if (cc){
	  cc->setScale(scale);
	}
	TAEL_PRINTF(&c->log, TAEL_INFO, "HF Scale Code received: set to %f", scale);

      }
      
    }
    if (um.code() == c->popluscode) {
      int use_po_plus_int = atoi(um.msg1());
      if (use_po_plus_int < 0 || use_po_plus_int > 1) {
        TAEL_PRINTF(&c->log, TAEL_INFO, "HF PO+ Code ignored: %d is not 0 (False) or 1 (True)",
                      use_po_plus_int);
      } else {
        TAEL_PRINTF(&c->log, TAEL_INFO, "HF PO+ Code received: set to %d", (bool)use_po_plus_int);
        c->dm->setPOPlus((bool)use_po_plus_int);
      }
    }

    if (um.code() == c->ignoretickdowncode) {
      int ignore_int = atoi(um.msg1());
      if (ignore_int < 0 || ignore_int > 1) {
        TAEL_PRINTF(&c->log, TAEL_INFO, "HF Ignore Tick Down Code ignored: %d is not 0 (False) or 1 (True)",
                      ignore_int);
      } else {
        TAEL_PRINTF(&c->log, TAEL_INFO, "HF Ignore Tick Down : set to %d", (bool)ignore_int);
	factory<FollowLeaderJoinQueueBackup>::pointer cc = factory<FollowLeaderJoinQueueBackup>::find(only::one);
	if (cc)
	  cc->setIgnoreTickDown(ignore_int);

      }
    }

    if (um.code() == c->requestsyncscode) {
      int request_syncs_int = atoi(um.msg1());
      if (request_syncs_int < 0 || request_syncs_int > 1) {
        TAEL_PRINTF(&c->log, TAEL_INFO, "HF Request Syncs Code ignored: %d is not 0 (False) or 1 (True)",
                      request_syncs_int);
      } else {
        TAEL_PRINTF(&c->log, TAEL_INFO, "HF Request Syncs Code received: set to %d", (bool)request_syncs_int);
        c->request_syncs = (bool)request_syncs_int;
      }
    }
    
    if (um.code() == c->orderprobcode){
      double prob = atof(um.msg1());
      if (prob<0 || prob>1){
	TAEL_PRINTF(&c->log, TAEL_INFO, "HF Order Probability Code ignored: %s is not valid",
                      um.msg1());
      }
      else{
	c->trd->setOrderProb( prob );
	TAEL_PRINTF(&c->log, TAEL_INFO, "HF Order Probability Code received: set to %.2f",
                      prob);
      }
    }

}

}}
