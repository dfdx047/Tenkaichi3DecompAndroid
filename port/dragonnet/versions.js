// Which games this service lets in, by netcode version (the `v` of every request). SPDX-License-Identifier: MIT
//
// ALLOWED_VERSIONS: a comma-separated list, e.g. "40001,5001". Games of different versions never meet: rooms, quick
// queues and Ranked searches are kept apart by version (a room only takes a player of its own version). Chat,
// presence and Ranked profiles are shared by all the allowed versions.
//
// Versions in use:
//   40001  DBT-NX (Switch) before its merge with the PC port's 0.1.37
//   5001   Dragon Net: Dragon Rage (Android) and DBT-NX after the merge: the PC port's netcode 5 (0.1.37), and the
//          simulation version 1 (DRNET_SIM in port/src/gs/net_dragon.h). It changes only when something that
//          decides a fight changes, not with every release of the PC port.
//   6001   Dragon Rage after the PC port's 0.1.39 (netcode 6: lobbies in place of rooms of two, a fingerprint always
//          sent), simulation 1. A Worker for it lists 6001 too ("40001,5001,6001"); 5001 and 6001 never meet.
// ALLOWED_VERSION (one number) is still read when ALLOWED_VERSIONS is not set, as the DBT-NX Worker had it.
export function allowedVersions(env) {
    const raw = String(env.ALLOWED_VERSIONS ?? env.ALLOWED_VERSION ?? "40001");
    return new Set(raw.split(",").map(s => Number(s.trim())).filter(n => Number.isInteger(n) && n > 0));
}
// Rooms and queues are checked only when one of the two variables is set (a Worker without them takes any version,
// as the PC port's does); Ranked, chat and presence always check (40001 when nothing is set).
export function versionGateOn(env) {
    return env.ALLOWED_VERSIONS !== undefined || env.ALLOWED_VERSION !== undefined;
}
export function versionAllowed(env, v) {
    return Number.isInteger(v) && allowedVersions(env).has(v);
}
