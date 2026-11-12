# Online admission and discovery recovery

Local candidate, not published. The additive endpoint migration is installed on both Supabase projects. The signed authority still selects one active admission backend; this change does not admit casual rooms on both routes at once.

## Client changes

- HTTP cancellation, room leave, lease renewal and native room reconciliation can proceed while the private Realtime inbox reconnects.
- A successful leave no longer clears a subscription requested by a subsequent create in the same queue. Subscription ownership remains with the consumers.
- Native UDP announcements are retained in the room descriptor. During discovery, the adapter reconciles at most once every two seconds, stops those reads after finding its remote endpoints, and validates membership and generation before accepting addresses.
- A delayed notification from the room just left cannot establish a new create/join's identity before its admission reply. Queued-match notifications remain accepted after the queue acknowledgement.
- Tag Team waits for its private subscription before admission can notify the host to send transient SDP/ICE. Early signals are buffered until metadata establishes the peers.
- Errors distinguish authentication, old versions, retired routes, invalid settings and unreachable signaling. Portuguese Ranked strings use the `tbo/` catalog namespace recognized by the renderer.

## Database change

`supabase/migrations/202611080001_discovery_endpoint_recovery.sql` adds a bounded nullable UDP-address array to private room membership. It retains an announcement only after the existing sender, payload, recipient and generation validation succeeds. An authenticated room member can recover the addresses through the existing descriptor/get path. Outsiders receive no endpoints, and direct table/helper permissions remain revoked. Member deletion removes its retained endpoints.

The migration is compatible with the existing descriptor shape: older clients ignore the additive `udp` field. They still require a client update to receive the worker and adapter corrections.

## Verification scope

- Actual native WinHTTP/Auth/Realtime worker: private create/join/signaling for duel, lobby, tournament and Tag Team, plus isolated quick-match pairing.
- Actual native discovery adapter with the production signed policy: successive create/join cycles, including both clients discarding UDP Realtime broadcasts and recovering endpoints by authenticated HTTP reads.
- PostgreSQL runtime: repeatable migration, late-subscriber recovery, helper/table restrictions, malformed payload rejection, outsider rejection and room-deletion cleanup.
- Build and local Dev installation are verified separately by a hash manifest.

These checks do not establish that a complete fight works across every pair of home networks. They do not supply a TURN relay or bypass restrictive NAT/firewall configurations.
