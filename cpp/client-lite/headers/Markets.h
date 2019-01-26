#ifndef _MARKETS_H_
#define _MARKETS_H_

#include <Common/MktEnums.h>

namespace Mkt {
    
    /** The two sides of a market.
      * 
      * This represents the two sides of a market that exist independent of our
      * position in it: there's no concept of selling off a positive position
      * versus negative.  Most market access functions think in terms of Side.
      */
    enum Side {
        BID, ASK };
    const char *const SideDesc[] = {
        "BID", "ASK"};

    /** The three possible trade actions.
      *
      * This is different from the market side because it implies an action
      * relative to our position: i.e. you must use SHORT if you're selling into
      * a negative position.  This should only be used for placing orders.
      */
    enum Trade {
        BUY, SELL, SHORT };
    const char *const TradeDesc[] = {
        "BUY ", "SELL", "SHRT"};

    enum Marking{
      LONG_SALE,SHORT_SALE,UNKWN };
    const char *const MarkingDesc[] = {
        "LONG", "SHRT","UNKW"};
    
    enum DUType { 
        BOOK, INVTRADE, VISTRADE };
    const char *const DUTypeDesc[] = {
        "BOOK  ", "INVTRD", "VISTRD" };

    enum RunStatus {
        STOPPED, COMPLETE, FAILURE
    };


    // GVNOTE: Should probably add another order state here : Unknown for orders for which
    // we do not know the status. Right now, I made them default to DONE.
    enum OrderState {
        NEW,    OPEN,   DONE };
    const char *const OrderStateDesc[] = {
        "NEW ", "OPEN", "DONE" };

    enum OrderAction {
        PLACING, CANCELING, CONFIRMED, REJECTED, CANCELED, CXLREJECTED, FILLED, NO_UPDATE };
    const char *const OrderActionDesc[] = {
        "PLCING", "CXLING", "CONFED", "RJCTED",  "CXLED ", "CXRJED",    "FILLED", "N/A   "};

    const char *const OrderErrorDesc[] = {
        "OKAY   ", "NOSEQ  ", "NOCONF ", "NOCXL  ", "UNLINK ", "NONSHRT"};

    //GVNOTE: When we migrate from margin server to position server, several enums in this
    // class would have to change (e.g. the OrderResult enum).
    enum OrderResult {
        GOOD, UNSHORTABLE, GO_SHORT, POSITION, SIZE, BUYING_POWER, RATE, NUM_ORDERS,
        NO_ROUTE, NO_TRADING, /*MARGIN_SERVER*/POSITION_SERVER, NO_REASON };
    const char *const OrderResultDesc [] = {
        "GOOD          ",
        "UNSHORTABLE   ",
        "GO SHORT      ",
        "TOO LARGE POSN",
        "TOO LARGE ORDR",
        "NO BUYING POWR",
        "TOO FAST      ",
        "TOO MANY ORDRS",
        "ROUTE DOWN    ",
        "TRADING STOPPD",
  /*      "MARGIN SERVER ",*/
        "POSITION SERVR",
        "UNKNOWN REASON"};

    // Different types of admin messages.  Conceptually, should include:
    // Time messages:
    // - Heartbeats.
    // Market events: 
    // - Mkt open/closed.
    // - Trading in stock started/halted.
    // Data problems/events:
    // - trade route up/down.
    // - lag problems on a feed (lag up/down?)
    // - switched data provider (e.g. Tower -> Lime -> aggregator).
    // Internal Tower infra problem:
    // - Quote server up/down.
    // - Trade server up/down.
    // Problem with trades or trade server????
    // - Broken trade??
    // - This one needs to be better spec'ed out.
    // User messages.
    // Server messages (probably comes under 1 of above headings).
    // Internal to exec engine:
    // - Population set change (not in V1).
    enum AdminState {
        MARKET_OPEN, MARKET_CLOSE, TIMER, DATA_CHANGE, LAG_UP, LAG_DOWN,
        USER_MSG, SERVER_MSG };
    const char *const AdminStateDesc[] = {
        "MKTOPEN", "MKTCLSE", "TIMER  ", "DATACHG", "LAGUP  ", "LAGDOWN",
        "USERMSG", "SERVMSG" };
    // GVNOTE: Should probably expand this relatively soon to take care of newer
    // alerts and messages.


    // listing tapes
    enum Tape {
        TAPE_A, TAPE_B, TAPE_C, NUM_TAPES, TAPE_UNKN };
    
    const char *const TapeDesc[] = {
        "TAPE_A", "TAPE_B", "TAPE_C", "NUM_TAPES", "TAPE_UNKN" };

    // GVNOTE: When trade server reject reasons change (and they have changed), we must update
    // this mapping as well, and add a few more reject reasons.
    OrderResult reasonToResult ( RejectReasons reason );
};

namespace ECN {
    enum ECN {
        ISLD, // = hyp2::ISLD,
        ARCA, // = hyp2::ARCA,  displayed by hyp2 as "ARCX",
        TRAC, // = hyp2::TRAC,
        BATS, // = hyp2::BATS,
        BTRD, // = hyp2::BTRD,

        EDGA, // = hyp2::EDGA,
        EDGX, // = hyp2::EDGX,
        CBSX, // = hyp2::CBSX,
        NYSE, // = hyp2::EXCHG, !!
        BSX , // = hyp2::BSX,

        AMEX, // = hyp2::EXCHG, !!
        UNKN  // anything weird.
    };

    const int ECN_size = 12;

    /** A static, by-hand mapping of hyp2 MarketMaker enums to our ECNs.
      * This is dangerous.  It assumes a particular assignment of enum names 
      * to ints, and stores them in an array with no number or name indication.
      * We should definitely find a better solution in the near future.
      */
    const ECN mmtoECN[] = {
        ISLD, NYSE, UNKN, NYSE, UNKN,    UNKN, UNKN, UNKN, EDGA, UNKN,
        ARCA, BTRD, UNKN, NYSE, UNKN,    UNKN, UNKN, UNKN, TRAC, UNKN,
        NYSE, UNKN, UNKN, UNKN, UNKN,    EDGX, BATS, UNKN, UNKN, UNKN, 
        ISLD, CBSX, UNKN, UNKN, UNKN,    UNKN, UNKN, UNKN, UNKN, UNKN, 
        UNKN, UNKN, ISLD, UNKN, UNKN,    UNKN, UNKN, UNKN, UNKN, UNKN,
        UNKN, UNKN, UNKN, UNKN, BSX ,    UNKN, UNKN, UNKN, UNKN, UNKN,
        UNKN, UNKN, UNKN, UNKN, UNKN,    UNKN, UNKN, UNKN, UNKN, UNKN,
        UNKN,
        };

    const char * desc(ECN e);
    ECN ECN_parse ( const char *str );
};

namespace Ex {
    // Tower L1 exchange character mappings are in comments (see MktEnums.h)
    enum Ex {
//        CQS,  //E  // these three aren't "real" exchanges, and should not be seen.
//        SIP,  //E
//        CTS,  //S
        AMEX,   //A    ==0
        BSX,    //B
        NSX,    //C
        FINRA,  //D         is ADF(N), is currently EDG[AX] and off-market (1/2009)
        ISE,    //I    ==4  is owned by EDG[AX]
        CHX,    //M         is the Chicago Stock Exchange
        NYSE,   //N
        ARCA,   //P
        NASDAQ, //T/Q       both T and Q appear to be equivalent
        CBOE,   //W    ==9
        PHLX,   //X
        BATS,   //Z
        BATSY,  //Y         not yet seen (1/2009)
        EDGA,   //J         not yet seen (1/2009) 
        EDGX,   //K   ==14  not yet seen (1/2009)
        UNKN    //          Should not be seen from real data!
    };

    const int Ex_size = 16;
    const char * desc ( Ex e );
    Ex Ex_parse ( const char *str );
    Ex charToEx ( char c );
    ECN::ECN ExToECN( Ex e );
};

namespace ECN {
    Ex::Ex EcnToEx(ECN e);
};

namespace Liq {
    enum Liq { add, remove, other, UNKN };
    const char * desc ( Liq l );
    const int size = 2;
    Liq parse ( const char *str );
    Liq fromHyp2 ( Liquidity l );
}

#endif
