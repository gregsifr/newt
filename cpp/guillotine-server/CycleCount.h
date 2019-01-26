#ifndef INFRA_TIMING_CYCLE_COUNT_H
#define INFRA_TIMING_CYCLE_COUNT_H

#include <stdint.h>

namespace infra_timing {


// Returns the current cycle count, as measured by rdtsc.
static inline uint64_t getCycleCount();





#if defined __i386__
#define _N_PERF_MACROS_HAVE_RDTSC64
        static inline uint64_t getCycleCount ( void )
        { 
            uint64_t A;
            __asm__ volatile (".byte 0x0f, 0x31" : "=A" (A));
            return A;
        }
#endif // __i386__
        
#if defined __x86_64__
#define _N_PERF_MACROS_HAVE_RDTSC64
        static inline uint64_t getCycleCount ( void )
        {
            uint64_t a, d;
            __asm__ __volatile__ ("rdtsc" : "=a" (a), "=d" (d));
            return (a | (d<<32));
        }
#endif // __x86_64__

#if defined __powerpc__
#define _N_PERF_MACROS_HAVE_RDTSC64
        static inline uint64_t getCycleCount ( void )
        {
            uint64_t hi, lo, ignore;
            __asm__ volatile (
                "0:                     \n"
                "\tmftbu   %0           \n"
                "\tmftb    %1           \n"
                "\tmftbu   %2           \n"
                "\tcmpw    %2,%0        \n"
                "\tbne     0b           \n"
                : "=r"(hi),"=r"(lo),"=r"(ignore)
            );
            return ((hi << 32) | lo);
        }
#endif // __powerpc__

#ifndef _N_PERF_MACROS_HAVE_RDTSC64
#warning "no rdtsc equivalent found for this architecture."
#warning "please add one to CycleCount.cpp.  using stub function (return 0) instead."
        static inline uint64_t getCycleCount ( void )
        {
            return 0;
        }
#endif

} /* namespace infra_timing */


#endif /* INFRA_TIMING_CYCLE_COUNT_H */
