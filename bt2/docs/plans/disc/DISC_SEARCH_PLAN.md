# Bounded disc-search compatibility

1. Validate the reached 0x80000597 boot service, handler/queue ownership and disc-matched CDVDFSV image against local captures. Binding uses the existing reference-backed boot-service pattern; it does not claim execution of boot entry or residency.
2. Capture the actual first EE request and result. Preserve request sizes, command, input path, output ownership and real callback cleanup. Undefined stack bytes are not result templates.
3. Generate the supplied CDVDFSV code into ignored work/, at a disjoint authored placement. Execute its actual search handler with retained GP and owned RAM/stack. No boot-entry execution is claimed.
4. Supply only reached CDVDMAN metadata lookup behavior through a bounded read-only local ISO9660 backend. Validate directories/extents, missing files and malformed inputs. Compare metadata/result and actual transfer against independent retail evidence before acknowledging completion.
5. Run generated EE completion/cleanup and continue related lookup paths. Actual sector requests need their command/size/destination/completion evidence; no implicit successful sectors, mounted cards, audio or presentation.

Use the pinned BT3 owned module/RPC architecture and public SDK contracts, adapted to BT2 evidence. Keep proprietary content and generated output ignored. Follow the permanent multi-blocker pass rule.
