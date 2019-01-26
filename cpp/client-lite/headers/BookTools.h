#ifndef _BOOKTOOLS_H_
#define _BOOKTOOLS_H_

#include <Markets.h>
#include <Client/lib3/bookmanagement/common/Book.h>
#include <Client/lib3/bookmanagement/common/BookOrder.h>
#include <Client/lib3/bookmanagement/common/BookLevel.h>

#include <cl-util/debug_stream.h>
#include <cl-util/factory.h>
#include <cl-util/float_cmp.h>
/*
virtual bool getTradeableMarket(unsigned int cid, Mkt::Side side, int level, 
        int minSize, bool aggregateVolume,
        double *price = NULL, int *size = NULL);
*/

/** Check whether we have good data for a current combined market.
 *
 * The exact meaning of this function will possibly be different among
 * implementors, but it should generally indicate that we have no
 * reason to think that information is missing.  Subclasses will
 * probably want to make finer points of this call configurable.
 */

template<typename OT> 
bool validMarket ( lib3::Book<OT> *book, int cid ) {
    using namespace lib3;
    using clite::util::factory;
    using clite::util::debug_stream;

    if (!book) return false;

    double bid = 0.0, ask = 0.0;
    size_t bs, as;
    typename BookLevel<OT>::OrderList *blvl, *alvl;

    blvl = book->get_nth_mkt(TradeType(Mkt::BID), cid, 0, &bid, &bs);
    alvl = book->get_nth_mkt(TradeType(Mkt::ASK), cid, 0, &ask, &as);

    return blvl && alvl && clite::util::cmp<3>::LT(bid, ask) && bs != 0 && as != 0;
}

/** Get the combined price and size of a combined market.
 *
 * Callers request a cid, side, and level (where 0 is the top of the
 * book), and pass a pointer to a double and/or a pointer to an int to
 * be filled in for price and size respectively.  The function returns
 * whether that market level exists.
 */
template<typename OT> 
bool getMarket ( lib3::Book<OT> *book, int cid, Mkt::Side side, 
		 int level, double *price = NULL, size_t *size = NULL ) 
{
    using namespace lib3;
    using clite::util::factory;
    using clite::util::debug_stream;
    double px;
    size_t sz;
    double *pxp = price? price : &px;
    size_t *szp = size ? size  : &sz;
    if (!book) return false;

    typedef typename BookLevel<OT>::OrderList OL;
    OL *ol  = book->get_nth_mkt(TradeType(side), cid, level, pxp, szp);
    /* Turning off Bookdebug: The problem still exists
    if (ol && *szp == 0) {
        factory<debug_stream>::pointer d = factory<debug_stream>::get(std::string("bookdebug"));
        TAEL_PRINTF(d.get(), TAEL_ERROR, "#%d at %s level %d: (getMarket)", cid, Mkt::SideDesc[side], level);
        TAEL_PRINTF(d.get(), TAEL_ERROR, "    lvl %c = %lu @ $%.2f", ol == 0? 'F':'T', *szp, *pxp);
        TAEL_PRINTF(d.get(), TAEL_ERROR, "%s", book->print_book(cid, level+1));
    }
    */
    return ol && !ol->empty() && *szp != 0;
}

template <typename OT>
size_t getMarketSize ( lib3::Book<OT> *book, int cid, Mkt::Side s ) 
{
    if (!book) return 0;
    return book->bh_size[s][cid];
}

template <typename OT>
int getMarketSize ( lib3::Book<OT> *book, int cid, Mkt::Side side, double price ) 
{
    using namespace lib3;
    typedef typename clite::util::cmp<3> cmp;

    double px;
    size_t sz;
    typename BookLevel<OT>::OrderList *ol;

    if (!book) return 0;

    size_t level = 0;
    do {
        ol = book->get_nth_mkt(TradeType(side), cid, level++, &px, &sz);
        if (!ol || cmp::GT(px, price) && side == Mkt::ASK
                || cmp::LT(px, price) && side == Mkt::BID) {
            return 0;
        } else if (cmp::EQ(px, price)) {
            return sz;
        }
    } while (ol);
    return 0;
}


/// size of specified level
template <typename OT>
size_t getMarketSize ( lib3::Book<OT> *book, int cid, Mkt::Side s, int l ) 
{
    using namespace lib3;
    if (!book) return 0;
    typename Book<OT>::BookHalfIter it = book->side[s][cid]->begin();
    
   int i = 0;
    while( it != book->side[s][cid]->end() ) {
      if( i==l ) return (*it)->size;
      it++;
      i++;
    }
    return 0;
}

/// Cumulated size on all levels which are more aggressive or equal to 'px'
template <typename OT>
size_t getMarketCumSize ( lib3::Book<OT> *book, int cid, Mkt::Side s, double px ) 
{
    using namespace lib3;
    typedef clite::util::cmp<3> cmp;

    if (!book) return 0;
    size_t res = 0;
    typename Book<OT>::BookHalfIter it = book->side[s][cid]->begin();
    
    while (it != book->side[s][cid]->end() && 
	   ((s == Mkt::BID && cmp::GE((*it)->px,px))
             || (s == Mkt::ASK && cmp::LE((*it)->px,px)))) {
        res += (*it)->size;
	it++;
    }

    return res;
}

/// Cumulated size on all levels which are more aggressive or equal to 'l'
template <typename OT>
size_t getMarketCumSize ( lib3::Book<OT> *book, int cid, Mkt::Side s, int l ) 
{
    using namespace lib3;
    if (!book) return 0;
    size_t res = 0;
    typename Book<OT>::BookHalfIter it = book->side[s][cid]->begin();
    
    int i = 0;
    while (it != book->side[s][cid]->end() && i <= l) {
        res += (*it)->size;
	it++;
	i++; 
    }

    return res;
}
/// Cumulated size on all levels which are more aggressive or equal to 'l'
template <typename OT>
size_t getMarketCumSize ( lib3::Book<OT> *book, int cid, Mkt::Side s ) 
{
    using namespace lib3;
    if (!book) return 0;
    return book->bh_size[s][cid];
}

/// total number of orders in the book of cid (both sides)
template <typename OT>
size_t getMarketOrders ( lib3::Book<OT> *book, int cid ) 
{
    if (!book) return 0;
    return book->bh_num_orders[Mkt::BID][cid] + book->bh_num_orders[Mkt::ASK][cid];
}

template <typename OT>
size_t getMarketOrders ( lib3::Book<OT> *book, int cid, Mkt::Side s ) 
{
    if (!book) return 0;
    return book->bh_num_orders[s][cid];
}

/// # orders in the price
template <typename OT>
size_t getMarketOrders ( lib3::Book<OT> *book, int cid, Mkt::Side s, double px ) 
{
    using namespace lib3;
    typedef clite::util::cmp<3> cmp;

    if (!book) return 0;
    typename Book<OT>::BookHalfIter it = book->side[s][cid]->begin();
    
    while (it != book->side[s][cid]->end() && 
	    ((s == Mkt::BID && cmp::GT((*it)->px,px))
             || (s == Mkt::ASK && cmp::LT((*it)->px,px)))) {
	it++;
    }
    if( it == book->side[s][cid]->end() || !cmp::EQ((*it)->px, px) )
      return 0;

    return (*it) -> num_orders;
}

/// # orders in the level ('l' is assumed to be non-negative)
template <typename OT>
size_t getMarketOrders ( lib3::Book<OT> *book, int cid, Mkt::Side s, int l ) 
{
    using namespace lib3;
    if (!book) return 0;
    typename Book<OT>::BookHalfIter it = book->side[s][cid]->begin();

    int i = 0;
    while( it != book->side[s][cid]->end() ) {
      if( i==l ) return (*it)->num_orders;
      it++;
      i++;
    }
    return 0;
}

/// # orders in more or equally aggressive to price
template <typename OT>
size_t getMarketCumOrders ( lib3::Book<OT> *book, int cid, Mkt::Side s, double px ) 
{
    using namespace lib3;
    typedef clite::util::cmp<3> cmp;

    if (!book) return 0;
    size_t res = 0;
    typename Book<OT>::BookHalfIter it = book->side[s][cid]->begin();
    
    while (it != book->side[s][cid]->end() && 
            ((s == Mkt::BID && cmp::GE((*it)->px, px))
             || (s == Mkt::ASK && cmp::LE((*it)->px, px)))) {
        res += (*it)->num_orders;
	it++;
    }

    return res;
}

/// # orders in more or equally aggressive level
template <typename OT>
size_t getMarketCumOrders ( lib3::Book<OT> *book, int cid, Mkt::Side s, int l ) 
{
    using namespace lib3;
    if (!book) return 0;
    size_t res = 0;
    typename Book<OT>::BookHalfIter it = book->side[s][cid]->begin();
    
    int i = 0;
    while (it != book->side[s][cid]->end() && i <= l) {
        res += (*it)->num_orders;
	it++;
	i++;
    }

    return res;
}

/// Get a list of orders in a fixed market, cid, side, price
template <typename OT>
std::pair<typename lib3::BookLevel<OT>::OrderListIter, typename lib3::BookLevel<OT>::OrderListIter> getOrdersList ( lib3::Book<OT> *book, 
														    int cid, Mkt::Side s, 
														    double px )
{
    using namespace lib3;
    typedef clite::util::cmp<3> cmp;

    typename BookLevel<OT>::OrderListIter oli;
    if (!book) return std::make_pair( oli, oli );

    std::pair<typename BookLevel<OT>::OrderListIter, typename BookLevel<OT>::OrderListIter> olip;
    typename Book<OT>::BookHalfIter it = book->side[s][cid]->begin();

    while (it != book->side[s][cid]->end() &&
            ((s == Mkt::BID && cmp::GT((*it)->px,px))
             || (s == Mkt::ASK && cmp::LT((*it)->px,px)))) {
        it++;
    }
    if( it == book->side[s][cid]->end() || !cmp::EQ((*it)->px, px) )
      return std::make_pair( oli, oli );

    return std::make_pair( (*it)->orders.begin(), (*it)->orders.end() );
}

template <typename OT, typename FN>
typename FN::result_type reduceMarket ( FN &fn, typename FN::result_type base, 
        lib3::Book<OT> *book, int cid, Mkt::Side s, int l )
{
    using namespace lib3;

    if (!book) return base;

    size_t sz;
    double px;

    typedef typename BookLevel<OT>::OrderList OL; 
    OL *ol = book->get_nth_mkt((TradeType)s, cid, l, &px, &sz);
    if (!ol) return base;

    for (typename OL::iterator it = ol->begin(); it != ol->end(); ++it) {
        base = fn(*it, base);
    }

    return base;
}
template <typename OT, typename FN>
typename FN::result_type reduceMarket ( FN &fn, typename FN::result_type base,
        lib3::Book<OT> *book, int cid, Mkt::Side s, double px ) 
{
    using namespace lib3;
    typedef clite::util::cmp<3> cmp;

    if (!book) return base;
    typename Book<OT>::BookHalfIter it = book->side[s][cid]->begin();
    
    while (it != book->side[s][cid]->end() && 
	    ((s == Mkt::BID && cmp::GT((*it)->px,px))
             || (s == Mkt::ASK && cmp::LT((*it)->px,px)))) {
	it++;
    }
    if( it == book->side[s][cid]->end() || !cmp::EQ((*it)->px, px) )
      return base;

    typedef typename BookLevel<OT>::OrderList OL; 
    for (typename OL::iterator lit = (*it)->orders.begin(); lit != (*it)->orders.end(); ++lit) {
        base = fn(*lit, base);
    }
    return base;
}

template <typename OT, typename FN>
typename FN::result_type reduceMarketCum ( FN &fn, typename FN::result_type base,
        lib3::Book<OT> *book, int cid, Mkt::Side s )
{
    using namespace lib3;
    if (!book) return base;

    typename Book<OT>::BookHalfIter it = book->side[s][cid]->begin();
    
    while (it != book->side[s][cid]->end()) {
        typename BookLevel<OT>::OrderList::iterator lit = (*it)->orders.begin();
        for (; lit != (*it)->orders.end(); ++lit)
            base = fn(*lit, base);
	it++;
    }
    return base;
}

template <typename OT, typename FN>
typename FN::result_type reduceMarketCum ( FN &fn, typename FN::result_type base,
        lib3::Book<OT> *book, int cid, Mkt::Side s, int maxl )
{
    using namespace lib3;
    if (!book) return base;

    typename Book<OT>::BookHalfIter it = book->side[s][cid]->begin();
    
    int i = 0;
    while (it != book->side[s][cid]->end() && i <= maxl) {
        typename BookLevel<OT>::OrderList::iterator lit = (*it)->orders.begin();
        for (; lit != (*it)->orders.end(); ++lit)
            base = fn(*lit, base);
	it++;
	i++;
    }
    return base;
}

// GVNOTE: Its not clear why we need a while loop below. We should probably
// just query for the needed level. Right now, in case we can't find the required
// level, we will end up setting the px and sz to 0.
template<typename OT>
bool getTradableMarket ( lib3::Book<OT> *book, int cid, Mkt::Side side, 
        int lvl, size_t minsz, double *px = 0, size_t *sz = 0 ) {
    int i = 0;
    while (getMarket(book, cid, side, i, px, sz)) {
        if (*sz >= minsz && lvl-- == 0) return true;
        else ++i;
    }
    return false;
};

#endif // _BOOKTOOLS_H_
