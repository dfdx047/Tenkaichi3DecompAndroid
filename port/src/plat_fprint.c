/* BT4's online data policy is shared by 1v1, lobbies, spectators and Tag Team.
 * Identify the supported base programs and simulation ABI, rather than comparing
 * BT4 archives with BT3's USA file list. Spanish/English and R labels do not
 * change this family. Unsupported/damaged base programs still fail validation.
 * Extracted presentation assets are deliberately not part of this identity;
 * this is the same compatibility policy used by Port_TagDataFingerprint.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bt4_online_compat.h"
extern int Port_BuildIdentity(unsigned long long *out);
extern const char *Port_BuildIdentitySummary(void);

static int test_claim(unsigned long long *out) {
    const char *t = getenv("BT3_TEST_FPRINT");
    if (!t || !*t || !strcmp(t,"off")) return 0;
    *out = strtoull(t,NULL,16); return 1;
}
int Port_DataFingerprint(unsigned long long *out) {
    unsigned long long value;
    int result = Port_BuildIdentity(&value);
    if (result == 1 && out) { *out=value; test_claim(out); }
    return result;
}
int Port_DataFingerprintNeed(void) { return Port_DataFingerprint(NULL); }
int Port_DataFingerprintOriginal(void) {
    unsigned long long value;
    return Port_DataFingerprint(&value)==1 && value==BT4_ONLINE_COMPAT_ID;
}
int Port_DataFingerprintPart(int part,unsigned long long *out) {
    /* There is one verified family identity, not seven BT3 archive components. */
    (void)part; (void)out; return -1;
}
const char *Port_DataFingerprintText(void) {
    return Port_BuildIdentity(NULL)==1 ? "BT4 B14 Rev2: compatible base programs and simulation ABI"
                                       : Port_BuildIdentitySummary();
}
void PortFp_Pending(int on) { (void)on; }
void PortFp_Run(int detail) { (void)detail; Port_BuildIdentity(NULL); }
