#include <cl-util/factory.h>
#include "fact1.h"

#include <cstdio>
using namespace clite::util;

test1::test1 ( ) {
    f0 = factory<foo>::get(only::one);
    f1 = factory<foo>::get(1);
    f2 = factory<foo>::get(2);
    f3 = factory<foo>::get(3);
}

void test1::go () {
    f0->y += 1;
    f1->y += 1;
    f2->y += 1;
    f3->y += 1;
    
    printf("0: %d -> %d, 1: %d -> %d, 2: %d -> %d, 3: %d -> %d \n",
            f0->x, f1->y, f1->x, f1->y, f2->x, f2->y, f3->x, f3->y);
}


