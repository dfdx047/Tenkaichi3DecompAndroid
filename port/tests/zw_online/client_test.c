/* Z Warriors Online, the game's side (port/src/plat_zwnet.c) with its server, session and library played by this
   test. Run: sh run.sh */
#include <stdio.h>
#include <string.h>
#include "../../src/plat_zwnet.c"

static char sSent[40][64]; static int sSentN;
static const char *sNext; static int sNextGot;
static char sMine[64] = "200.1.2.3:7777";
static int sSessions, sCan = 1, sRole, sPort, sPunchPort = -1; static char sAddr[64], sPunch[64];
int ZwHttp_Start(const char *what, const char *json) { (void)json; snprintf(sSent[sSentN++ % 40], 64, "%s", what); return 1; }
int ZwHttp_Take(char *out, int size) { int g = sNextGot; if (g) { snprintf(out, size, "%s", sNext ? sNext : ""); sNextGot = 0; } return g; }
const char *ZwHttp_Public(void) { return sMine; }
const char *ZwHttp_Local(void) { return "192.168.1.5:7777"; }
int Port_WarriorCode(int i, char *out, int size) { (void)i; snprintf(out, size, "ZW1:me"); return 1; }
int Port_WarriorGuest(const char *c) { (void)c; return 5; }
int Port_WarriorDropGuests(void) { return 0; }
int Port_WarriorFight(int i, int *a, int *b) { (void)i; *a = *b = 1; return 1; }
void Port_ModRescan(void) {}
void ZwFight_Put(const char *a, const char *b, int s, int t) { (void)a; (void)b; (void)s; (void)t; }
int ZwFight_Take(char *a, char *b, int n, int *s, int *t) { (void)a; (void)b; (void)n; (void)s; (void)t; return 0; }
void Port_SessionRequest(int role, const char *address, int port) { sSessions++; sRole = role; sPort = port; snprintf(sAddr, 64, "%s", address); }
int Port_SessionCan(void) { return sCan; }
void Port_NetZwPunch(int port, const char *punch) { sPunchPort = port; snprintf(sPunch, 64, "%s", punch ? punch : ""); }
void PortZw_ClipCopy(const char *t) { (void)t; }

static int bad;
static void check(int ok, const char *what) { printf("  %s  %s\n", ok ? "ok " : "BUG", what); bad += !ok; }
static void say(const char *answer) { sNext = answer; sNextGot = 1; Port_ZwNetTick(); }
static void frames(int n) { while (n--) Port_ZwNetTick(); }

static const char *peer(const char *lan, const char *pub, const char *seen, int round) {
    static char b[600];
    snprintf(b, sizeof(b), "{\"peer\":{\"name\":\"P\",\"addrs\":[\"%s\"%s%s%s],\"seen\":\"%s\",\"ready\":false,\"rev\":1,\"warrior\":\"ZW1:them\"},\"rules\":[3,1,0,0,0,0],\"round\":%d}",
             lan, pub[0] ? ",\"" : "", pub, pub[0] ? "\"" : "", seen, round);
    return b;
}
/* a guest in a room, its first poll in flight */
static void guest(void) {
    Port_ZwNetLeave(); frames(1); say("{}"); /* (the last room is told) */
    sSentN = sSessions = 0; sAddr[0] = 0; sCan = 1;
    Port_ZwNetJoin(0, "ABCDEF");
    frames(1);
    say("{\"key\":\"k\",\"you\":\"1.1.1.1\",\"rules\":[3,1,0,0,0,0],\"peer\":{\"name\":\"H\",\"addrs\":[\"192.168.0.9:7777\"],\"ready\":false,\"rev\":1,\"warrior\":\"ZW1:them\"}}");
    frames(ZN_POLL + 2);
}
/* the fight a guest asks for, given the two phones' addresses: its own LAN is 192.168.1.x */
static void address(const char *mine, const char *lan, const char *pub, const char *seen) {
    snprintf(sMine, sizeof(sMine), "%s", mine);
    guest();
    say(peer(lan, pub, seen, 1));
}

int main(void) {
    ZwNetInfo i;

    puts("addresses");
    address("200.1.2.3:7777", "192.168.1.20:7777", "200.9.9.9:40000", "200.9.9.9");
    check(sSessions == 1 && !strcmp(sAddr, "200.9.9.9") && sPort == 40000, "different networks: the address STUN saw");
    address("200.1.2.3:7777", "192.168.1.20:7777", "200.1.2.3:7777", "200.1.2.3");
    check(sSessions == 1 && !strcmp(sAddr, "192.168.1.20"), "one home network: the local address");
    address("200.1.2.3:7777", "10.77.5.20:7777", "200.1.2.3:41000", "200.1.2.3");
    check(sSessions == 1 && !strcmp(sAddr, "200.1.2.3") && sPort == 41000, "one carrier address (CGNAT), other networks: not the local address");
    address("200.1.2.3:7777", "192.168.50.4:7777", "", "200.9.9.9");
    check(sSessions == 1 && !strcmp(sAddr, "200.9.9.9"), "their STUN failed: the address the server saw, not their local one");
    address("", "192.168.50.4:7777", "200.9.9.9:40000", "200.9.9.9");
    check(sSessions == 1 && !strcmp(sAddr, "200.9.9.9") && sPort == 40000, "my STUN failed: still their public address");

    address("200.1.2.3:7777", "192.168.1.20:7777", "200.9.9.9:40000", "200.9.9.9");
    check(sSessions == 1 && !strcmp(sAddr, "200.9.9.9"), "the same local numbers in two different homes: the public address");
    address("200.1.2.3:7777", "0.0.0.0:7777", "", "2804:14c:1:2::5");
    check(sSessions == 0, "nothing reachable known (IPv6 only, no STUN): no session at a wrong address");

    puts("a server that stumbles");
    snprintf(sMine, sizeof(sMine), "200.1.2.3:7777");
    guest(); say("{\"error\":\"unavailable\"}"); Port_ZwNetGet(&i);
    check(i.state == ZN_ROOM, "one poll answered 503 \"unavailable\": the room stays");
    guest(); say("{\"error\":\"limit\",\"http\":429}"); Port_ZwNetGet(&i);
    check(i.state == ZN_ROOM, "one poll answered 429: the room stays");
    guest(); say("{\"error\":\"no room\"}"); Port_ZwNetGet(&i);
    check(i.state == ZN_FAILED && i.error == ZNE_NOROOM, "the room is gone: said so");
    guest();
    { int n; for (n = 0; n < 40; n++) { say("{\"error\":\"unavailable\"}"); frames(ZN_POLL + 2); } }
    Port_ZwNetGet(&i);
    check(i.state == ZN_FAILED, "the server stays down: gives up in the end");

    puts("the start");
    guest(); sCan = 0; say(peer("192.168.1.20:7777", "200.9.9.9:40000", "200.9.9.9", 1)); sCan = 1;
    frames(ZN_POLL + 2); say(peer("192.168.1.20:7777", "200.9.9.9:40000", "200.9.9.9", 1));
    check(sSessions == 1, "the game could not start that frame: the guest still starts at the next poll");
    frames(ZN_POLL + 2); say(peer("192.168.1.20:7777", "200.9.9.9:40000", "200.9.9.9", 1));
    check(sSessions == 1, "and starts that round only once");
    puts("cancelled before the answer came");
    Port_ZwNetLeave(); frames(1); say("{}");
    Port_ZwNetQuick(0, 1); frames(1); sSentN = 0; Port_ZwNetLeave(); frames(1);
    say("{\"ticket\":\"t1\",\"key\":\"k1\",\"state\":\"waiting\"}"); frames(2);
    check(sSentN > 0 && !strcmp(sSent[0], "quickleave"), "a search: its ticket is given back (nobody meets a ghost)");
    say("{}"); Port_ZwNetHost(0, (const int[6]){3, 0, 0, 0, 0, 0}); frames(1); sSentN = 0; Port_ZwNetLeave(); frames(1);
    say("{\"code\":\"QWERTY\",\"key\":\"k2\"}"); frames(2);
    check(sSentN > 0 && !strcmp(sSent[0], "leave"), "a room being made: closed when it comes");
    printf("%d bug(s)\n", bad);
    return bad != 0;
}
