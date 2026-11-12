# R4 online deployment

Status: prepared, not activated. The checked-in configuration leaves routing and
the minimum-client requirement disabled. The release must remain a draft until
gameplay and the platform packages have been checked.

This is a standby matchmaking service, not replication or a recoverable backup
of the primary database. Game inputs and established voice connections use P2P.

## Compatible database preparation

1. Inspect both projects and compare the six existing migrations in this folder
   with their installed API contract. Do not reinstall the base schema or purge
   Realtime history. Preserve active rooms and users.
2. Apply `migrations/202611060001_online_route_gate.sql` to both projects. It is
   transactional and repeatable. It retains old-client admission on the primary
   until the explicit cutover. The original dispatcher remains behind the new
   authenticated admission wrapper; clients cannot call it directly.
3. On the primary (`mhgksaiitqoijnatpurf`), check `backend_id=0`, `epoch=1`,
   `require_update=false`, `admission_open=true`, `admission_until=null`.
4. On the reserve (`houhjsfukkkicjejvett`), run this initial-only setup:

   ```sql
   update bt4_private.route_config set backend_id=1,admission_open=false
   where singleton and epoch=1 and not require_update and admission_until is null;
   ```

5. Check `public.bt4_route_health()` and authenticated create/join/leave on the
   primary. Use only new test identities and clean up their rooms. The reserve
   stays closed to public admission until the signed authority activates it.

## After R4 is public and its updater is verified

Set repository Actions secrets `SUPABASE_PRIMARY_MANAGEMENT_TOKEN` and
`SUPABASE_SECONDARY_MANAGEMENT_TOKEN` using each account's management token.
Retain `UPDATE_SIGNING_KEY`; do not rotate it or commit any secret. Each token
must reach only its intended project. The controller uses the management API
only for the private route configuration row.

Change `enabled` and `require_update` in `online-routing.json` to `true`, retaining
minimum `0.1.39.5-R4`. Dispatch **Online routing authority**, first with `apply`
unchecked, then with `apply` checked and route `auto`. Initial activation
downloads all three **public stable** archives and verifies their complete bytes
and Ed25519 signatures. A draft cannot pass this check.

The authority publishes signed policy and lease bounds on `online-routing`
before database writes. It then closes the old route before opening the new
one. If closure cannot be acknowledged, it waits for the last published old
lease plus clock tolerance to expire. Enable the repository variable
`BT4_ONLINE_ROUTING_ENABLED=true` only after this manual activation passes.
Scheduled executions are serialized. A healthy reserve is not automatically
switched back to the primary.

Policy lifetime is just under 24 hours; admission leases last 24 hours. The
five-minute Actions schedule is not a real-time guarantee. This longer signed
window keeps current clients admitting rooms during delayed scheduled runs. If
the previous database cannot be closed administratively, a route switch can
take up to 24 hours to fence it. Failed policy refresh stops new admission after
the trusted policy expires.

## Sessions, updates and limits

Refresh tokens and inboxes are separate for each project and game instance.
Only one project's Realtime inbox is open per client. Room generation and
pending operations remain pinned to their project. Legacy codes refer to the
primary; new coordinated codes start with `1` (primary) or `0` (reserve).

The minimum applies to create, join, queue and list. Existing rooms can signal,
renew, leave and finish. The gate is compatibility metadata, not binary
attestation or anti-cheat. Older binaries can be rejected by SQL, but cannot
acquire the new translated UI solely through SQL. R4 presents the required
update and offers Restart now only after signature verification, at an offline
menu. Failed downloads preserve the admission block and permit retry.

An existing P2P game/voice transport is not closed merely because the authority
changes. This implementation does not replicate live room/member state into the
reserve. Rejoining or renegotiating a pinned room while its old project is down
may fail. Do not advertise transparent recovery of that case until demonstrated
with real clients. The reserve is used for subsequent admission.

## Recovery and rollback

Disable `BT4_ONLINE_ROUTING_ENABLED` to stop scheduled changes. During an
activated incident, keep both projects fenced or use a newer signed epoch to
select a healthy route after closing the previous one. Do not reopen both
projects, lower the minimum, reset epochs, or delete active rooms to recover.

Before the first cutover only, an initial reserve can be re-prepared using the
guarded update above. Keep the additive migration installed; there is no need to
drop tables or restore the old unauthenticated execution permissions. Never
reapply the initial configuration after an activated epoch.

The release handoff contains local validation results and per-project SQL files.
Administrative application and full cross-project game/voice failure recovery
remain separate validation steps; passing isolated SQL tests is not evidence
that production has been migrated.
