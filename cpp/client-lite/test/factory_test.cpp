#include <cl-util/factory.h>

#include <string>
#include <iostream>

using namespace clite::util;
using namespace std;

struct foo { 
    string s;
    int x;
    foo (int x_ ) : s(""), x(x_) { 
        cout << "created   " << s << ": " << x << endl;
    } 
    foo (string s_ ) : s(s_), x(0) { 
        cout << "created   " << s << ": " << x << endl;
    } 
    ~foo ( ) { 
        cout << "destroyed " << s << ": " << x << endl;
    }
};

class bar {
    public:
    int i;
    int j;
    bar ( int i ) : i(i), j(i) { }
    virtual void go ( ) = 0;
    virtual ~bar ( ) { }
};

class sub_bar : public bar {
    public:
    sub_bar ( int i ) : bar(i) { }
    virtual void go ( ) { cout << "sub_bar" << ": " << i << ", " << j << endl; }
    virtual ~sub_bar ( ) 
    {
        cout << "deleting! ";
        go();
    }
};

ostream &operator<<( ostream &os, const foo &f) { 
    os << f.s << ": " << f.x; 
    return os; 
}

int main ( void ) {
    factory<bar>::iterator it = factory<bar>::begin();
    factory<bar>::pointer barp = factory<bar>::find(1);
    if (!barp) {
        factory<bar>::insert(1, new sub_bar(1));
    }
    factory<bar>::find(1)->go();

    sub_bar *sb2 = new sub_bar(1);
    sb2->j = 2;
    if (factory<bar>::insert(1, sb2)) {
        cout << "inserted duplicate sub_bar!" << endl;
    } else {
        cout << "did not insert dupe sub_bar" << endl;
    }
    factory<bar>::find(1)->go();
    factory<bar>::pointer old_sb1;
    if (factory<bar>::insert(1, sb2, &old_sb1)) {
        cout << "replaced sub_bar 1." << endl;
    } else { 
        cout << "replace returned false!" << endl;
    }
    bar * abar (new sub_bar(10));
    abar->go();
    if (factory<bar>::insert(1, abar)) {
        cout << "abar inserted!" << endl;
    } else {
        cout << "abar not inserted!" << endl;
        abar->go();
    }

    old_sb1->go();
    factory<bar>::pointer bp = factory<bar>::find(1);
    bp->go();


    for (factory<bar>::iterator bi = factory<bar>::begin();
            bi != factory<bar>::end(); ++bi) {
        cout << "iterating... ";
        (*bi)->go();
    }

    factory<foo>::pointer one = factory<foo>::get(string("one"));
    one->x = 1;
    cout << *one << endl;
    factory<foo>::pointer three = factory<foo>::get(3);
    three->s = "three";
    cout << *three << endl;
    three.reset();
    factory<foo>::pointer two = factory<foo>::get(string("two"));
    two->x = 2;
    cout << *two << endl;

    if (factory<foo>::remove(3)) {
        cout << "foo 3 removed" << endl;
    } else {
        cout << "foo 3 not removed" << endl;
    }
    factory<foo>::pointer one2 = factory<foo>::get(string("one"));
    cout << *one2 << endl;

    for (factory<foo>::iterator i = factory<foo>::begin();
            i != factory<foo>::end();
            ++i) {
        cout << *(*i) << endl;
    }

}
