#ifndef ITERATOR_MRU_H
#define ITERATOR_MRU_H

#include <windows.h>
#include <stdint.h>

#define ITERATOR_MRU_GRAPH_COUNT 4
#define ITERATOR_MRU_BIF_VIEW_MAX 32
#define ITERATOR_MRU_CYCLE_PERIOD_MAX 16
#define ITERATOR_MRU_STABLE_CYCLE_MAX 64
#define ITERATOR_MRU_CYCLE_POINT_MAX 12

typedef struct IteratorMruWindowRecord {
    int visible;
    int left;
    int top;
    int right;
    int bottom;
} IteratorMruWindowRecord;

typedef struct IteratorMruSessionRecord {
    uint32_t version;
    int family;
    double param;
    double view_min;
    double view_max;
    double bif_x_min;
    double bif_x_max;
    double x0;
    int show_transient;
    int lyap_overlay;
    int period_colors;
    int find_period;
    IteratorMruWindowRecord windows[ITERATOR_MRU_GRAPH_COUNT];
} IteratorMruSessionRecord;

typedef struct IteratorMruBifViewRecord {
    uint32_t version;
    int family;
    double param_min;
    double param_max;
    double x_min;
    double x_max;
    double marker_param;
} IteratorMruBifViewRecord;

typedef struct IteratorMruCyclePeriodRecord {
    uint32_t version;
    int family;
    int period;
} IteratorMruCyclePeriodRecord;

typedef struct IteratorMruStableCycleRecord {
    uint32_t version;
    int family;
    int period;
    int point_count;
    double param;
    double multiplier;
    double orbit[ITERATOR_MRU_CYCLE_POINT_MAX];
} IteratorMruStableCycleRecord;

void IteratorMruInit(HINSTANCE inst);

int IteratorMruLoadSession(IteratorMruSessionRecord *record);
int IteratorMruSaveSession(const IteratorMruSessionRecord *record);

int IteratorMruAddBifView(const IteratorMruBifViewRecord *record);
int IteratorMruLoadBifViews(IteratorMruBifViewRecord *records, int max_records, int *count_out);

int IteratorMruAddCyclePeriod(const IteratorMruCyclePeriodRecord *record);
int IteratorMruLoadCyclePeriods(IteratorMruCyclePeriodRecord *records, int max_records, int *count_out);

int IteratorMruAddStableCycle(const IteratorMruStableCycleRecord *record);
int IteratorMruLoadStableCycles(IteratorMruStableCycleRecord *records, int max_records, int *count_out);

#endif
