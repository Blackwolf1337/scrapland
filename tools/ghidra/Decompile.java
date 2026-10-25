// Prints Ghidra's decompiler output for functions, given by name (as in symbols/functions.tsv) or by RVA (hex).
// For reading only: decompiled code is the starting point of a C reimplementation, never copied in unchecked.
//
// Headless: tools/ghidra/headless.sh Decompile.java <output file> <name or rva> ...
// @category Scrapland

import java.io.FileWriter;
import java.io.PrintWriter;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Symbol;

public class Decompile extends GhidraScript {

	Function lookup(String key) {
		if (key.matches("(0x)?[0-9a-fA-F]{5,8}")) {
			Address a = currentProgram.getImageBase().add(Long.parseLong(key.replaceFirst("^0x", ""), 16));
			return getFunctionAt(a);
		}
		for (Symbol s : currentProgram.getSymbolTable().getAllSymbols(true)) {
			if (s.getName(true).equals(key) || s.getName().equals(key)) {
				Function f = getFunctionAt(s.getAddress());
				if (f != null) {
					return f;
				}
			}
		}
		return null;
	}

	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		DecompInterface d = new DecompInterface();
		d.openProgram(currentProgram);
		try (PrintWriter w = new PrintWriter(new FileWriter(args[0]))) {
			for (int i = 1; i < args.length; i++) {
				Function f = lookup(args[i]);
				if (f == null) {
					w.printf("// %s: no such function%n%n", args[i]);
					continue;
				}
				DecompileResults r = d.decompileFunction(f, 60, monitor);
				w.printf("// %s  rva %08x  size %d%n", f.getName(true),
					f.getEntryPoint().subtract(currentProgram.getImageBase()), f.getBody().getNumAddresses());
				w.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// " + r.getErrorMessage());
			}
		}
		d.dispose();
	}
}
