#ifndef ITERATOR_MRU_STORAGE_H
#define ITERATOR_MRU_STORAGE_H

#include <windows.h>
#include <stdint.h>

typedef int (*IteratorMruCompareFn)(const void *a, const void *b);

void IteratorMruStorageInit(HINSTANCE inst);
const char *IteratorMruStateDir(void);
int IteratorMruLoadList(const char *name, uint32_t magic, uint32_t item_size,
                        void *items, int max_items, int *count_out);
int IteratorMruSaveList(const char *name, uint32_t magic, uint32_t item_size,
                        const void *items, int count);
int IteratorMruPromote(const char *name, uint32_t magic, uint32_t item_size,
                       int max_items, const void *item, IteratorMruCompareFn cmp);

#endif
