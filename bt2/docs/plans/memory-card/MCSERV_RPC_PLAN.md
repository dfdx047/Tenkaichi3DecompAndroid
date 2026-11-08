# Bounded MCSERV startup exchange

## Evidence and scope

After the completed DBCMAN group, generated EE creates local semaphores 8/9 (options 0x3824C8/0x3824D8). It holds the first lock while binding MCSERV 0x80000400, client 0x3A6F80 / RPC ID 39. The owned resident server/worker is already validated; do not synthesize a separate implementation or card filesystem.

Independent local captures at 0x31D3CC (before call) and 0x31D3D4 (after call) establish command 0xFE: 48 zero input bytes at 0x3A7000, 12-byte reply at 0x3A8540 containing [0,0x210,0x226]. Completed bind/client ID is 39; completed call/client ID is 40, temporary semaphore 10. Public pinned PS2SDK libmc.c identifies service 0x80000400 and command 0xFE as the initialization/version exchange. Actual BT2 result remains authoritative.

## Implementation and validation

1. Deliver the bind using existing owned server/queue/worker identity and generated RPC_END/wait/delete.
2. Invoke the real registered MCSERV handler with its retained worker GP/stack for exactly this input and command. Copy its actual 12-byte response; reject wrong epoch/input/repetition without mutation.
3. Validate response and client ownership against independent captures, then continue generation/link/execution until the actual next blocker.
4. Keep card access, files, device detection, callbacks and timing outside this initial exchange. Establish separate evidence when reached; no blanket successful memory-card service.

Reference: [pinned PS2SDK libmc.c](https://github.com/ps2dev/ps2sdk/blob/ac92a9f657d2e531dd8f060250b07f2a5ac6dea5/ee/rpc/memorycard/src/libmc.c). Reviewed the local pinned BT3 IOP architecture for owned real-handler delivery; no implementation copied.

## Verified result and new scope

Bind, real handler response, generated RPC_END/wait/delete and both local lock pairs complete. Service tests verify [0,0x210,0x226], invalid-input/epoch rejection and no-mutation duplicate rejection. The independent validator compares relocated code and owned client/server identity. Native checkpoint: 1,263 EE calls / 1,255 returns; 32,616 IOP instructions / 150 physical entry targets.

The next command is asynchronous 1 / ID 41, send 48 bytes from 0x3A7000, receive 4 bytes at 0x3A8540. Independent before-call capture confirms all input words zero except word 7 = 0x3A7480. Callback FUN_0031DD60 receives argument 0x3A7480; generated initialization zeroes a 192-byte area before submission. Current client semaphore is -1, matching asynchronous transport rather than a temporary blocking call semaphore. The callback/data path and actual card-query handler effects need separate evidence and architecture; do not infer no-card status or deliver a successful callback from the initialization result. The submitted request remains pending.

## Native query execution and SIO2 DMA handoff

The captured command-1 input now enters the actual registered MCSERV handler and compiled MCMAN/SIO2 exports. The first event wake initially failed because the RPC worker was active. The bounded priority-16 SIO2 preemption from priority-104 MCSERV now runs the waiting worker to its next yield and restores the caller; unrelated nested wakes remain rejected before event mutation. Synthetic tests verify restoration on yield and failure, unchanged caller registers and rejection of a same-priority target.

The first sceSetSliceDMA call (dmacman:28) programs channel 11 from owned source 0x32020, 36 words / one block / direction 1. Native configuration is MADR 0x32020 / BCR 0x10024 / CHCR 0x201 and return 1. An isolated interpreter hook invokes the actual retail API, reads the physical registers and validates the same 144-byte canonical command buffer (81 11 followed by zeros). The loaded MCMAN code proves normalized source ownership. The next sceStartDMA (ordinal 32 / PC 0x10E30) remains unsupported; no transport completion or RPC_END occurs. Failed query retry is rejected without further mutation.

A separate retail capture stops at callback FUN_0031DD60. It observes primary result -2 in that local profile and 192 bytes delivered to 0x3A7480; many are unrequested/uninitialized stack data. This is not a generic device result or payload template. Actual SIO2 transfer, receive-buffer ownership and asynchronous delivery require the separate SIO2_CARD_DMA_PLAN.md scope. Query completion count remains zero.
