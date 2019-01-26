#include <cstdio>
#include <cl-util/float_cmp.h>

using namespace clite::util;

template <int N>
void comp ( double x, double y ) {
    typedef cmp<N> Cmp;
    int d = N+1;

    printf(" -- eps %.*6$f => %.*6$f (%d) ?? (%d) %.*6$f -- \n", Cmp::eps, x, Cmp::idx(x), Cmp::idx(y), y, d);
    printf(" %.*6$f (%d) %.*6$f +1 %.*6$f, -1 %.*6$f \n", x, Cmp::idx(x), Cmp::round(x), Cmp::inc(x), Cmp::dec(x), d);
    printf(" %.*6$f (%d) %.*6$f +1 %.*6$f, -1 %.*6$f \n", y, Cmp::idx(y), Cmp::round(y), Cmp::inc(y), Cmp::dec(y), d);
    printf(" %.*4$f  < %.*4$f is %s \n", x, y, Cmp::lt(x, y)?"true":"false", d);
    printf(" %.*4$f  > %.*4$f is %s \n", x, y, Cmp::gt(x, y)?"true":"false", d);
    printf(" %.*4$f <= %.*4$f is %s \n", x, y, Cmp::le(x, y)?"true":"false", d);
    printf(" %.*4$f >= %.*4$f is %s \n", x, y, Cmp::ge(x, y)?"true":"false", d);
    printf(" %.*4$f == %.*4$f is %s \n", x, y, Cmp::eq(x, y)?"true":"false", d);
    printf("\n");
}

int main () {
    comp<2>(-1.01, -1.02);
    comp<2>(-1.01, -1.021);
    comp<2>(-1.01, -1.019);
    comp<2>(-1.01, -1.01);
    comp<2>(-1.01, -1.011);
    comp<2>(-1.01, -1.009);
    comp<2>(-1.01, -1.00);
    comp<2>(-1.01, -1.001);
    comp<2>(-1.01, -0.999);
    comp<2>(1.01, 1.02);
    comp<2>(1.01, 1.021);
    comp<2>(1.01, 1.019);
    comp<2>(1.01, 1.01);
    comp<2>(1.01, 1.011);
    comp<2>(1.01, 1.009);
    comp<2>(1.01, 1.00);
    comp<2>(1.01, 1.001);
    comp<2>(1.01, 0.999);
}

