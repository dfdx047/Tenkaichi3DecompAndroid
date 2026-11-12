# Memory: heap, battle pool, containers

Sources: `src/sys/heap.c`, `heap_info.c`, `src/battle/btl_pool.c`, `src/sys/list.c`, `queue.c`,
`align.c`. Layouts: `include/sys/heap.h`, `include/battle/btl_pool.h`, `include/sys/list.h`,
`include/sys/queue.h`.

## Main heap (verified)

- One heap, from 0x3BE71C (end of the overlay) to 0x1EFB000. The code supports two heaps and a
  "search any" index (2), but boot only creates the first.
- Block header, 0x20 bytes: magic `'TBHS'`, `used`, `fromTail`, `align`, `size` (whole block),
  `data` (pointer handed out), `dataSize`. The last word of every block repeats its size, so the
  allocator can walk backwards.
- `Heap_Alloc(size, align, fromTail, heap)`: first fit from the head or from the tail. Needs
  `size + align + 0x24`. Splits the block only when more than 0x80 bytes would remain.
- `Heap_Free(ptr)`: linear search over all blocks for the one whose `data == ptr`, then merges
  with free neighbours on both sides.
- Statistics: free size, used size, largest free block, block counts.

## Battle pool (verified)

`BtlPool` (0x9C bytes, `gBtlPool`): nine slots `{base, cur, size, used}`, the arena memory
pointer, the current slot, and a mask of which slots are bump arenas.

- `BtlPool_Init` allocates one block and carves nine arenas of 0xA000, 0x113000, 0x1E000, 0x1400,
  0x19000, 0x19000, 0x400, 0x19000 and 0x19000 bytes.
- `BtlPool_Alloc(slot, size)` returns `cur` and advances it by `size` rounded up to 32. There is
  no capacity check.
- `BtlPool_Reset(slot)` rewinds an arena. `BtlPool_Free` only does anything for non-arena slots.
- The mask is always 0x3FE (every slot is an arena), so the heap fall-through is dead code
  (inferred from the constant: nothing else in the file writes the mask).

## Containers (verified)

- `List` / `ListNode`: intrusive doubly linked list `{head, tail, count}`. Mostly uncalled.
- `SList` / `SListNode`: intrusive singly linked list. This is the one the game uses.
- `Queue`: `{capacity, nodes, freeList, head, used}` with a node pool from the heap. `used`
  counts pool nodes handed out, not queued items; a queue never draws more than `capacity - 1`
  nodes.
- `Mem_Align{4,8,16,32,64,128}{Rem,Pad,Up}`: alignment helpers.

About 25 of these utility functions have no callers in either binary: the developers linked a
general-purpose library.
