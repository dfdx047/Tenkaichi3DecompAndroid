#ifndef CRI_ADXT_H
#define CRI_ADXT_H

/* CRI ADX "talk" (streamed audio) handle. Only the fields used so far are known. */
typedef struct ADXT {
    /* 0x00 */ signed char used;
    /* 0x01 */ signed char stat;
    /* 0x02 */ short unk2;
    /* 0x04 */ void *sjd; /* ADXSJD decoder handle */
} ADXT;

#endif
