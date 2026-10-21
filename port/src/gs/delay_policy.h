/* Auto input-delay policy, from DBT-NX v1.4 (MIT). */
#ifndef DBT_DELAY_POLICY_H
#define DBT_DELAY_POLICY_H
#include <stdint.h>
enum { DBT_DELAY_AUTO_CAP = 4, DBT_DELAY_MIN = 1, DBT_DELAY_MAX = 12 };
typedef struct { int samples[16]; unsigned count; int peer_auto; } DbtDelayPolicy;
static inline void DbtDelay_Note(DbtDelayPolicy *p, int ms) {
    if (ms >= 0 && ms < 5000 && p->count < 16) p->samples[p->count++] = ms;
}
static inline int DbtDelay_UpperRtt(const DbtDelayPolicy *p) {
    if (p->count < 3) return -1;
    int v[16]; unsigned n = p->count;
    for (unsigned i = 0; i < n; i++) {
        unsigned j = i;
        while (j && v[j - 1] > p->samples[i]) { v[j] = v[j - 1]; j--; }
        v[j] = p->samples[i];
    }
    return v[(n * 9 + 9) / 10 - 1];
}
static inline int DbtDelay_Choose(const DbtDelayPolicy *p, int speed_milli) {
    int ms = DbtDelay_UpperRtt(p);
    if (!p->peer_auto || ms < 0 || speed_milli < 500 || speed_milli > 2000) return -1;
    uint64_t blank_ns = 16683333ull * 1000 / (unsigned)speed_milli;
    uint64_t one_way_ns = (uint64_t)ms * 500000ull;
    int d = (int)((one_way_ns + blank_ns - 1) / blank_ns) + 1;
    return d < DBT_DELAY_MIN ? DBT_DELAY_MIN : d > DBT_DELAY_MAX ? DBT_DELAY_MAX : d;
}
#endif
