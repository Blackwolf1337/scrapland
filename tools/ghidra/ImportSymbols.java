// Reads the text files written by ExportSymbols.java back into a Ghidra database: creates missing functions, sets
// names and namespaces, applies signatures, global labels with their data types, and comments. Rows with source
// DEFAULT only create the function. Run it on a freshly analyzed import of the same Scrap.exe.
//
// Headless: analyzeHeadless ghidra Scrapland -process Scrap.exe -noanalysis -scriptPath tools/ghidra
//           -postScript ImportSymbols.java <input directory>
// @category Scrapland

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.List;

import ghidra.app.cmd.function.ApplyFunctionSignatureCmd;
import ghidra.app.cmd.function.FunctionRenameOption;
import ghidra.app.services.DataTypeManagerService;
import ghidra.app.util.NamespaceUtils;
import ghidra.app.util.cparser.C.CParserUtils;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.DataType;
import ghidra.program.model.data.DataUtilities;
import ghidra.program.model.data.FunctionDefinitionDataType;
import ghidra.program.model.listing.CommentType;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Namespace;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;
import ghidra.app.script.GhidraScript;

public class ImportSymbols extends GhidraScript {

	public static String unesc(String s) {
		StringBuilder b = new StringBuilder(s.length());
		for (int i = 0; i < s.length(); i++) {
			char c = s.charAt(i);
			if (c == '\\' && i + 1 < s.length()) {
				char n = s.charAt(++i);
				b.append(n == 'n' ? '\n' : n == 't' ? '\t' : n);
			}
			else {
				b.append(c);
			}
		}
		return b.toString();
	}

	static List<String[]> readTsv(File f) throws Exception {
		List<String[]> rows = new ArrayList<>();
		if (!f.exists()) {
			return rows;
		}
		try (BufferedReader r = new BufferedReader(new FileReader(f))) {
			String line = r.readLine(); // header
			while ((line = r.readLine()) != null) {
				if (!line.isEmpty()) {
					rows.add(line.split("\t", -1));
				}
			}
		}
		return rows;
	}

	Address addr(String rva) {
		return currentProgram.getImageBase().add(Long.parseLong(rva, 16));
	}

	// "A::B::name" -> namespace A::B (created if needed) and the plain name. Template arguments may hold "::".
	void setName(Address a, Symbol sym, String full, SourceType src) throws Exception {
		int depth = 0, split = -1;
		for (int i = 0; i < full.length() - 1; i++) {
			char c = full.charAt(i);
			if (c == '<') depth++;
			else if (c == '>') depth--;
			else if (depth == 0 && c == ':' && full.charAt(i + 1) == ':') split = i;
		}
		Namespace ns = currentProgram.getGlobalNamespace();
		String name = full;
		if (split >= 0) {
			ns = NamespaceUtils.createNamespaceHierarchy(full.substring(0, split), null, currentProgram, src);
			name = full.substring(split + 2);
		}
		if (sym == null) {
			currentProgram.getSymbolTable().createLabel(a, name, ns, src);
		}
		else if (!sym.getName(true).equals(full) || sym.getSource() != src) {
			sym.setNameAndNamespace(name, ns, src);
			// an unchanged name keeps its old source in setNameAndNamespace
			if (sym.getSource() != src) {
				sym.setSource(src);
			}
		}
	}

	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		File dir = new File(args.length > 0 ? args[0] : "symbols");
		int nfunc = 0, nsig = 0, nglob = 0, ncomm = 0, fail = 0;

		for (String[] r : readTsv(new File(dir, "functions.tsv"))) {
			Address a = addr(r[0]);
			SourceType src = SourceType.valueOf(r[2]);
			Function f = getFunctionAt(a);
			if (f == null) {
				disassemble(a);
				f = createFunction(a, null);
				if (f == null) {
					printerr("ImportSymbols: no function at " + r[0] + " " + r[4]);
					fail++;
					continue;
				}
			}
			nfunc++;
			if (src == SourceType.DEFAULT) {
				continue;
			}
			try {
				setName(a, f.getSymbol(), unesc(r[4]), src);
				// the C parser does not know every convention keyword: it comes from its own column instead
				String sig = unesc(r[5]).replaceAll("\\b__(cdecl|stdcall|fastcall|thiscall|vectorcall)\\b", "")
					.replaceAll(" +\\(", "(").trim();
				if (!sig.isEmpty() && (src == SourceType.USER_DEFINED || src == SourceType.IMPORTED)) {
					int p = sig.indexOf('(');
					FunctionDefinitionDataType def = CParserUtils.parseSignature((DataTypeManagerService) null,
						currentProgram, sig.substring(0, p) + " _f" + sig.substring(p), false);
					if (def != null && new ApplyFunctionSignatureCmd(a, def, src, true, false, null,
						FunctionRenameOption.NO_CHANGE).applyTo(currentProgram)) {
						f = getFunctionAt(a);
						nsig++;
					}
					else {
						printerr("ImportSymbols: signature not applied at " + r[0] + ": " + sig);
						fail++;
					}
				}
			}
			catch (Exception e) {
				printerr("ImportSymbols: " + r[0] + " " + r[4] + ": " + e.getMessage());
				fail++;
			}
			try {
				String cc = unesc(r[3]);
				if (!cc.isEmpty() && !cc.equals(f.getCallingConventionName())) {
					f.setCallingConvention(cc);
				}
			}
			catch (Exception e) {
				printerr("ImportSymbols: " + r[0] + " calling convention " + r[3] + ": " + e.getMessage());
				fail++;
			}
		}

		for (String[] r : readTsv(new File(dir, "globals.tsv"))) {
			Address a = addr(r[0]);
			SourceType src = SourceType.valueOf(r[1]);
			String full = unesc(r[2]);
			try {
				Symbol existing = null;
				for (Symbol s : currentProgram.getSymbolTable().getSymbols(a)) {
					if (s.getName(true).equals(full)) {
						existing = s;
					}
				}
				if (existing == null) {
					setName(a, null, full, src);
				}
				String type = unesc(r[3]);
				if (!type.isEmpty()) {
					DataType dt = currentProgram.getDataTypeManager().getDataType(type);
					ghidra.program.model.listing.Data have = getDataAt(a);
					if (have != null && have.getDataType().getPathName().equals(type)) {
						dt = null; // the analysis already made the same data
					}
					if (dt != null) {
						// a type given by hand wins over data the analysis guessed there
						DataUtilities.createData(currentProgram, a, dt, -1, src == SourceType.USER_DEFINED
							? DataUtilities.ClearDataMode.CLEAR_ALL_CONFLICT_DATA
							: DataUtilities.ClearDataMode.CLEAR_ALL_UNDEFINED_CONFLICT_DATA);
					}
				}
				nglob++;
			}
			catch (Exception e) {
				printerr("ImportSymbols: global " + r[0] + " " + full + ": " + e.getMessage());
				fail++;
			}
		}

		for (String[] r : readTsv(new File(dir, "comments.tsv"))) {
			currentProgram.getListing().setComment(addr(r[0]), CommentType.valueOf(r[1]), unesc(r[2]));
			ncomm++;
		}

		println(String.format("ImportSymbols: %d functions (%d signatures), %d globals, %d comments, %d failed",
			nfunc, nsig, nglob, ncomm, fail));
	}
}
