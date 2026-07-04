#define WIN32_LEAN_AND_MEAN
#include "iterator_mru_storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ITERATOR_MRU_FILE_VERSION 1u
#define ITERATOR_MRU_HEADER_TAG "ITMRU1"

typedef struct IteratorMruHeader {
    char tag[8];
    uint32_t version;
    uint32_t magic;
    uint32_t item_size;
    uint32_t max_items;
    uint32_t count;
} IteratorMruHeader;

static char g_state_dir[MAX_PATH];

static void make_state_path(char *dst, size_t dst_size, const char *name)
{
    snprintf(dst, dst_size, "%s\\%s", g_state_dir, name);
}

void IteratorMruStorageInit(HINSTANCE inst)
{
    char exe[MAX_PATH];
    char *slash;
    (void)inst;

    exe[0] = 0;
    GetModuleFileNameA(NULL, exe, (DWORD)sizeof(exe));
    slash = strrchr(exe, '\\');
    if (slash)
        *slash = 0;
    else
        lstrcpynA(exe, ".", (int)sizeof(exe));

    snprintf(g_state_dir, sizeof(g_state_dir), "%s\\state", exe);
    CreateDirectoryA(g_state_dir, NULL);
}

const char *IteratorMruStateDir(void)
{
    return g_state_dir;
}

int IteratorMruLoadList(const char *name, uint32_t magic, uint32_t item_size,
                        void *items, int max_items, int *count_out)
{
    char path[MAX_PATH];
    IteratorMruHeader h;
    FILE *f;
    int count;

    if (count_out)
        *count_out = 0;
    if (!items || max_items <= 0 || item_size == 0)
        return 0;

    make_state_path(path, sizeof(path), name);
    f = fopen(path, "rb");
    if (!f)
        return 0;

    if (fread(&h, sizeof(h), 1, f) != 1) {
        fclose(f);
        return 0;
    }
    if (memcmp(h.tag, ITERATOR_MRU_HEADER_TAG, strlen(ITERATOR_MRU_HEADER_TAG)) != 0 ||
        h.version != ITERATOR_MRU_FILE_VERSION ||
        h.magic != magic ||
        h.item_size != item_size) {
        fclose(f);
        return 0;
    }

    if (h.count > (uint32_t)max_items)
        count = max_items;
    else
        count = (int)h.count;
    if (count > 0 && fread(items, item_size, (size_t)count, f) != (size_t)count) {
        fclose(f);
        return 0;
    }
    fclose(f);
    if (count_out)
        *count_out = count;
    return count > 0;
}

int IteratorMruSaveList(const char *name, uint32_t magic, uint32_t item_size,
                        const void *items, int count)
{
    char path[MAX_PATH];
    IteratorMruHeader h;
    FILE *f;

    if (count < 0 || item_size == 0 || (count > 0 && !items))
        return 0;

    make_state_path(path, sizeof(path), name);
    f = fopen(path, "wb");
    if (!f)
        return 0;

    memset(&h, 0, sizeof(h));
    memcpy(h.tag, ITERATOR_MRU_HEADER_TAG, strlen(ITERATOR_MRU_HEADER_TAG));
    h.version = ITERATOR_MRU_FILE_VERSION;
    h.magic = magic;
    h.item_size = item_size;
    h.max_items = (uint32_t)count;
    h.count = (uint32_t)count;

    if (fwrite(&h, sizeof(h), 1, f) != 1) {
        fclose(f);
        return 0;
    }
    if (count > 0 && fwrite(items, item_size, (size_t)count, f) != (size_t)count) {
        fclose(f);
        return 0;
    }
    fclose(f);
    return 1;
}

int IteratorMruPromote(const char *name, uint32_t magic, uint32_t item_size,
                       int max_items, const void *item, IteratorMruCompareFn cmp)
{
    unsigned char *old_items;
    unsigned char *new_items;
    int count = 0;
    int i;
    int out_count = 0;

    if (!item || item_size == 0 || max_items <= 0)
        return 0;

    old_items = (unsigned char *)calloc((size_t)max_items, item_size);
    new_items = (unsigned char *)calloc((size_t)max_items, item_size);
    if (!old_items || !new_items) {
        free(old_items);
        free(new_items);
        return 0;
    }

    IteratorMruLoadList(name, magic, item_size, old_items, max_items, &count);
    memcpy(new_items, item, item_size);
    out_count = 1;

    for (i = 0; i < count && out_count < max_items; ++i) {
        const void *old_item = old_items + (size_t)i * item_size;
        int same = cmp ? cmp(item, old_item) : (memcmp(item, old_item, item_size) == 0);
        if (!same) {
            memmove(new_items + (size_t)out_count * item_size, old_item, item_size);
            ++out_count;
        }
    }

    i = IteratorMruSaveList(name, magic, item_size, new_items, out_count);
    free(old_items);
    free(new_items);
    return i;
}
