#include "common.h"
#include "cri/adxt.h"

void ADXERR_CallErrFunc1(const char *msg);
int ADXSJD_GetSfreq(void *sjd);

/* Returns the sampling frequency of the stream, 0 if not yet decoded, -1 on a null handle. */
int adxt_GetSfreq(ADXT *adxt) {
    if (adxt == NULL) {
        ADXERR_CallErrFunc1("E02080819 adxt_GetSfreq: parameter error");
        return -1;
    }
    if (adxt->stat >= 2) {
        return ADXSJD_GetSfreq(adxt->sjd);
    }
    return 0;
}
