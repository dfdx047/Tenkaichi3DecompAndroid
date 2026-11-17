#ifndef NET_BT3_H
#define NET_BT3_H
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct Bt3Profile {
    int state;       /* 0 idle, 1 loading/saving, 2 loaded, -1 connection/server/version error */
    int board_state; /* 0 idle, 1 loading, 2 loaded, -1 failed */
    int board_count, position, rating, wins, losses, draws;
    char username[33], flag[3], rank[32];
    char board_flag[10][3], board_name[10][33], board_rank[10][32];
    int board_rating[10], board_position[10], board_wins[10], board_losses[10];
    int error;       /* 1 no connection, 2 server error, 3 incompatible protocol */
} Bt3Profile;

void Bt3Profile_Open(void);
void Bt3Profile_Save(const char *name, const char *flag);
void Bt3Profile_Board(void);
void Bt3Profile_Poll(Bt3Profile *out);

#ifdef __cplusplus
}
#endif
#endif
