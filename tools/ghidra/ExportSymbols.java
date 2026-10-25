// Writes the knowledge of the Ghidra database as text files, so git holds it and any Ghidra project can be rebuilt
// from them with ImportSymbols.java. Addresses are RVAs (image base 0x400000 subtracted), the exe uses ASLR.
//
//   functions.tsv  rva  size  source  callconv  name  signature
//   globals.tsv    rva  source  name  datatype      (non function symbols with a name that is not a default one)
//   comments.tsv   rva  kind  text                  (newlines and tabs escaped as \n and \t)
//   types.h        the data types of the /Scrap category
//
// Headless: analyzeHeadless ghidra Scrapland -process Scrap.exe -noanalysis -scriptPath tools/ghidra
//           -postScript ExportSymbols.java <output directory>
// @category Scrapland

import java.io.File;
import java.io.FileWriter;
import java.io.PrintWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressIterator;
import ghidra.program.model.data.Category;
import ghidra.program.model.data.CategoryPath;
import ghidra.program.model.data.DataTypeWriter;
import ghidra.program.model.listing.CommentType;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Parameter;
import ghidra.program.model.listing.Listing;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;
import ghidra.program.model.symbol.SymbolType;

public class ExportSymbols extends GhidraScript {

	static final CommentType[] COMMENT_KINDS = { CommentType.PLATE, CommentType.PRE, CommentType.EOL,
		CommentType.POST, CommentType.REPEATABLE };

	static String esc(String s) {
		return s == null ? "" : s.replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n").replace("\r", "");
	}

	String rva(Address a) {
		return String.format("%08x", a.subtract(currentProgram.getImageBase()));
	}

	// "rettype __cc(type name, ...)" without the function name and without auto parameters (the this of a
	// __thiscall), so CParserUtils can read it back in ImportSymbols.java
	static String signature(Function f) {
		StringBuilder b = new StringBuilder(f.getReturnType().getDisplayName());
		String cc = f.getCallingConventionName();
		if (cc != null && !cc.equals("unknown") && !cc.equals("default")) {
			b.append(' ').append(cc);
		}
		b.append('(');
		boolean first = true;
		for (Parameter p : f.getParameters()) {
			if (p.isAutoParameter()) {
				continue;
			}
			b.append(first ? "" : ", ").append(p.getDataType().getDisplayName()).append(' ').append(p.getName());
			first = false;
		}
		if (f.hasVarArgs()) {
			b.append(first ? "..." : ", ...");
		}
		else if (first) {
			b.append("void");
		}
		return b.append(')').toString();
	}

	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		File dir = new File(args.length > 0 ? args[0] : "symbols");
		dir.mkdirs();

		int nfunc = 0, nglob = 0, ncomm = 0;
		try (PrintWriter w = new PrintWriter(new FileWriter(new File(dir, "functions.tsv")))) {
			w.println("rva\tsize\tsource\tcallconv\tname\tsignature");
			for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
				if (f.isThunk() && f.isExternal()) {
					continue;
				}
				SourceType src = f.getSymbol().getSource();
				w.printf("%s\t%d\t%s\t%s\t%s\t%s%n", rva(f.getEntryPoint()), f.getBody().getNumAddresses(), src,
					esc(f.getCallingConventionName()), esc(f.getName(true)),
					src == SourceType.DEFAULT ? "" : esc(signature(f)));
				nfunc++;
			}
		}

		Listing listing = currentProgram.getListing();
		try (PrintWriter w = new PrintWriter(new FileWriter(new File(dir, "globals.tsv")))) {
			w.println("rva\tsource\tname\tdatatype");
			SymbolIterator it = currentProgram.getSymbolTable().getAllSymbols(true);
			while (it.hasNext()) {
				Symbol s = it.next();
				if (s.isExternal() || s.getSource() == SourceType.DEFAULT || s.getSymbolType() != SymbolType.LABEL) {
					continue;
				}
				if (!s.getAddress().isMemoryAddress()) {
					continue;
				}
				Data d = listing.getDefinedDataAt(s.getAddress());
				w.printf("%s\t%s\t%s\t%s%n", rva(s.getAddress()), s.getSource(), esc(s.getName(true)),
					d == null ? "" : esc(d.getDataType().getPathName()));
				nglob++;
			}
		}

		try (PrintWriter w = new PrintWriter(new FileWriter(new File(dir, "comments.tsv")))) {
			w.println("rva\tkind\ttext");
			AddressIterator it = listing.getCommentAddressIterator(currentProgram.getMemory(), true);
			while (it.hasNext()) {
				Address a = it.next();
				for (CommentType k : COMMENT_KINDS) {
					String c = listing.getComment(k, a);
					if (c != null && !c.isEmpty()) {
						w.printf("%s\t%s\t%s%n", rva(a), k, esc(c));
						ncomm++;
					}
				}
			}
		}

		Category cat = currentProgram.getDataTypeManager().getCategory(new CategoryPath("/Scrap"));
		try (FileWriter fw = new FileWriter(new File(dir, "types.h"))) {
			fw.write("// Generated by tools/ghidra/ExportSymbols.java from the /Scrap category, do not edit.\n");
			if (cat != null) {
				new DataTypeWriter(currentProgram.getDataTypeManager(), fw).write(cat, monitor);
			}
		}

		println(String.format("ExportSymbols: %d functions, %d globals, %d comments -> %s", nfunc, nglob, ncomm,
			dir.getPath()));
	}
}
