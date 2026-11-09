// Export analysis candidates without inferring function purposes.
// @category BT2

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.address.AddressRange;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.mem.MemoryBlock;
import com.google.gson.GsonBuilder;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

public class ExportBT2Candidates extends GhidraScript {
    private List<Map<String, Object>> instructionsFor(Function function) throws Exception {
        List<Map<String, Object>> records = new ArrayList<>();
        InstructionIterator iterator = currentProgram.getListing().getInstructions(function.getBody(), true);
        while (iterator.hasNext()) {
            monitor.checkCancelled();
            Instruction instruction = iterator.next();
            Map<String, Object> record = new LinkedHashMap<>();
            record.put("address", instruction.getAddress().toString());
            record.put("instruction", instruction.toString());
            record.put("flow_type", instruction.getFlowType().toString());
            List<String> flows = new ArrayList<>();
            for (Address target : instruction.getFlows()) flows.add(target.toString());
            record.put("flows", flows);
            records.add(record);
        }
        return records;
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 2) throw new IllegalArgumentException("Supply the local export directory and ELF entry address.");
        Path output = Path.of(args[0]);
        Files.createDirectories(output);
        List<Map<String, Object>> functions = new ArrayList<>();
        StringBuilder csv = new StringBuilder("Name,Start,End,Size\n");
        int contiguousCount = 0;
        FunctionIterator iterator = currentProgram.getFunctionManager().getFunctions(true);
        while (iterator.hasNext()) {
            monitor.checkCancelled();
            Function function = iterator.next();
            if (function.isExternal()) continue;
            long start = function.getEntryPoint().getOffset();
            long end = function.getBody().getMaxAddress().getOffset() + 1;
            long size = function.getBody().getNumAddresses();
            String name = String.format("FUN_%08X", start);
            List<String> ranges = new ArrayList<>();
            for (AddressRange range : function.getBody().getAddressRanges()) {
                ranges.add(range.getMinAddress() + "-" + range.getMaxAddress());
            }
            boolean contiguous = function.getBody().getMinAddress().equals(function.getEntryPoint())
                && size == end - start && start % 4 == 0 && end % 4 == 0;
            // A body with holes that no other function owns is exported as its whole span: the holes are
            // code Ghidra did not reach (for example jump-table cases), still filtered by prepare_map.
            boolean spansUnownedGaps = false;
            if (!contiguous && function.getBody().getMinAddress().equals(function.getEntryPoint()) && start % 4 == 0 && end % 4 == 0) {
                AddressSet span = new AddressSet(function.getEntryPoint(), toAddr(end - 1));
                spansUnownedGaps = true;
                var overlapping = currentProgram.getFunctionManager().getFunctionsOverlapping(span);
                while (overlapping.hasNext())
                    if (!overlapping.next().equals(function)) spansUnownedGaps = false;
            }
            Map<String, Object> record = new LinkedHashMap<>();
            record.put("name", name);
            record.put("ghidra_name", function.getName());
            record.put("start", start);
            record.put("end_exclusive", end);
            record.put("body_size", size);
            record.put("body_ranges", ranges);
            record.put("contiguous_aligned", contiguous);
            record.put("spans_unowned_gaps", spansUnownedGaps);
            record.put("status", "unreviewed analysis candidate");
            functions.add(record);
            if (contiguous || spansUnownedGaps) {
                csv.append(String.format("%s,0x%08X,0x%08X,%d%n", name, start, end, end - start));
                contiguousCount++;
            }
        }
        List<Map<String, Object>> entryInstructions = new ArrayList<>();
        Address entry = toAddr(Long.decode(args[1]));
        if (entry != null) {
            Function entryFunction = currentProgram.getFunctionManager().getFunctionContaining(entry);
            InstructionIterator instructions = entryFunction == null
                ? currentProgram.getListing().getInstructions(entry, true)
                : currentProgram.getListing().getInstructions(entryFunction.getBody(), true);
            for (int count = 0; count < 512 && instructions.hasNext(); count++) {
                Instruction instruction = instructions.next();
                Map<String, Object> record = new LinkedHashMap<>();
                record.put("address", instruction.getAddress().toString());
                record.put("instruction", instruction.toString());
                record.put("flow_type", instruction.getFlowType().toString());
                List<String> flows = new ArrayList<>();
                for (Address target : instruction.getFlows()) flows.add(target.toString());
                record.put("flows", flows);
                entryInstructions.add(record);
            }
        }
        List<Map<String, Object>> blocks = new ArrayList<>();
        for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
            Map<String, Object> record = new LinkedHashMap<>();
            record.put("name", block.getName());
            record.put("start", block.getStart().toString());
            record.put("size", block.getSize());
            record.put("execute", block.isExecute());
            blocks.add(record);
        }
        Map<String, Object> report = new LinkedHashMap<>();
        report.put("language", currentProgram.getLanguageID().toString());
        report.put("executable_sha256", currentProgram.getExecutableSHA256());
        report.put("status", "unreviewed analysis candidates");
        report.put("function_count", functions.size());
        report.put("csv_count", contiguousCount);
        report.put("functions", functions);
        report.put("entry_instructions", entryInstructions);
        List<Map<String, Object>> entryTargets = new ArrayList<>();
        List<String> seen = new ArrayList<>();
        Function entryFunction = currentProgram.getFunctionManager().getFunctionContaining(entry);
        if (entryFunction == null) throw new IllegalStateException("No function contains the declared entry.");
        for (Map<String, Object> instruction : entryInstructions) {
            @SuppressWarnings("unchecked")
            List<String> targets = (List<String>) instruction.get("flows");
            for (String targetText : targets) {
                Address target = toAddr(targetText);
                if (entryFunction.getBody().contains(target) || seen.contains(targetText)) continue;
                seen.add(targetText);
                Function function = currentProgram.getFunctionManager().getFunctionAt(target);
                Map<String, Object> record = new LinkedHashMap<>();
                record.put("target", target.getOffset());
                record.put("status", "unreviewed direct entry target");
                record.put("has_function_at_target", function != null);
                if (function != null) record.put("instructions", instructionsFor(function));
                entryTargets.add(record);
            }
        }
        report.put("entry_targets", entryTargets);
        report.put("memory_blocks", blocks);
        Files.writeString(output.resolve("candidates.csv"), csv.toString(), StandardCharsets.UTF_8);
        Files.writeString(output.resolve("analysis.json"), new GsonBuilder().setPrettyPrinting().create().toJson(report), StandardCharsets.UTF_8);
        println("BT2 candidate export: " + functions.size() + " functions; " + contiguousCount + " contiguous aligned CSV candidates. None is marked reviewed.");
    }
}
