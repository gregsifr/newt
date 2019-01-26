#ifndef _INTERCHANGE_H_
#define _INTERCHANGE_H_
#include <deque>

#include <c_util/Lock.h>
using trc::compat::util::Mutex;
using trc::compat::util::Lock;

class TryLock {
    Mutex &m;
    bool held;
    public:
        explicit TryLock(Mutex &m) : m (m), held (m.tryacquire()) { }
        ~TryLock ( ) { if (held) { m.release(); held = false; } }
        operator bool () { return held; }
};

template <typename T> 
class mxdeque {

    public: 
    typedef typename std::deque<T> deque;

    private:
    deque d;
    Mutex m;

    public:

    bool try_swap ( deque &old ) {
        TryLock l(m);
        if (!l) return false;
        if (! (d.empty() ^ old.empty())) return false;
         d.swap(old);
        return true;
    }
    bool swap ( deque &old ) {
        Lock l(m);
        if (! (d.empty() ^ old.empty())) return false;
        d.swap(old);
        return true;
    }
};

#endif
