#include "iterator_mru.h"
#include "iterator_mru_storage.h"

#include <math.h>

#define ITERATOR_MRU_STABLE_MAGIC 0x53544359u
#define ITERATOR_MRU_STABLE_FILE "stable_cycles.mru"
#define ITERATOR_MRU_STABLE_VERSION 1u

static int same_stable_cycle(const void *a, const void *b)
{
    const IteratorMruStableCycleRecord *x = (const IteratorMruStableCycleRecord *)a;
    const IteratorMruStableCycleRecord *y = (const IteratorMruStableCycleRecord *)b;
    int i;
    if (x->family != y->family ||
        x->period != y->period ||
        x->point_count != y->point_count ||
        x->param != y->param)
        return 0;
    for (i = 0; i < x->point_count; ++i) {
        if (x->orbit[i] != y->orbit[i])
            return 0;
    }
    return 1;
}

int IteratorMruAddStableCycle(const IteratorMruStableCycleRecord *record)
{
    IteratorMruStableCycleRecord r;
    if (!record || record->period < 1 || record->point_count < 1 ||
        record->point_count > ITERATOR_MRU_CYCLE_POINT_MAX ||
        !isfinite(record->param) ||
        !isfinite(record->multiplier) ||
        fabs(record->multiplier) >= 1.0)
        return 0;
    r = *record;
    r.version = ITERATOR_MRU_STABLE_VERSION;
    return IteratorMruPromote(ITERATOR_MRU_STABLE_FILE,
                              ITERATOR_MRU_STABLE_MAGIC,
                              sizeof(r),
                              ITERATOR_MRU_STABLE_CYCLE_MAX,
                              &r,
                              same_stable_cycle);
}

int IteratorMruLoadStableCycles(IteratorMruStableCycleRecord *records, int max_records, int *count_out)
{
    int count = 0;
    int i;
    if (!records || max_records <= 0)
        return 0;
    if (!IteratorMruLoadList(ITERATOR_MRU_STABLE_FILE,
                             ITERATOR_MRU_STABLE_MAGIC,
                             sizeof(records[0]),
                             records,
                             max_records,
                             &count))
        return 0;
    for (i = 0; i < count; ++i) {
        if (records[i].version != ITERATOR_MRU_STABLE_VERSION ||
            records[i].point_count < 1 ||
            records[i].point_count > ITERATOR_MRU_CYCLE_POINT_MAX) {
            count = i;
            break;
        }
    }
    if (count_out)
        *count_out = count;
    return count > 0;
}
