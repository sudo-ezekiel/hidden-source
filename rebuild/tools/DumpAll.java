// Decompile every function in parallel and dump vtables. Args: outdir
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.mem.*;
import java.io.*;
import java.util.*;
import java.util.concurrent.*;

public class DumpAll extends GhidraScript {
    @Override
    public void run() throws Exception {
        String outdir = getScriptArgs()[0];
        Program p = currentProgram;
        // vtables
        try (PrintWriter vw = new PrintWriter(new FileWriter(outdir + "/vtables.txt"))) {
            SymbolTable st = p.getSymbolTable();
            Memory mem = p.getMemory();
            FunctionManager fm = p.getFunctionManager();
            for (Symbol s : st.getAllSymbols(true)) {
                String n = s.getName();
                if (!n.contains("vftable")) continue;
                Address a = s.getAddress();
                vw.println("VTABLE " + s.getName(true) + " @ " + a);
                for (int i = 0; i < 400; i++) {
                    Address ea = a.add(i * 4);
                    if (i > 0 && st.getPrimarySymbol(ea) != null && st.getPrimarySymbol(ea).getSource() != SourceType.DEFAULT && st.getPrimarySymbol(ea).getName().contains("vftable")) break;
                    int v;
                    try { v = mem.getInt(ea); } catch (Exception e) { break; }
                    Address t = a.getNewAddress(v & 0xffffffffL);
                    Function f = fm.getFunctionAt(t);
                    if (f == null) {
                        MemoryBlock b = mem.getBlock(t);
                        if (b == null || !b.isExecute()) break;
                    }
                    vw.println("  [" + i + "] +0x" + Integer.toHexString(i*4) + " " + t + " " + (f != null ? f.getName(true) : "?"));
                }
            }
        }
        // functions
        List<Function> funcs = new ArrayList<>();
        for (Function f : p.getFunctionManager().getFunctions(true)) funcs.add(f);
        println("functions: " + funcs.size());
        ConcurrentSkipListMap<Long, String> out = new ConcurrentSkipListMap<>();
        int nThreads = 10;
        ExecutorService ex = Executors.newFixedThreadPool(nThreads);
        ConcurrentLinkedQueue<DecompInterface> all = new ConcurrentLinkedQueue<>();
        ThreadLocal<DecompInterface> tl = ThreadLocal.withInitial(() -> {
            DecompInterface d = new DecompInterface(); all.add(d);
            DecompileOptions o = new DecompileOptions();
            d.setOptions(o);
            d.toggleCCode(true);
            d.toggleSyntaxTree(false);
            d.setSimplificationStyle("decompile");
            d.openProgram(p);
            return d;
        });
        final int[] done = {0};
        List<Future<?>> fs = new ArrayList<>();
        for (Function f : funcs) {
            fs.add(ex.submit(() -> {
                String txt;
                try {
                    DecompileResults r = tl.get().decompileFunction(f, 90, monitor);
                    if (r != null && r.decompileCompleted() && r.getDecompiledFunction() != null)
                        txt = r.getDecompiledFunction().getC();
                    else txt = "/* decompile failed: " + (r == null ? "null" : r.getErrorMessage()) + " */\n";
                } catch (Exception e) { txt = "/* exception " + e + " */\n"; }
                out.put(f.getEntryPoint().getOffset(), "\n//==== FUNC " + f.getEntryPoint() + " " + f.getName(true) + " size=" + f.getBody().getNumAddresses() + "\n" + txt);
                synchronized (done) { done[0]++; if (done[0] % 1000 == 0) println("decompiled " + done[0]); }
            }));
        }
        for (Future<?> fu : fs) fu.get();
        ex.shutdown();
        for (DecompInterface d : all) d.dispose();
        try (PrintWriter w = new PrintWriter(new BufferedWriter(new FileWriter(outdir + "/decomp.c")))) {
            for (String s : out.values()) w.print(s);
        }
        println("done");
    }
}
