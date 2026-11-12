#ifndef BT4_ONLINE_ROOM_POLICY_H
#define BT4_ONLINE_ROOM_POLICY_H
#include <stdint.h>
/* Established rooms and tournaments: grace since the last authenticated
 * communication. Initial connection attempts and active duel rollback retain
 * their own deadlines; they must not wait half an hour to report a failure. */
#define BT4_ROOM_IDLE_MS UINT64_C(1800000)
static inline int Bt4Room_IsIdleMs(uint64_t now,uint64_t heard) {
    return now>=heard&&now-heard>=BT4_ROOM_IDLE_MS;
}
static inline int Bt4Room_IsIdleNs(uint64_t now,uint64_t heard) {
    return now>=heard&&now-heard>=BT4_ROOM_IDLE_MS*UINT64_C(1000000);
}
#endif
