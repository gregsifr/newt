#ifndef _FACT2_
#define _FACT2_
#include <cl-util/factory.h>
#include "factbase.h"

struct test2 {
    clite::util::factory<foo>::pointer f0, f1, f2, f3;

    void go ();
    test2 ();
};

#endif
