/* Exercise the production updater against a local HTTP fixture. */
#include <assert.h>
#include "../../src/gs/net_update.c"

static void wait_for(int want) {
    Uint64 start = SDL_GetTicks();
    while (SDL_GetAtomicInt(&sU.state) == UPD_CHECKING && SDL_GetTicks() - start < 10000) SDL_Delay(5);
    assert(SDL_GetAtomicInt(&sU.state) == want);
}

int main(void) {
    UpdateStatus u;
    assert(SDL_Init(0));
    Update_Check();
    Update_Recheck(); /* The first request is still running: do not duplicate it. */
    wait_for(UPD_FAILED);
    Update_Status(&u);
    assert(u.why[0] && !u.canInstall && !u.latest[0]);
    Update_Check(); /* The startup check remains one-shot. */
    assert(SDL_GetAtomicInt(&sU.state) == UPD_FAILED);
    Update_Recheck(); /* A manual check can recover from an HTTP error. */
    wait_for(UPD_NONE);
    Update_Status(&u);
    assert(!u.why[0]);
    Update_Recheck(); /* Also recheck after an up-to-date result. */
    wait_for(UPD_NONE);
    for (int state = UPD_CHECKING; state <= UPD_INSTALLED; ++state) {
        if (state == UPD_NONE) continue;
        SDL_SetAtomicInt(&sU.state, state);
        Update_Recheck();
        assert(SDL_GetAtomicInt(&sU.state) == state);
    }
    SDL_SetAtomicInt(&sU.state, UPD_FAILED);
    SDL_SetAtomicInt(&sU.busy, 1);
    Update_Recheck();
    assert(SDL_GetAtomicInt(&sU.state) == UPD_FAILED);
    puts("Updater manual retry, error recovery, request coalescing and active-operation guards PASS");
    SDL_Quit();
    return 0;
}
