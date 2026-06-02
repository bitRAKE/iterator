#include "iterator_mru.h"
#include "iterator_mru_storage.h"

#define ITERATOR_MRU_SESSION_MAGIC 0x53534553u
#define ITERATOR_MRU_SESSION_FILE "session.mru"
#define ITERATOR_MRU_SESSION_VERSION 1u

void IteratorMruInit(HINSTANCE inst)
{
    IteratorMruStorageInit(inst);
}

int IteratorMruLoadSession(IteratorMruSessionRecord *record)
{
    int count = 0;
    IteratorMruSessionRecord r;
    if (!record)
        return 0;
    if (!IteratorMruLoadList(ITERATOR_MRU_SESSION_FILE,
                             ITERATOR_MRU_SESSION_MAGIC,
                             sizeof(r), &r, 1, &count) ||
        count < 1 ||
        r.version != ITERATOR_MRU_SESSION_VERSION)
        return 0;
    *record = r;
    return 1;
}

int IteratorMruSaveSession(const IteratorMruSessionRecord *record)
{
    IteratorMruSessionRecord r;
    if (!record)
        return 0;
    r = *record;
    r.version = ITERATOR_MRU_SESSION_VERSION;
    return IteratorMruSaveList(ITERATOR_MRU_SESSION_FILE,
                               ITERATOR_MRU_SESSION_MAGIC,
                               sizeof(r), &r, 1);
}
