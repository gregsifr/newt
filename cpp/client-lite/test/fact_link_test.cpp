#include "fact1.h"
#include "fact2.h"

#include <cstdio>

using namespace clite::util;
using namespace std;

int main ( void ) {

    test1 t1;
    test2 t2;

    t1.go();
    t2.go();
    t1.go();
    t2.go();

    for (factory<foo>::iterator i = factory<foo>::begin(); i != factory<foo>::end(); ++i) {
        printf("%d -> %d\n", (*i)->x, (*i)->y);
    }


};
