// Create functions at seed addresses and label vftables, then re-run auto-analysis on the changes.
// Args: <seeds.txt> <labels.txt> [gaps]
//   seeds.txt: one hex address per line (pe_seeds.py); labels.txt: "hex<TAB>label".
//   gaps: afterwards also create functions in undefined .text gaps that start on a 16-byte
//   boundary after int3/nop padding (MSVC pads between functions with 0xCC).
// Run with -noanalysis against an existing project; the program is saved on exit.
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.mem.*;
import java.io.*;
import java.nio.file.*;
import java.util.*;

public class RecoverFunctions extends GhidraScript {
    int created = 0, inside = 0, failed = 0;

    boolean seed(Address a) throws Exception {
        FunctionManager fm = currentProgram.getFunctionManager();
        if (fm.getFunctionAt(a) != null) return false;
        Function owner = fm.getFunctionContaining(a);
        if (owner != null) { inside++; return false; }
        Listing l = currentProgram.getListing();
        if (l.getInstructionAt(a) == null) {
            Instruction over = l.getInstructionContaining(a);
            if (over != null) clearListing(over.getMinAddress(), over.getMaxAddress());
            Data d = l.getDefinedDataContaining(a);
            if (d != null) clearListing(d.getMinAddress(), d.getMaxAddress());
            DisassembleCommand cmd = new DisassembleCommand(a, null, true);
            cmd.applyTo(currentProgram, monitor);
        }
        if (l.getInstructionAt(a) == null) { failed++; return false; }
        Function f = createFunction(a, null);
        if (f == null) { failed++; return false; }
        created++;
        return true;
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        AddressSpace sp = currentProgram.getAddressFactory().getDefaultAddressSpace();
        SymbolTable st = currentProgram.getSymbolTable();
        for (String line : Files.readAllLines(Paths.get(args[1]))) {
            String[] p = line.split("\t");
            if (p.length < 2) continue;
            Address a = sp.getAddress(Long.parseLong(p[0], 16));
            st.createLabel(a, p[1], SourceType.USER_DEFINED).setPrimary();
        }
        List<String> seeds = Files.readAllLines(Paths.get(args[0]));
        for (String s : seeds) {
            if (s.isBlank()) continue;
            seed(sp.getAddress(Long.parseLong(s.trim(), 16)));
        }
        println("seeds: created " + created + ", inside existing " + inside + ", failed " + failed);
        analyzeChanges(currentProgram);
        println("functions after seed analysis: " + currentProgram.getFunctionManager().getFunctionCount());
        if (args.length > 2 && args[2].equals("gaps")) {
            for (int pass = 0; pass < 4; pass++) {
                int before = created;
                gaps();
                analyzeChanges(currentProgram);
                println("gap pass " + pass + ": created " + (created - before) + ", functions "
                        + currentProgram.getFunctionManager().getFunctionCount());
                if (created == before) break;
            }
        }
    }

    void gaps() throws Exception {
        // MSVC 7.1 aligns functions to 16 bytes and pads with int3, so every 16-byte boundary that
        // follows an int3 (or a function's last byte) and is not padding itself starts a function.
        // Ghidra often disassembled these already without making a function, or made them data.
        Memory mem = currentProgram.getMemory();
        MemoryBlock text = mem.getBlock(".text");
        FunctionManager fm = currentProgram.getFunctionManager();
        long start = text.getStart().getOffset(), end = text.getEnd().getOffset();
        List<Address> cands = new ArrayList<>();
        for (long o = (start + 15) & ~15L; o < end; o += 16) {
            Address a = text.getStart().getNewAddress(o);
            if (fm.getFunctionContaining(a) != null) continue;
            int b = mem.getByte(a) & 0xff;
            if (b == 0xcc || b == 0x00) continue;
            Address prev = a.subtract(1);
            int pb = mem.getByte(prev) & 0xff;
            Function pf = fm.getFunctionContaining(prev);
            if (pb == 0xcc || (pf != null && pf.getBody().getMaxAddress().equals(prev))) cands.add(a);
        }
        for (Address c : cands) seed(c);
    }
}
