#ifndef _FACT1_
#define _FACT1_
#include <cl-util/factory.h>
#include "factbase.h"

struct test1 {
    clite::util::factory<foo>::pointer f0, f1, f2, f3;

    test1 ();
    void go ();
};

#endif
