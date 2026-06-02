#include "iterator_mru.h"
#include "iterator_mru_storage.h"

#define ITERATOR_MRU_CYCLE_MAGIC 0x43594350u
#define ITERATOR_MRU_CYCLE_FILE "cycle_periods.mru"
#define ITERATOR_MRU_CYCLE_VERSION 1u

static int same_cycle_period(const void *a, const void *b)
{
    const IteratorMruCyclePeriodRecord *x = (const IteratorMruCyclePeriodRecord *)a;
    const IteratorMruCyclePeriodRecord *y = (const IteratorMruCyclePeriodRecord *)b;
    return x->family == y->family && x->period == y->period;
}

int IteratorMruAddCyclePeriod(const IteratorMruCyclePeriodRecord *record)
{
    IteratorMruCyclePeriodRecord r;
    if (!record || record->period < 1)
        return 0;
    r = *record;
    r.version = ITERATOR_MRU_CYCLE_VERSION;
    return IteratorMruPromote(ITERATOR_MRU_CYCLE_FILE,
                              ITERATOR_MRU_CYCLE_MAGIC,
                              sizeof(r),
                              ITERATOR_MRU_CYCLE_PERIOD_MAX,
                              &r,
                              same_cycle_period);
}

int IteratorMruLoadCyclePeriods(IteratorMruCyclePeriodRecord *records, int max_records, int *count_out)
{
    int count = 0;
    int i;
    if (!records || max_records <= 0)
        return 0;
    if (!IteratorMruLoadList(ITERATOR_MRU_CYCLE_FILE,
                             ITERATOR_MRU_CYCLE_MAGIC,
                             sizeof(records[0]),
                             records,
                             max_records,
                             &count))
        return 0;
    for (i = 0; i < count; ++i) {
        if (records[i].version != ITERATOR_MRU_CYCLE_VERSION) {
            count = i;
            break;
        }
    }
    if (count_out)
        *count_out = count;
    return count > 0;
}
