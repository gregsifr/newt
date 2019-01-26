#ifndef _FACTBASE_
#define _FACTBASE_

struct foo {
    int x;
    int y;
    foo (int x_) : x(x_), y(0) { }
    foo () : x(0), y(0) { }
};

#endif // _FACTBASE_
