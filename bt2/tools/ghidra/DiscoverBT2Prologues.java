// Discover functions the automatic analysis missed (reached only through pointers), in one pass:
// an aligned .text address that directly follows a completed function end (JR RA or a tail J with its
// delay slot, then up to three alignment NOPs), holds code rather than padding, and lies outside the
// address extent (lowest to highest address) of every existing function of plausible size. Nothing can fall into such
// code, and code inside a function's extent (jump-table case blocks) is left to that function. Names
// stay neutral; every candidate remains subject to prepare_map.py's structural proofs.
// @category BT2
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionManager;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;

public class DiscoverBT2Prologues extends GhidraScript {
    @Override public void run() throws Exception {
        Memory memory = currentProgram.getMemory();
        MemoryBlock text = memory.getBlock(".text");
        if (text == null || !text.isInitialized() || !text.isExecute())
            throw new IllegalStateException("Expected an initialized executable .text block.");
        FunctionManager functions = currentProgram.getFunctionManager();
        AddressSet extents = new AddressSet();
        for (Function function : functions.getFunctions(true))
            addExtent(extents, function);
        int created = 0, refused = 0;
        Address end = text.getEnd().subtract(7);
        for (Address at = text.getStart().add(16); at.compareTo(end) < 0; at = at.add(4)) {
            if (memory.getInt(at) == 0 || extents.contains(at)) continue; // Padding, or inside a function's extent.
            // The previous function's last transfer and delay slot, before optional NOP padding.
            Address before = at;
            for (int pad = 0; pad < 3 && memory.getInt(before.subtract(4)) == 0 && memory.getInt(before.subtract(8)) != 0x03E00008; ++pad)
                before = before.subtract(4);
            int transfer = memory.getInt(before.subtract(8));
            if (transfer != 0x03E00008 && (transfer >>> 26) != 2) continue;
            if (!disassemble(at)) { ++refused; continue; }
            Function function = createFunction(at, String.format("FUN_%08X", at.getOffset()));
            if (function == null) { ++refused; continue; }
            addExtent(extents, function);
            ++created;
        }
        println("DiscoverBT2Prologues: created " + created + " functions, " + refused + " refused.");
    }

    // A function's extent. A body spread over more than the largest function the map accepts
    // (prepare_map.py, 0x5000 bytes) is a wrong merge of distant code, not one function with case
    // blocks; only its own ranges count then, so it does not hide the code between them.
    private static void addExtent(AddressSet extents, Function function) {
        Address low = function.getBody().getMinAddress(), high = function.getBody().getMaxAddress();
        if (high.subtract(low) < 0x5000) extents.addRange(low, high);
        else extents.add(function.getBody());
    }
}
