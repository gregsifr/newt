#ifndef __CLUTIL__FACTORY_H__
#define __CLUTIL__FACTORY_H__

#include <boost/smart_ptr.hpp>
#include <list>
#include <map>
#include <string>

#include <iostream>

namespace clite { namespace util {

    /** A "multiton" factory to build or retrieve long-lived items by a key.
     *
     *  Example usage:
     *  
     * \code
     *  // allocates new foo(1);
     *  factory<foo>::pointer foo_1ptr = factory<foo>::get(1); 
     *
     *  // allocates new foo(std::string("two"));
     *  // note that we cannot use character literals.
     *  factory<foo>::pointer foo_2ptr = factory<foo>::get(std::string("two"));
     *
     *  // returns same foo as foo1ptr
     *  factory<foo>::pointer foo_3ptr = factory<foo>::get(1);
     *
     *  // allocates new foo();
     *  factory<foo>::pointer foo_defptr = factory<foo>::get(only::one);
     *
     *  // iterates over three foos; note we get iterators to 
     *  // foo smart pointers, and no keys (they could be 
     *  // multiple types.
     *  for (factory<foo>::iterator i = factory<foo>::begin();
     *       i != factory<foo>::end(); ++) {
     *      (*i)->do_the_foo_thing();
     *  }
     *
     *
     * // set a particular key->value mapping, possibly of a subtype.
     * base_bar * bbp = new bar_impl(100);
     * if (factory<base_bar>::insert(100, bbp)) { 
     *     // success! 
     * } else { 
     *     //something is already stored in base_bar's 100 slot...
     *     delete bbp;
     * }
     *
     * // check for a value for a given key without creating it.
     * factory<base_bar>::pointer bar200 = factory<base_bar>::find(200);
     * if (bar200) {
     *    // someone has inserted a bar_impl(200)
     * } else {
     *    // bar200 == NULL
     * }
     *
     * 
     * //replace an element with a new one.
     * //always succeeds.
     * factory<foo>::pointer old_only_foo;
     * foo * new_only_foo = new foo("special foo!");
     * factory<foo>::insert(only::one, new_only_foo, &old_only_foo);
     *
     * // let go of the old foo shared pointer, so it will be destroyed.
     * old_only_foo.reset();
     *
     * \endcode
     *
     *  This exposes a map from a constructor argument to an internal
     *  shared repository of (new() allocated) objects.  Objects are
     *  returned by boost shared pointer.  
     *
     *  Objects are created (if necessary) or retrieved and returned by:
     *  factory<stored_type>::get(constructor_argument);
     *
     *  All currently constructed items can be iterated through with:
     *  factory<stored_type>::begin() and ::end();
     *
     *  A single default-constructed item can be created and retrieved
     *  by using a special argument to get:
     *  factory<default_constructible_type>::get(only::one);
     *
     *  only is a singleton struct with no object data, and a static
     *  data member of type only called one.
     *
     *  char * keys are handled specially: internally they are stored as
     *  strings, so that they will properly compare equal.  However, if
     *  you access an object with a char * key and an object with an
     *  equivalent std::string key, they will create retrieve two 
     *  distinct objects!  I hope to fix this in a future release.
     *
     *  TODO: (in order of increasing difficulty)
     *   -> support auto_ptr properly (const/ref specialization?)
     *   -> allow weak pointer option (auto-removal)
     *   -> allow multiple constructor arguments
     *   -> allow get("character literal")
     */
    template <typename product>
    class factory {

        public:
            /** The return type of get.  Acts like a pointer. */
            typedef boost::shared_ptr<product> pointer;


        private:
            typedef typename std::list<pointer> ptrlist;

            template<typename key_value_type>
            static std::map<key_value_type, pointer> *get_map ( );

            static ptrlist *get_insts ( ) {
                static ptrlist insts;
                return &insts;
            }



        public:
            /** The return type of begin() and end(); iterates over pointers. */
            typedef typename ptrlist::iterator iterator;

            /** An iterator that will access (in no order) every value stored in the map. */
            static iterator begin ( void ) { return get_insts()->begin(); }
            /** The iterator immediately following the last value in the map */
            static iterator end   ( void ) { return get_insts()->end();   }

            /** Create or retrieve a value from the map corresponding to k1. */
            template <typename K1>
            static pointer get ( const K1 &key );

            /** Retrieve a value corresponding to key if it exists, null pointer otherwise */
            template <typename K1>
            static pointer find ( const K1 &key );

            /** Insert a specific value for key, replacing and filling old_value if provided.
             *
             *  insert takes a key and a value, and optionally a buffer pointer old_value.
             *
             *  If old_value is provided, insert always succeeds, and will insert value, 
             *  filling old_value with the previous value for key.
             *
             *  If old_value is not provided, insert inserts value -> key and returns true 
             *  iff no value was previously stored for key.
             */
            template <typename K1, typename ptr_type>
            static bool insert ( K1 const &key, ptr_type const & value, pointer * old_value = 0);

            /** Remove the value corresponding to key; return true if it existed. */
            template <typename K1>
            static bool remove ( const K1 &key );
    };


    /* **** Implementation of get **** */


    /** A singleton class and object to request a default-constructed object from the factory.
     *
     *  See documentation for factory for example usage.
     */
    struct only 
    {
        private:
            only () { }
        public:
            bool operator< ( const only &other_one ) const { return false; }
            static only one;
    };

    /* A mechanism template for allowing default-construction and char * factory keys. */
    template <typename K1>
    struct factory_caster {
        typedef K1 const & key_type;
        typedef K1 key_value_type;
        static key_type make_key ( key_type k1 ) { return k1; }
        template <typename product> 
        static product *make_product ( key_type key ) { return new product(key); }
    };

    template <>
    struct factory_caster<char const *> {
        typedef std::string key_type;
        typedef std::string key_value_type;
        static key_type make_key ( char const * const &s ) { return key_type(s); }
        template <typename product> 
        static product *make_product ( key_type key ) { return new product(key); }
    };

    template <>
    struct factory_caster<char *> {
        typedef std::string key_type;
        typedef std::string key_value_type;
        static key_type make_key ( char * const &s ) { return key_type(s); }
        template <typename product> 
        static product *make_product ( key_type key ) { return new product(key); }
    };

    template <>
    struct factory_caster<only> {
        typedef const only& key_type;
        typedef only key_value_type;
        static key_type make_key ( key_type k ) { return k; }
        template <typename product> 
        static product *make_product ( key_type k ) { return new product(); }
    };

    template<typename product> template<typename key_value_type>
    std::map<key_value_type, typename factory<product>::pointer> *factory<product>::get_map ( ) {
        static std::map<key_value_type, pointer> map_;
        return &map_;
    }

    template<typename product> template<typename K1>
    typename factory<product>::pointer factory<product>::get ( const K1 &k1 ) {
        typedef typename factory_caster<K1>::key_value_type key_value_type;
        typedef typename factory_caster<K1>::key_type key_type;

        key_type key(factory_caster<K1>::make_key(k1));
        std::map<key_value_type, pointer> *map_ = get_map<key_value_type>();

        typename std::map<key_value_type, pointer>::iterator pit = map_->find(key);

        if (pit == map_->end()) {
            pointer p(factory_caster<K1>::template make_product<product>(key));
            std::pair<typename std::map<key_value_type, pointer>::iterator, bool> pres = 
                    map_->insert(std::make_pair(key, p));
            if (pres.second) { 
                get_insts()->push_back(p);
                return p;
            } else {
                // hmm, how did we get here?
                return (pres.first)->second;
            }
        } else {
            return pit->second;
        }
    }

    template <typename product> template<typename K1, typename ptr_type>
    bool factory<product>::insert ( K1 const &k1, ptr_type const &ptr, pointer *old ) {
        typedef typename factory_caster<K1>::key_value_type key_value_type;
        typedef typename factory_caster<K1>::key_type key_type;

        key_type key(factory_caster<K1>::make_key(k1));
        std::map<key_value_type, pointer> *map_ = get_map<key_value_type>();

        typename std::map<key_value_type, pointer>::iterator pit = map_->find(key);
        if (pit == map_->end()) {
            pointer p(ptr);
            std::pair<typename std::map<key_value_type, pointer>::iterator, bool> pres = 
                map_->insert(std::make_pair(key, p));
            if (pres.second) {
                get_insts()->push_back(p);
                return true;
            } else {
                // hmm, how did we get here?
                return false;
            }
        } else if (old != 0) {
            pointer p(ptr);
            *old = pit->second;
            pit->second = p;
            typename ptrlist::iterator i = std::find(get_insts()->begin(), get_insts()->end(), *old);
            if (i != get_insts()->end()) {
                *i = p;
            } // else our total-list is out of sync??
            return true;
        } else {
            return false;
        }
    }

    template <typename product> template <typename K1>
    bool factory<product>::remove ( const K1 &k1 ) {
        typedef typename factory_caster<K1>::key_value_type key_value_type;
        typedef typename factory_caster<K1>::key_type key_type;

        key_type key(factory_caster<K1>::make_key(k1));
        std::map<key_value_type, pointer> *map_ = get_map<key_value_type>();

        typename std::map<key_value_type, pointer>::iterator mit = map_->find(key);
        if (mit != map_->end()) {
            typename ptrlist::iterator lit = std::find(get_insts()->begin(), get_insts()->end(), mit->second);
            if (lit != get_insts()->end()) {
                get_insts()->erase(lit);
            } // else our total-list is out of sync??
            map_->erase(mit);
            return true;
        } else {
            return false;
        }
    }

    template <typename product> template <typename K1>
    typename factory<product>::pointer factory<product>::find ( const K1 &k1 ) {
        typedef typename factory_caster<K1>::key_value_type key_value_type;
        typedef typename factory_caster<K1>::key_type key_type;

        key_type key(factory_caster<K1>::make_key(k1));
        std::map<key_value_type, pointer> *map_ = get_map<key_value_type>();

        typename std::map<key_value_type, pointer>::iterator pit = map_->find(key);
        if (pit == map_->end()) {
            return pointer();
        } else {
            return pit->second;
        }
    }

} }

#endif // __CLUTIL__FACTORY_H__
