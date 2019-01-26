#include <cl-util/factory.h>
#include "fact2.h"

#include <cstdio>

using namespace clite::util;

test2::test2 ( ) {
    f0 = factory<foo>::get(only::one);
    f1 = factory<foo>::get(1);
    f2 = factory<foo>::get(2);
    f3 = factory<foo>::get(3);
}

void test2::go() {
    f0->y += 10;
    f1->y += 10;
    f2->y += 10;
    f3->y += 10;
    
    printf("0: %d -> %d, 1: %d -> %d, 2: %d -> %d, 3: %d -> %d \n",
            f0->x, f0->y, f1->x, f1->y, f2->x, f2->y, f3->x, f3->y);
}


