/* port/src/gs/tag_net.c's host-to-guest input datagram (TN_FAST) on its worst case, without the game: the real
   arb_send_fast encodes ten players' last frames, the real arb_recv_fast decodes them on the guest's side, and every
   frame must come through, in datagrams the connection can carry (TagLink_SendDatagram takes up to TL_PIECE bytes).
   Build and run with AddressSanitizer:  sh port/tools/tagnet_test/run.sh
   Simulated: the game (pads, rollback, menus, the room's screens) and the connection (TagLink_SendDatagram keeps
   the datagrams); tag_net.c itself is the game's own file, #included below. */
#include <SDL3/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include "tag_link.h"

/* ---- stand-ins for the game ---- */
void Gs_HoldFrame(void) {}
int PortGame_CharacterCount(void) { return 160; }
int PortGame_Costumes(int chara) { (void)chara; return 4; }
int PortGame_IsBT4(void) { return 0; }
int PortGame_IsCharacter(int chara) { return chara >= 0 && chara < 160; }
int PortGame_StageCount(void) { return 30; }
int Port_GameSpeed(void) { return 100; }
const char *Port_GetEnv(const char *name) { return getenv(name); }
unsigned long long Port_NetDigest(void) { return 0; }
const char *Port_NetNameGet(void) { return "Test"; }
int Port_PadRead(int socket, unsigned char *data) { (void)socket; (void)data; return 0; }
void Port_RollBack(int k) { (void)k; }
int Port_RollCan(void) { return 1; }
void Port_RollForget(void) {}
int Port_RollSave(void) { return 0; }
int Port_TagOption(int option) { (void)option; return 0; }
const char *Port_TagText(const char *english) { return english; }
void Port_UiBanner(const char *text, int ms) { (void)text; (void)ms; }
void Port_UiNotice(const char *text) { (void)text; }
int TagChallenge_Capacity(void) { return 10; }
void TagChallenge_Choose(int index) { (void)index; }
int TagChallenge_FirstCount(void) { return 0; }
int TagChallenge_FirstFighter(int member, int costume) { (void)member; (void)costume; return 0; }
int TagLobby_IsSelecting(int member) { (void)member; return 0; }
unsigned long long TagNet_DataIdentity(void) { return 1; }
void TagOnlineMenu_Close(void) {}
int TagOnlineMenu_Frame(void) { return 0; }
void TagOnlineMenu_Open(void) {}
unsigned long long TagOnline_SaveFingerprint(void) { return 0; }
void TagOnline_SaveScope(int on) { (void)on; }
void TagOnline_Seed(void) {}
uint64_t gPortNetWaitNs;
unsigned gPortPaceShiftNs;
int gPortResim;
unsigned long long gPortSleptTotalNs;

/* ---- the connection: datagrams kept for the guest's side ---- */
static unsigned char sent[16][2048];
static int sentLen[16], sentN, sentTooLong;
int TagLink_SendDatagram(int slot, const void *data, int len) {
    (void)slot;
    if (len > 1100) { sentTooLong++; return -1; } /* (tag_link.c: TL_PIECE; a longer one is never sent) */
    if (sentN < 16) { memcpy(sent[sentN], data, (size_t)len); sentLen[sentN++] = len; }
    return len;
}
int TagLink_Accept(void) { return -1; }
int TagLink_Active(void) { return 0; }
void TagLink_Close(int slot) { (void)slot; }
void TagLink_FlushAcks(void) {}
int TagLink_Host(const char *name, int version) { (void)name; (void)version; return 0; }
int TagLink_Join(const char *code, const char *name, int version, int watch) { (void)code; (void)name; (void)version; (void)watch; return -1; }
void TagLink_Pump(void) {}
int TagLink_Recv(int slot, void *out, int max) { (void)slot; (void)out; (void)max; return 0; }
int TagLink_RecvDatagram(int slot, void *out, int max) { (void)slot; (void)out; (void)max; return 0; }
int TagLink_RttMs(int slot) { (void)slot; return -1; }
int TagLink_Send(int slot, const void *data, int len) { (void)slot; (void)data; return len; }
int TagLink_State(int slot) { (void)slot; return 0; }
void TagLink_Status(char *out, int size, char *code, int codeSize) { (void)size; (void)codeSize; out[0] = code[0] = 0; }
void TagLink_Stop(void) {}

#include "tag_net.c"

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL " __VA_ARGS__); printf("\n"); } } while (0)

/* The pad of player p at frame t: `kind` 0 every byte changes every frame (the worst case), 1 a few bytes change
   (sticks and a button), 2 nothing changes. */
static void pad_of(int kind, int p, unsigned t, unsigned char *out) {
    int i;
    for (i = 0; i < PAD; i++) out[i] = (unsigned char)(kind == 0 ? ((t & 1) ? 0x00 : 0xFF) ^ p : kind == 2 ? 128 : (i < 3 ? (t * 7 + i + p) & 255 : 128));
}

/* The host sends player 1's guest (peer 0) the ten players' frames up to `frame`; the guest, which has every frame
   before `from`, decodes them. `acked`: the guest has acknowledged frames up to `from` (relative encoding). */
static void one_case(const char *name, int kind, int acked) {
    unsigned char want[TN_PLAYERS][8][PAD];
    unsigned base = 200, f, t, from;
    int p, d, total = 0;
    /* the host */
    memset(&config, 0, sizeof(config)); config.party = 1; config.practice = 0;
    matchMask = (1u << TN_PLAYERS) - 1; arbEnv = 1;
    host = 1; role = 0; phase = 4; begun = 1; roundId = 9; frame = base;
    memset(peers, 0, sizeof(peers)); for (p = 0; p < PEERS; p++) peers[p].fd = TN_BAD;
    peers[0].fd = (tn_socket)(TN_LINK | 0); peers[0].hello = 1; peers[0].role = 1;
    memset(inputStamp, 255, sizeof(inputStamp)); memset(fastAck, 0, sizeof(fastAck));
    from = base - ARB_FAST;
    for (p = 0; p < TN_PLAYERS; p++) {
        for (t = from - 1; t < base; t++) { unsigned char pad[PAD]; pad_of(kind, p, t, pad); memcpy(inputs[p][t % RING], pad, PAD); inputStamp[p][t % RING] = t; }
        have[p] = base;
        if (acked) fastAck[0][p] = from;
        for (t = from; t < base; t++) memcpy(want[p][t - from], inputs[p][t % RING], PAD);
    }
    sentN = sentTooLong = 0;
    arb_send_fast();
    for (d = 0; d < sentN; d++) total += sentLen[d];
    printf("%s: %d datagram(s), %d bytes (largest %d), %d too long for the connection\n", name, sentN, total,
           sentN ? sentLen[0] : 0, sentTooLong);
    CHECK(sentTooLong == 0, "%s: a datagram longer than the connection takes (dropped: every guest waits for the stream)", name);
    /* the guest: has every frame before `from`, none after */
    host = 0; role = 1;
    for (p = 0; p < TN_PLAYERS; p++) { for (t = from; t < base + 8; t++) inputStamp[p][t % RING] = ~0u; have[p] = from; }
    for (d = 0; d < sentN; d++) {
        unsigned n = get32(sent[d]);
        CHECK(n == (unsigned)sentLen[d] && get32(sent[d] + 4) == TN_FAST, "%s: datagram %d's frame", name, d);
        arb_recv_fast(sent[d] + 8, n - 8);
    }
    for (p = 0; p < TN_PLAYERS; p++) {
        CHECK(have[p] == base, "%s: player %d's frames up to %u came, not %u", name, p, have[p], base);
        for (f = from; f < have[p]; f++) CHECK(!memcmp(inputs[p][f % RING], want[p][f - from], PAD), "%s: player %d frame %u differs", name, p, f);
    }
}

int main(void) {
    one_case("worst case, first send (whole pads)", 0, 0);
    one_case("worst case, acknowledged (relative)", 0, 1);
    one_case("typical (sticks and a button)", 1, 1);
    one_case("nothing changes", 2, 1);
    printf(failures ? "%d failure(s)\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
