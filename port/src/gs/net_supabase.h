#ifndef NET_SUPABASE_H
#define NET_SUPABASE_H
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DrProfile {
    int state; /* 0 idle, 1 loading/saving, 2 loaded, -1 failed */
    int board_state; /* 0 idle, 1 loading, 2 loaded, -1 failed */
    int board_count;
    int position; /* 0 means not ranked */
    char username[17];
    char country[3];
    char error[128];
    char board_country[10][3];
    char board_name[10][17];
    int board_elo[10];
} DrProfile;

void DrProfile_Open(void);
void DrProfile_Save(const char *username, const char *country);
void DrProfile_Board(void);
void DrProfile_Poll(DrProfile *out);
void DrProfile_Country(char *out, size_t size);

#ifdef __cplusplus
}
#endif

#endif
