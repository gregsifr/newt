#include <Markets.h>

#include <Common/MktEnums.h>

#include <cstring>

namespace ECN {
    const char * desc(ECN e) {
        switch (e) {
            case ECN::ISLD: return "ISLD";
            case ECN::ARCA: return "ARCA";
            case ECN::TRAC: return "TRAC";
            case ECN::BATS: return "BATS";
            case ECN::BTRD: return "BTRD";
            case ECN::EDGA: return "EDGA";
            case ECN::EDGX: return "EDGX";
            case ECN::CBSX: return "CBSX";
            case ECN::NYSE: return "NYSE";
            case ECN::BSX:  return "BSX ";
            case ECN::AMEX: return "AMEX";
            default:   return "*NA*";
        }
    }

    ECN ECN_parse ( const char *str ) {
        if (strncasecmp("ISLD", str, 5) == 0) return ECN::ISLD;
        if (strncasecmp("ARCA", str, 5) == 0) return ECN::ARCA;
        if (strncasecmp("TRAC", str, 5) == 0) return ECN::TRAC;
        if (strncasecmp("BATS", str, 5) == 0) return ECN::BATS;
        if (strncasecmp("BTRD", str, 5) == 0) return ECN::BTRD;
        if (strncasecmp("EDGA", str, 5) == 0) return ECN::EDGA;
        if (strncasecmp("EDGX", str, 5) == 0) return ECN::EDGX;
        if (strncasecmp("CBSX", str, 5) == 0) return ECN::CBSX;
        if (strncasecmp("NYSE", str, 5) == 0) return ECN::NYSE;
        if (strncasecmp("BSX",  str, 5) == 0) return ECN::BSX;
        if (strncasecmp("AMEX", str, 5) == 0) return ECN::AMEX;
        return ECN::UNKN;
    }
    Ex::Ex EcnToEx(ECN e) {
        switch (e) {
            case ISLD: return Ex::NASDAQ;
            case ARCA: return Ex::ARCA;
            case BATS: return Ex::BATS;
            case EDGA: return Ex::EDGA;
            case EDGX: return Ex::EDGX;
            case NYSE: return Ex::NYSE;
            case BSX:  return Ex::BSX;
            case AMEX: return Ex::AMEX;
            default:   return Ex::UNKN;
        }
    }
};

namespace Ex {
    const char * desc ( Ex ex ) {
        switch (ex) {
            case AMEX  : return "AMEX";
            case BSX   : return "BSX ";
            case NSX   : return "NSX ";
            case FINRA : return "FNRA";
            case ISE   : return "ISE ";
            case CHX   : return "CHX ";
            case NYSE  : return "NYSE";
            case ARCA  : return "ARCA";
            case NASDAQ: return "NSDQ";
            case CBOE  : return "CBOE";
            case PHLX  : return "PHLX";
            case BATS  : return "BATS";
            case BATSY : return "BTSY";
            case EDGA  : return "EDGA";
            case EDGX  : return "EDGX";
            case UNKN  : return "UNKN";
            default: return "*NA*";
        }
    }

    Ex Ex_parse ( const char * str ) {
        if (strncasecmp("AMEX", str, 5) == 0) return AMEX;
        if (strncasecmp("BSX",  str, 5) == 0) return BSX;
        if (strncasecmp("NSX",  str, 5) == 0) return NSX;
        if (strncasecmp("FNRA", str, 5) == 0) return FINRA;
        if (strncasecmp("ISE",  str, 5) == 0) return ISE;
        if (strncasecmp("CHX",  str, 5) == 0) return CHX;
        if (strncasecmp("NYSE", str, 5) == 0) return NYSE;
        if (strncasecmp("ARCA", str, 5) == 0) return ARCA;
        if (strncasecmp("NSDQ", str, 5) == 0) return NASDAQ;
        if (strncasecmp("CBOE", str, 5) == 0) return CBOE;
        if (strncasecmp("PHLX", str, 5) == 0) return PHLX;
        if (strncasecmp("BATS", str, 5) == 0) return BATS;
        if (strncasecmp("BTSY", str, 5) == 0) return BATSY;
        if (strncasecmp("EDGA", str, 5) == 0) return EDGA;
        if (strncasecmp("EDGX", str, 5) == 0) return EDGX;
        return UNKN;
    }

    Ex charToEx ( char c ) {
        switch (c) {
            case 'A': return AMEX  ;
            case 'B': return BSX   ;
            case 'C': return NSX   ;
            case 'D': return FINRA ;
            case 'I': return ISE   ;
            case 'M': return CHX   ;
            case 'N': return NYSE  ;
            case 'P': return ARCA  ;
            case 'Q':        
            case 'T': return NASDAQ;
            case 'W': return CBOE  ;
            case 'X': return PHLX  ;
            case 'Z': return BATS  ;
            case 'Y': return BATSY ;
            case 'J': return EDGA  ;
            case 'K': return EDGX  ;
            default: return UNKN;
        }
    }
    ECN::ECN ExToECN( Ex e ) {
        switch (e) {
            case AMEX  : return ECN::AMEX;
            case BSX   : return ECN::BSX;
            case NYSE  : return ECN::NYSE;
            case ARCA  : return ECN::ARCA;
            case BATS  : return ECN::BATS;
            case EDGA  : return ECN::EDGA;
            case EDGX  : return ECN::EDGX;
            case NASDAQ: return ECN::ISLD;

            default: return ECN::UNKN;
        }
    }
};

Mkt::OrderResult Mkt::reasonToResult ( RejectReasons reason ) {
    switch (reason) {
        // probably one-off errors; ignore.
        case REJECT_DUPLICATE_ID:
        case REJECT_CONFLICT_ORDER:
        case REJECT_FULLY_EXECUTED:
        case REJECT_ALREADY_CANCELLED:
        case REJECT_ALREADY_OUT:
        case REJECT_ALREADY_REJECTED:
        case REJECT_DUP_SEQNUM:
        case REJECT_PRICE:
        case REJECT_NO_SUCH_ORDER:
            return Mkt::GOOD;

            // can't borrow / locate:
        case REJECT_HARD_TO_BORROW:
        case REJECT_NON_EASY_TO_BORROW:
        case REJECT_NONSHORTABLE:
        case REJECT_THRESHOLD:
            return Mkt::UNSHORTABLE;

            // should be going short:
        case REJECT_CAUSES_SHORT_POS:
        case REJECT_SHORT_SALE:
            return Mkt::GO_SHORT;

        case REJECT_NOT_FLATTENED:
//            return Mkt::MARGIN_SERVER;
            return Mkt::POSITION_SERVER;

        case REJECT_BUYING_POWER:
            return Mkt::BUYING_POWER;

        case REJECT_MAX_ORDER_RATE:
            return Mkt::RATE;

        case REJECT_MAX_ORDER_VALUE:
        case REJECT_MAX_ORDER_SIZE:
        case REJECT_QUANTITY:
            return Mkt::SIZE;

        case REJECT_MAX_POSITION:
        case REJECT_MINMAX_POSITION:
        case REJECT_MAX_POSITION_VALUE:
            return Mkt::POSITION;

            // that market or route is having issues?
        case REJECT_NO_ROUTE:
        case REJECT_MARKET_CONGESTED:
        case REJECT_MARKET_DOWN:
        case REJECT_DISCONNECTED:
        case REJECT_SESSION_LEVEL:
            return Mkt::NO_ROUTE;

        case REJECT_NO_MKT_PRICE:
        case REJECT_SYMBOL_NOT_OPEN:
            return Mkt::NO_TRADING;

        case REJECT_DOWN_TICK:
        case REJECT_NOT_EXECUTABLE:
        case REJECT_NO_REASON:
        case REJECT_ODD_LOT:
        case REJECT_SECURITY_MASTER:
        case REJECT_EXPIRE_TIME:
        case NUM_REJECT_REASONS:
            return Mkt::NO_REASON;
    }

    // should never come here
    return Mkt::NO_REASON;
}

namespace Liq {
    const char *desc ( Liq l ) {
        switch (l) {
            case add:    return "Add";
            case remove: return "Rem";
            case other:  return "Spc";
            default:     return "*NA";
        }
    }
    Liq parse ( const char * str ) {
        if (strncasecmp("Add", str, 4) == 0) return add;
        if (strncasecmp("Spc", str, 4) == 0) return other;
        if (strncasecmp("Rem", str, 4) == 0) return remove;
        return UNKN;

    }
    Liq fromHyp2 ( Liquidity l ) {
        switch (l) {
            case REMOVE_LIQUIDITY: return remove;
            case ADD_LIQUIDITY:    return add;
            case ARCA_NYSE_ADD_LIQUIDITY: return add;
            case ARCA_NYSE_REMOVE_LIQUIDITY: return remove;
            default:               return other;
        };
    }
}


