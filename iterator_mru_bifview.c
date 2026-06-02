#include "iterator_mru.h"
#include "iterator_mru_storage.h"

#include <math.h>

#define ITERATOR_MRU_BIF_MAGIC 0x42494656u
#define ITERATOR_MRU_BIF_FILE "bif_views.mru"
#define ITERATOR_MRU_BIF_VERSION 1u

static int same_bif_view(const void *a, const void *b)
{
    const IteratorMruBifViewRecord *x = (const IteratorMruBifViewRecord *)a;
    const IteratorMruBifViewRecord *y = (const IteratorMruBifViewRecord *)b;
    return x->family == y->family &&
           x->param_min == y->param_min &&
           x->param_max == y->param_max &&
           x->x_min == y->x_min &&
           x->x_max == y->x_max;
}

int IteratorMruAddBifView(const IteratorMruBifViewRecord *record)
{
    IteratorMruBifViewRecord r;
    if (!record)
        return 0;
    r = *record;
    r.version = ITERATOR_MRU_BIF_VERSION;
    if (!isfinite(r.param_min) || !isfinite(r.param_max) ||
        !isfinite(r.x_min) || !isfinite(r.x_max))
        return 0;
    return IteratorMruPromote(ITERATOR_MRU_BIF_FILE,
                              ITERATOR_MRU_BIF_MAGIC,
                              sizeof(r),
                              ITERATOR_MRU_BIF_VIEW_MAX,
                              &r,
                              same_bif_view);
}

int IteratorMruLoadBifViews(IteratorMruBifViewRecord *records, int max_records, int *count_out)
{
    int count = 0;
    int i;
    if (!records || max_records <= 0)
        return 0;
    if (!IteratorMruLoadList(ITERATOR_MRU_BIF_FILE,
                             ITERATOR_MRU_BIF_MAGIC,
                             sizeof(records[0]),
                             records,
                             max_records,
                             &count))
        return 0;
    for (i = 0; i < count; ++i) {
        if (records[i].version != ITERATOR_MRU_BIF_VERSION) {
            count = i;
            break;
        }
    }
    if (count_out)
        *count_out = count;
    return count > 0;
}
