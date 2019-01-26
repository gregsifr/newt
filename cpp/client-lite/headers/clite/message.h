#ifndef __MESSAGE_LOOP_H__
#define __MESSAGE_LOOP_H__

#include <list>
#include <deque>
#include <algorithm>

namespace clite { namespace message {

class dispatch_base {

    public:
    static bool block;

    protected:
    class listener_base {
        public:
        listener_base ( ) { }
        virtual ~listener_base ( ) { }
    };
    public:
    dispatch_base ( ) { }
    virtual void add_listener ( listener_base *lb, listener_base *lb_after ) = 0;
    virtual void add_listener_front ( listener_base *lb ) = 0;
    virtual void remove_listener ( listener_base *lb ) = 0;
    virtual void deliver ( ) = 0;

    virtual ~dispatch_base ( ) { }
};

template <typename T>
class dispatch : public dispatch_base {

    public:
    class listener : public virtual dispatch_base::listener_base {
        public:
        virtual void update ( const T & ) = 0;
        virtual ~listener ( ) { };
    };
    typedef T message;

    private:
    typedef std::deque<T> tdeque;
    typedef std::list<listener *> llist;

    tdeque pending;
    llist ls;
    dispatch ( const dispatch<T> &other ) { } //not cool

    public:
    dispatch ( ) { }
    ~dispatch ( ) { }
    void add_listener ( dispatch_base::listener_base *lb, dispatch_base::listener_base *lb_after );
    void add_listener_back ( dispatch_base::listener_base *lb ) {
        add_listener(lb, 0); //XXX: not the point.
    }
    void add_listener_front ( dispatch_base::listener_base *lb );
    void remove_listener ( dispatch_base::listener_base *lb );
    void deliver ( );
    void send ( const T &t);
};

// GVNOTE: Do we really want to have a list of dispatchers, with each listener added to each
// dispatcher? We will end up calling the update function on each listener as many times as
// the number of dispatchers. I doubt if in actual practice we'll have multiple dispatchers.
// This seems unnecessarily complicated.

// GVNOTE: Made coordinator inherit from dispatch_base to be able to use the protected class
// listener_base. Using virtual inheritance here (and in listener above) to avoid the diamond
// problem.

// GVNOTE: Perhaps listener and listener_base above should be classes in their own right, and
// outside of the definition of the dispatch and dispatch_base classes.
template <class wakeup_dispatch>
class coordinator: public virtual dispatch_base {

    typedef std::list<dispatch_base *> dblist;
    typedef std::list<dispatch_base::listener_base *> llist;


    dblist dbs;
    llist ls;
    wakeup_dispatch wh;

    protected:
    virtual bool advise_wakeup () = 0;
    virtual typename wakeup_dispatch::message wakeup_message () { return typename wakeup_dispatch::message(); }

    public:
    void add_listener ( dispatch_base::listener_base *lb, dispatch_base::listener_base *after = 0) {

        llist::iterator li;

        if (after) { 
            li = std::find(ls.begin(), ls.end(), after);
        }
        if (after && li != ls.end()) {
            ++li;
            ls.insert(li, lb);
        } else {
            ls.push_back(lb);
        }

        for (dblist::iterator i = dbs.begin(); i != dbs.end(); ++i)
            (*i)->add_listener(lb, after);
        wh.add_listener(lb, after);
    }
    void add_listener_back ( dispatch_base::listener_base *lb) {
        add_listener(lb, 0);
    }
    void add_listener_front ( dispatch_base::listener_base *lb ) {
        ls.push_front(lb);
        for (dblist::iterator i = dbs.begin(); i != dbs.end(); ++i)
            (*i)->add_listener_front(lb);
        wh.add_listener_front(lb);
    }
    void remove_listener ( dispatch_base::listener_base *lb ) {
        for (dblist::iterator i = dbs.begin(); i != dbs.end(); ++i)
            (*i)->remove_listener(lb);
        wh.remove_listener(lb);
        llist::iterator li = std::find(ls.begin(), ls.end(), lb);
        if (li != ls.end()) { ls.erase(li); }
    }
    void add_dispatch ( dispatch_base *db ) { 
        dbs.push_back(db); 
        for (llist::iterator li = ls.begin(); li != ls.end(); ++li) {
            db->add_listener(*li, 0);
        }
    }
    void remove_dispatch ( dispatch_base *db ) { 
        dblist::iterator i = std::find(dbs.begin(), dbs.end(), db);
        if (i != dbs.end()) dbs.erase(i);
    }
    void deliver ( ) {
        if (dispatch_base::block) return;
        for (dblist::iterator i = dbs.begin(); i != dbs.end(); ++i) {
            (*i)->deliver();
        }
        if (advise_wakeup()) {
            wh.send(wakeup_message());
            for (dblist::iterator i = dbs.begin(); i != dbs.end(); ++i) {
                (*i)->deliver();
            }
        }
    }
    virtual ~coordinator ( ) { }
};

template <typename T>
void dispatch<T>::add_listener ( dispatch_base::listener_base *lb, 
        dispatch_base::listener_base *lb_after = 0 ) {
    listener *l, *after;
    l = dynamic_cast<listener *>(lb);
    after = dynamic_cast<listener *>(lb_after);

    // GV: The dynamic_cast here is the reason why I think we can get away with not
    // defining an update(WakeUpdate) for each class that adds itself as a listener
    // to a dm object. If a class doesn't care about wakeup, it doesn't define the
    // update function, and hence doesn't get added as a listner for the wakeup_dispatcher.
    if (!l) {
    	//printf("In dispatcher<T>::add_listener - unable to add listener to dispatcher, "
    	//		"since a dynamic_cast from listener_base to listener gives a null!\n");
        return;
    }

    if(after) {
        typename llist::iterator al = std::find(ls.begin(), ls.end(), after);
        if (al != ls.end()) {
            ls.insert(al++, l);
            return;
        }
    }

    ls.push_back(l);
    }

template <typename T>
void dispatch<T>::add_listener_front ( dispatch_base::listener_base *lb ) {
    listener *l = dynamic_cast<listener *>(lb);
    if (l) {
        ls.push_front(l);
    }
}

template <typename T>
void dispatch<T>::remove_listener ( dispatch_base::listener_base *lb ) {
    listener *l = dynamic_cast<listener *>(lb);
    if (!l) {
        return;
    }
    typename llist::iterator al = std::find(ls.begin(), ls.end(), l);
    if (al != ls.end()) {
        ls.erase(al);
    }
}

template <typename T>
void dispatch<T>::deliver ( ) {
    for (typename tdeque::iterator u = pending.begin(); u != pending.end(); ++u)
        for (typename llist::iterator l = ls.begin(); l != ls.end(); ++l)
            (*l)->update(*u);
    pending.clear();
}

template <typename T>
void dispatch<T>::send ( const T &t) {
    if (block || !pending.empty()) {
        pending.push_back(t);
    } else {
        // GVNOTE: This implementation does not seem thread safe. Either switch to using
        // mutex, or re-think whether we need to make this thread safe or not.
        block = true;
        for (typename llist::iterator l = ls.begin(); l != ls.end(); ++l)
            (*l)->update(t);
        block = false;
    }
}

} }

#endif
