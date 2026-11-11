/* Optional detail in the existing 512-byte TTW_HASH packet. Legacy clients remain valid. */
#ifndef TAG_TEAM_SYNC_H
#define TAG_TEAM_SYNC_H
#define TT_SYNC_MAGIC 0x32535442u
#define TT_SYNC_WORDS 10
#define TT_SYNC_OFFSET 48
#define TT_SYNC_POSITION_TOLERANCE 2 /* 2/16 world units, positions only */
#define TT_SYNC_FAILURE_LIMIT 6     /* six complete consecutive samples; never a single transient */
#define TT_SYNC_NOTICE 103
#define TT_CAPS_COMMAND 22
#define TT_CAPS_NOTICE 104
#define TT_CAP_PACED_INPUT 1u
#define TT_CAP_DETAILED_SYNC 2u
#define TT_CAP_DIRECT_INPUT 4u
#define TT_TAG_SPEED_PERCENT 114
#define TT_TAG_STEPS_PER_SECOND 34.2 /* 30 simulation steps, two vblanks each, at 114% */
enum { TT_SYNC_EQUAL,TT_SYNC_ROUNDING,TT_SYNC_DIFFERENT };
#endif
