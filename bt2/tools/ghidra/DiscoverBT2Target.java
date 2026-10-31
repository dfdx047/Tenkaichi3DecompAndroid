// Discover only an execution-observed missing entry; never assign a semantic name.
// @category BT2
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import java.util.ArrayList;
import java.util.List;

public class DiscoverBT2Target extends GhidraScript {
    // Direct J (not JAL) from the observed body to another address that is a JAL target or
    // opens its own stack frame (ADDIU sp,sp,-N within its first two instructions): a tail call
    // that Ghidra otherwise absorbs as intra-function flow. A target directly after a completed
    // JR RA/delay-slot pair has no fall-through entry and is also separated (leaf tail calls).
    // Other targets stay as discovered.
    // JR RA or a tail J: with its delay slot, the end of the code before a separate entry.
    private static boolean endsFunction(int word) { return word == 0x03E00008 || (word >>> 26) == 2; }

    private List<Address> tailCallTargets(Function function, Address entry) throws Exception {
        List<Address> targets = new ArrayList<>();
        for (Instruction ins : currentProgram.getListing().getInstructions(function.getBody(), true)) {
            int word = currentProgram.getMemory().getInt(ins.getAddress());
            if ((word >>> 26) != 2) continue;
            long target = ((ins.getAddress().getOffset() + 4) & 0xF0000000L) | ((word & 0x03FFFFFFL) << 2);
            if (target == entry.getOffset()) continue;
            Address address = toAddr(target);
            boolean frame = false;
            for (int n = 0; n < 2; ++n) {
                int ins2 = currentProgram.getMemory().getInt(address.add(4L * n));
                frame |= (ins2 & 0xFFFF0000) == 0x27BD0000 && (short) ins2 < 0;
            }
            // A previous function's JR RA (or tail J) / delay slot ends right before the target, possibly followed by
            // up to three alignment NOPs.
            // (The delay slot itself may be a NOP, so the adjacent case is checked first.)
            frame |= endsFunction(currentProgram.getMemory().getInt(address.subtract(8)));
            Address before = address;
            for (int pad = 0; pad < 3 && currentProgram.getMemory().getInt(before.subtract(4)) == 0; ++pad) {
                before = before.subtract(4);
                frame |= endsFunction(currentProgram.getMemory().getInt(before.subtract(8)));
            }
            boolean called = false;
            for (Reference ref : getReferencesTo(address)) called |= ref.getReferenceType().isCall();
            if ((frame || called) && !targets.contains(address)) targets.add(address);
        }
        return targets;
    }

    @Override public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) throw new IllegalArgumentException("Supply one observed target address.");
        Address start = toAddr(Long.decode(args[0]));
        MemoryBlock block = currentProgram.getMemory().getBlock(start);
        if ((start.getOffset() & 3) != 0 || block == null || !block.isInitialized()
            || !block.isExecute() || !block.getName().equals(".text"))
            throw new IllegalArgumentException("Target must be aligned initialized executable .text.");
        Function containing = currentProgram.getFunctionManager().getFunctionContaining(start);
        if (containing != null && !containing.getEntryPoint().equals(start))
            throw new IllegalStateException("Target lies within another function; review overlap.");
        if (containing == null) {
            if (!disassemble(start)) throw new IllegalStateException("Target disassembly failed.");
            containing = createFunction(start, String.format("FUN_%08X", start.getOffset()));
            if (containing == null) throw new IllegalStateException("Function discovery failed.");
        }
        List<Address> tails = tailCallTargets(containing, start);
        if (!tails.isEmpty()) {
            removeFunction(containing);
            for (Address tail : tails) {
                if (currentProgram.getFunctionManager().getFunctionAt(tail) == null
                    && createFunction(tail, String.format("FUN_%08X", tail.getOffset())) == null)
                    throw new IllegalStateException("Tail-call target discovery failed.");
                println("Separated direct tail-call target provisionally: " + tail);
            }
            containing = createFunction(start, String.format("FUN_%08X", start.getOffset()));
            if (containing == null) throw new IllegalStateException("Function rediscovery failed.");
        }
        println("Observed target discovered provisionally: " + containing.getEntryPoint()
            + " body bytes=" + containing.getBody().getNumAddresses());
    }
}
