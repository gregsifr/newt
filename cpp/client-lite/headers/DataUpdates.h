#ifndef _DATAUPDATES_H_
#define _DATAUPDATES_H_

#include <Common/MktEnums.h>
#include <Markets.h>
#include <clite/message.h>

#include <c_util/Time.h>
using trc::compat::util::TimeVal;

#include <stdint.h>

#include <Client/lib3/ordermanagement/common/Order.h>

class DataManager;

// GVNOTE: The members of this class like type, ecn, side, etc are directly set in functions
// like DataManager::buildImpTick, DataManager::onBookChange, etc. Should probably change
// this and define get / set functions and/or modify the constructor to take more params.
class DataUpdate {
    const DataManager *dm;
    public:
    Mkt::DUType type;
    ECN::ECN ecn;
    Mkt::Side side;
    int cid;
    int size;
    uint64_t id;

    /*
      Times at which events were originated (at exchange), and received by the HF infra, respectively.
    */
    TimeVal tv;
    TimeVal addtv;
    double price;

    bool isTrade ( ) const  { return type == Mkt::VISTRADE || type == Mkt::INVTRADE; } 
    bool isBook ( ) const  { return !isTrade(); }
    bool isVisibleTrade() const {return type == Mkt::VISTRADE;}
    bool isInvisibleTrade() const {return type == Mkt::INVTRADE;}

    int snprint ( char *s, int n ) const;

    friend bool operator== (const DataUpdate &du1, const DataUpdate &du2);
    friend bool operator!= (const DataUpdate &du1, const DataUpdate &du2);
    DataUpdate (const DataManager *dm) :dm(dm) { }
    DataUpdate () :dm(0) { }
};

typedef clite::message::dispatch<DataUpdate> MarketHandler;

// Only for SIAC & UTDF trade ticks. Exchange trade ticks are handled using DataUpdate above.
class TapeUpdate {
    const DataManager *dm;
    Ex::Ex ex_;
    int cid_;
    int size_;
    TimeVal tv_;
    double px_;

    //friend bool operator== ( const TapeUpdate &a, const TapeUpdate &b );
    //friend bool operator!= ( const TapeUpdate &a, const TapeUpdate &b );
    friend class DataManager;
    TapeUpdate ( DataManager *dm, Ex::Ex ex, int cid, int size, TimeVal tv, double px) 
        : dm(dm), ex_(ex), cid_(cid), size_(size), tv_(tv), px_(px)
    { }

    public:
    inline Ex::Ex ex() const { return ex_; }
    inline int cid() const { return cid_; }
    inline int size() const { return size_; }
    inline double price() const { return px_; }
    inline TimeVal tv() const { return tv_; }
    int snprint ( char *s, int n ) const;
};
typedef clite::message::dispatch<TapeUpdate> TapeHandler;

class Order;
/** OrderUpdates are all the information we expose about an order.
  *
  * Update is kind of a misnomer, because they contain all the info that the
  * DataManager by default tracks and exposes to the user.  It probably keeps
  * the OrderUpdates in a set internally, and just hands out references thereto.
  */
class OrderUpdate { 

	// GVNOTE: Add orderID to an OrderUpdate message
	long orderID_;

    DataManager *dm;
    Mkt::OrderAction action_;
    Mkt::OrderResult error_;

    int size_, id_, cid_, timeout_;
    double price_;
    bool inv_, mine_;
    ECN::ECN ecn_;
    Mkt::Trade dir_;

    int upshs_, cxlshs_, fillshs_;
    int64_t exid_;
    // GVNOTE: Looks like fee_ and fees_ are never set.
    double uppx_, fee_, fees_;
    TimeVal tv_;
    Liq::Liq liq_;
    public:

    std::string placementAlgo_;
    OrderUpdate () : dm(0), action_(Mkt::NO_UPDATE) { }

    int snprint ( char *buf, int n ) const; 

    /* Updating information: not stored by the "immutable" order */

    inline Mkt::OrderAction action ( ) const { return action_; }
    inline Mkt::OrderState  state  ( ) const { 
        switch (action_) {
            case Mkt::PLACING:
            case Mkt::NO_UPDATE:
                return Mkt::NEW;
            case Mkt::CANCELING:
            case Mkt::CONFIRMED:
            case Mkt::CXLREJECTED:
                return Mkt::OPEN;
            case Mkt::FILLED:
                return sharesOpen() > 0? Mkt::OPEN:Mkt::DONE;
            case Mkt::CANCELED:
            case Mkt::REJECTED:
                return Mkt::DONE;
        }
        // GVNOTE: Adding this to get rid of compiler warning. Should never reach here.
        return Mkt::DONE;
    }
    inline Mkt::OrderResult error  ( ) const { return error_; }

    inline int sharesCanceled ( ) const { return cxlshs_; }
    inline int sharesFilled   ( ) const { return fillshs_; }
    inline int sharesOpen     ( ) const { return size_ - (fillshs_ + cxlshs_); }

    inline int thisShares     ( ) const { return upshs_; }
    inline double thisPrice   ( ) const { return uppx_; }

    inline double totalFees   ( ) const { return fees_; }
    inline double thisFee     ( ) const { return action_ == Mkt::FILLED? fee_ : 0.0; } 

    inline TimeVal tv ( ) const { return tv_; }

    /* Order-derived information */

    inline long orderID ( ) const { return orderID_; }
    inline int size     ( ) const { return size_; }
    inline int id       ( ) const { return id_; }
    inline int64_t exchangeID ( ) const { return exid_; }
    inline double price ( ) const { return price_; }
    inline int cid      ( ) const { return cid_; }
    inline ECN::ECN ecn ( ) const { return ecn_; }
    inline Mkt::Trade dir ( ) const { return dir_; }
    inline Mkt::Side side ( ) const { return dir_==Mkt::BUY ? Mkt::BID : Mkt::ASK; }
    inline bool invisible ( ) const { return inv_; }
    inline int timeout ( ) const { return timeout_; }
    inline Liq::Liq liq ( ) const { return liq_; }
    inline bool mine ( ) const { return mine_; }

    inline operator bool ( ) const { return action_ != Mkt::NO_UPDATE; }
    void init ( Order const *o, DataManager *dm, Mkt::OrderAction act, Mkt::OrderResult err,
            int64_t exid, int upshs, int fillshs, Liq::Liq l, bool mine, int cxlshs, double uppx, 
            TimeVal tv, long orderID );
};

class Order : public lib3::CustomPlugin<Order> {
    friend class DataManager;

    lib3::Order const *lo_;
    int size_, id_, cid_, timeout_;
    double price_;
    bool inv_, mine_;
    ECN::ECN ecn_;
    ECN::ECN realecn_;
    Mkt::Trade dir_;
    bool poplus_;
    long orderID_;
    OrderUpdate plreq, plrsp, cxreq, cxrsp, flrsp;
    OrderUpdate *last;

    public:

    std::string placementAlgo;
    inline Mkt::OrderAction action ( ) const { return last->action(); }
    inline Mkt::OrderState  state  ( ) const { return last->state(); }
    inline Mkt::OrderResult error  ( ) const { return last->error(); }

    inline OrderUpdate const &placing   ( ) const { return plreq; }
    inline OrderUpdate const &confirmed ( ) const { return plrsp; }
    inline OrderUpdate const &canceling ( ) const { return cxreq; }
    inline OrderUpdate const &canceled  ( ) const { return cxrsp; }
    inline OrderUpdate const &filled    ( ) const { return flrsp; }

    inline OrderUpdate const &lastUpdate( ) const { return *last; }

    inline int size           ( ) const { return size_; }
    inline int sharesOpen     ( ) const { return last->sharesOpen(); }
    inline int sharesFilled   ( ) const { return last->sharesFilled(); }
    inline int sharesCanceled ( ) const { return last->sharesCanceled(); }
    inline int id             ( ) const { return id_; }
    inline double price       ( ) const { return price_; }
    inline int cid            ( ) const { return cid_; }
    inline ECN::ECN ecn       ( ) const { return poplus_ ? ECN::NYSE : ecn_; }
    inline ECN::ECN realecn   ( ) const { return realecn_; }
    inline Mkt::Trade dir     ( ) const { return dir_; }
    inline Mkt::Side side     ( ) const { return dir_==Mkt::BUY ? Mkt::BID : Mkt::ASK; }
    inline bool invisible     ( ) const { return inv_; }
    inline bool mine          ( ) const { return mine_; }
    inline int timeout        ( ) const { return timeout_; }
    inline long orderID       ( ) const { return orderID_; }
    inline void setRealEcn (MarketMaker realmm) { realecn_ = ECN::mmtoECN[realmm]; }

    bool isCanceling() const; /// is there an "outstanding cancel-request"?
         
    Order ( ) { };
    void init ( lib3::Order const *lo, long orderID, const char* placeAlgo );
    int snprint ( char *buf, int n ) const;
    static Order *allocate ();
};

typedef clite::message::dispatch<OrderUpdate> OrderHandler;

/** Admin updates, including state of data feeds and markets, and Tower alerts.
  *
  * This is a fun one.  Every update includes a state message, which is one of
  * the Mkt::AdminState --- these should be self-explanatory --- plus two
  * TimeVals: one is the current time, and one is the next time that this thing
  * might happen, if we know that: in particular, it's when the next market
  * status change (i.e. open, close) should happen, or when the next timer will
  * be set off.
  *
  * The info union is where the mess happens.  Each admin update is either a
  * data update, or a user or server message.  States of USER_MSG and SERVER_MSG
  * are the latter two; all other updates are the data update.
  *
  * A data update includes the ECN to which it applies, and the feed source that
  * that ECN is currently using (oldfeed).  If this update indicates that the
  * feed source is changing, the new feedsource is also included (newfeed).
  *
  * Note that only universal listeners get admin updates, since admin updates
  * aren't tied to a particular cid.  This might change, because it's not
  * exactly right.
  *
  * XXX: Trading halted, trading restarted, initial NYSE trade info?
  * 
  */

class UserMessage { 
    char msg1_[32];
    char msg2_[32];
    int code_, strategy_;
    TimeVal tv_;
    public:
    UserMessage ( const char *m1, const char *m2, int c, int s, TimeVal const &tv ) {
        strncpy(msg1_, m1, 32);
        strncpy(msg1_, m1, 32);
        msg1_[31] = 0;
        msg2_[31] = 0;
        code_ = c;
        strategy_ = s;
        tv_ = tv;
    }
    inline int code() const { return code_; }
    inline int strategy() const { return strategy_; }
    inline const char * msg1() const { return msg1_; }
    inline const char * msg2() const { return msg2_; }
    inline const TimeVal & tv() const { return tv_; }
    int snprint ( char *s, int n ) const ;
};

typedef clite::message::dispatch<UserMessage> UserMessageHandler;

class Timer {
    // period and phase
    // if period == (0,0), this is a one-off firing at phase
    TimeVal period_, phase_;
    static inline uint64_t usof( const TimeVal t ) { return t.sec() * 1000000 + t.usec(); }
    static inline TimeVal  tvof( const uint64_t t ) { return TimeVal(t / 1000000, t %1000000); }
    
    public:
        
        Timer ( const TimeVal &period_, const TimeVal &phase ) : period_(period_), phase_(phase) { }
        Timer ( const TimeVal &oneoff ) : period_(), phase_(oneoff) { }
        Timer ( const Timer &t ) : period_(t.period_), phase_(t.phase_) { }
        Timer & operator = ( const Timer &t ) { 
            period_ = t.period_; phase_ = t.phase_;
            return *this;
        }
        inline bool isOneoff ( ) const { return period_ == TimeVal(); }
        inline TimeVal period ( ) const { return period_; }
        inline TimeVal phase  ( ) const { return isOneoff()?  TimeVal() : phase_; }
        inline TimeVal oneoff ( ) const { return !isOneoff()? TimeVal() : phase_; }
        inline TimeVal nextAfter ( const TimeVal & tv ) const {
            if (isOneoff()) {
                return tv > phase_? TimeVal() : phase_;
            } else {
                uint64_t tv_u = usof(tv);
                uint64_t ph_u = usof(phase_);
                uint64_t pd_u = usof(period_);
                uint64_t x = tv_u - (tv_u % pd_u) + ph_u;

                return tvof(x > tv_u? x : x + pd_u);
            }
        }
        inline bool operator == ( const Timer &r ) const
        { return period_ == r.period_ && phase_ == r.phase_; }
        inline bool operator != ( const Timer &r ) const
        { return period_ != r.period_ || phase_ != r.phase_; }
        inline bool operator < ( const Timer &r ) const
        { return period_ < r.period_ || (period_ == r.period_ && phase_ < r.phase_); }
};

class TimeUpdate {
    Timer   timer_;
    TimeVal tv_;
    public:
    TimeUpdate ( Timer t, TimeVal tv ) : timer_(t), tv_(tv) { }
    inline Timer const & timer ( ) const { return timer_; }
    inline TimeVal const & tv ( ) const { return tv_; }
    // priority queue less-than is flipped so that soonest is on top.
    bool operator < ( const TimeUpdate &r ) const { return tv_ > r.tv_; }
    int snprint ( char *s, int n ) const;
};

typedef clite::message::dispatch<TimeUpdate> TimeHandler;

class WakeUpdate {
    public:
    TimeVal tv;
    WakeUpdate ( ) { }
    WakeUpdate ( const TimeVal &tv ) : tv(tv) { }
};

typedef clite::message::dispatch<WakeUpdate> WakeupHandler;

/** Base class for listening to updates.

    The various update methods are called in semi real-time, aka as
    updates are actually pulled off of the bus.  Aka, processing of
    them should be light-weight only.

    wakeup will be called by the DataManager when it believes it is a good time
    to do heavier processing.  In a live manager, this will be when the event
    queue is empty; in historical mode, it may be periodic or something else.

    Subclasses of updatelistener may have more complex wakeup semantics.
*/
class UpdateListener :
    public MarketHandler::listener, 
    public OrderHandler::listener, 
    public WakeupHandler::listener
{
 public:
  
  virtual void wakeup ( ) = 0;
  virtual void update ( const DataUpdate & ) { }
  virtual void update ( const OrderUpdate & ) { }
  virtual void update ( const WakeUpdate & w ) { wakeup(); }
  virtual ~UpdateListener ( ) { }
};

#endif
