#ifndef __WAKEUP_TIMER_H__
#define __WAKEUP_TIMER_H__

#include <DataManager.h>
#include <cl-util/factory.h>
#include <cl-util/debug_stream.h>

#include <c_util/Histogram.h>
using trc::compat::util::Histogram;

#include <c_util/Buckets.h>
using trc::compat::util::UniformBucketizer;
using trc::compat::util::CountingBucket;

#include "CycleCount.h"

#include <string>
#include <iostream>

using infra_timing::getCycleCount;

class WakeupTimer : public clite::message::dispatch<WakeUpdate>::listener
                  , public clite::message::dispatch<DataUpdate>::listener 
                  , public clite::message::dispatch<UserMessage>::listener 
                  , public Configurable 
{

    clite::util::factory<DataManager>::pointer  dm;
    clite::util::factory<debug_stream>::pointer log;

    uint64_t start;
    double cyc_us;
    unsigned count;
    double length;
    int scode;
    int ct;
    typedef Histogram<double, UniformBucketizer<double>, CountingBucket > d_hist_t;
    typedef Histogram<int, UniformBucketizer<int>, CountingBucket > i_hist_t;
    d_hist_t *lags;
    i_hist_t *counts;

    string filestr;

    public:
    WakeupTimer ( ) 
        :Configurable("timer"), dm(clite::util::factory<DataManager>::get(only::one)), 
        start(0), ct(0), lags(0), counts(0)
    {
        defOption("clock-mhz", &cyc_us, "Clockspeed in MHz");
        defOption("buckets", &count, "# of histogram buckets", 100u);
        defOption("bucket-size", &length, "length of bucket (us)", 10.0);
        defOption("strategy", &scode, "Strategy #", 2000900);
    }

    ~WakeupTimer  ( ) { 
        //fld.close();
        delete lags;
        delete counts;
    }
    void init ( ) {
        dm->add_listener(this);
        lags = new d_hist_t(UniformBucketizer<double>(count, 0, count*length));
        counts = new i_hist_t(UniformBucketizer<int>(count, 0, count-1));
        log = clite::util::factory<debug_stream>::get( std::string("wakeup-timer") );
    }
    
    void update ( const WakeUpdate &w ) {
        if (start != 0) {
            lags->insert( (getCycleCount() - start)/(double)cyc_us );
            counts->insert(ct);
            ct = 0;
            start = 0;
        }
    }

    void update ( const DataUpdate &du ) {
        if (start == 0) start = getCycleCount();
        ct++;
    }

    void update ( const UserMessage &um ) {
        if (um.strategy() == scode) {
            TAEL_PRINTF(log.get(), TAEL_INFO, "\n\n # Lags at %d.%06d", (int) dm->curtv().sec(), (int) dm->curtv().usec());
            for (unsigned i = 0; i <= count; ++i) {
                TAEL_PRINTF(log.get(), TAEL_INFO, "%d %.1f %d", i, i * length, (*lags)[i].count);
            }
            lags->clear();
            TAEL_PRINTF(log.get(), TAEL_INFO, "\n\n # Counts at %d.%06d", (int) dm->curtv().sec(), (int) dm->curtv().usec());
            for (unsigned i = 0; i <= count; ++i) {
                TAEL_PRINTF(log.get(), TAEL_INFO, "%d %d %d", i, i, (*counts)[i].count);
            }
            counts->clear();
        }
    }
};

#endif
