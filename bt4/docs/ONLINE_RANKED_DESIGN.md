# Ranked online mode

## Implementation

The R5 client adds Ranked below Quick Match in Tenkaichi Budokai Online. The menu has separate solo 1 vs 1 and native BT3 Team Battle queues. Tag Team is not one of these formats. Ratings, match records and Top snapshots are independent for each ruleset.

The client reads status and paged Top data through `public.bt4_ranked_api`, on its own response channel. Ranked queueing uses a separate `ranked-v1:` wire namespace and capacity two, then reuses normal room admission, compatibility checks and P2P simulation. A result is reported after a stable terminal state has remained beyond the rollback window and confirmed input. The client sends only a match ID, outcome and deterministic digest. The database waits for matching reports from both participants, then writes the rating update once.

Initial rating is 1000 and the Elo update uses K=32. Ratings carry between weekly seasons. Weekly records and snapshots are season-scoped. The public Top is refreshed hourly. The scheduled rollover is Monday 02:59 UTC, which is Sunday 23:59 in Buenos Aires. The rollover retains lifetime ratings and starts a new weekly record.

The secondary database (backend ID 1) is the single authority for Ranked scores on both projects. The migration pins both configurations to it. If matchmaking fails over to the primary database, Ranked returns unavailable instead of creating a second, divergent rating history. Casual rooms and other online functions continue to use the currently active route.

The release contains only six signed game update assets: Windows, Linux and macOS archives and their signatures. Database migration files remain in the source repository and are not packaged as a user-installed archive.

## Trust and limits

Combat is peer-to-peer. The server accepts a result only when both authenticated participants report the same outcome and digest. This agreement reduces accidental or one-sided reporting, but it is not an independent referee and does not prevent a modified client from lying. Do not describe this first version as anti-cheat protected. Conflicting reports are marked disputed and do not apply a rating change.

Ranks are not matched by rating range in this initial queue; the queue isolates the two formats and uses the existing public matchmaking pool. New accounts are represented at 1000 points for status, and enter the leaderboard after their first completed Ranked match.

## Deployment and verification

Migrations `202611080001` through `202611080005` establish endpoint recovery, private Ranked tables, hourly and weekly jobs, persistent ratings, two-party result reports and a fixed rating authority. Production activation requires applying migration 005 to both projects after 001 through 004, then verifying that the secondary project is the active route and its `admission_open` lease is valid. The signed R5 update packages must be public before raising the minimum supported client to R5.

The SQL/PGlite checks cover solo/team isolation, two-party result agreement, idempotent retries, conflicting reports, the 1000 starting rating, rating updates, weekly rollover, access controls and route-authority rejection. Native checks cover menu parsing, paging, stale response cancellation, queue startup and result reporting. A live smoke test must use the active route and be spaced to avoid Supabase rate limits.

Required production checks before publishing R5:

1. Apply migration 005 on both projects and confirm backend 1 is the sole Ranked authority.
2. Confirm secondary route admission is open, Ranked status is enabled there, and status is unavailable on backend 0.
3. Complete a two-client solo match and a native Team Battle match; verify one rating update per participant and no cross-format records.
4. Verify disagreement, retry, disconnect, timeout, and duplicate reports do not award points twice.
5. Confirm hourly snapshots and the Sunday 23:59 Argentina rollover jobs are active.
6. Build and sign Windows, Linux and macOS archives; publish only after their signatures verify.

No experimental aura work or separate development menu belongs in the public release.
