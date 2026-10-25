// Names functions of the statically linked open source libraries (Python 1.5.2, zlib 1.1.3, libpng 1.0.5,
// libjpeg 6a) through the string literals only one source function has (symbols/srcstrings.tsv, written by
// tools/srcstrings.py). Every exe function that uses such a string gets a vote for that name. A name is taken when
// the votes agree: one name has at least two thirds of the function's votes, and no other function has as many
// votes for it (MSVC inlines small functions, so a string can show up in a caller). Only unnamed functions are
// renamed, the plate comment gets the source file.
//
// Headless: tools/ghidra/headless.sh NameFromStrings.java symbols/srcstrings.tsv
// @category Scrapland

import java.io.BufferedReader;
import java.io.FileReader;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.CommentType;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.SourceType;

public class NameFromStrings extends GhidraScript {

	@Override
	protected void run() throws Exception {
		ScrapUtil u = new ScrapUtil(this, currentProgram, monitor);
		String[] args = getScriptArgs();
		String path = args.length > 0 ? args[0] : "symbols/srcstrings.tsv";

		Map<Function, Map<String, Integer>> votes = new HashMap<>();
		Map<String, String> sourceFile = new HashMap<>();
		int nstrings = 0, found = 0;
		try (BufferedReader r = new BufferedReader(new FileReader(path, StandardCharsets.UTF_8))) {
			String line;
			while ((line = r.readLine()) != null) {
				String[] f = line.split("\t", -1);
				if (f.length < 3) {
					continue;
				}
				nstrings++;
				String str = ImportSymbols.unesc(f[0]);
				sourceFile.put(f[1], f[2]);
				byte[] s = str.getBytes(StandardCharsets.ISO_8859_1);
				byte[] pat = new byte[s.length + 1];
				System.arraycopy(s, 0, pat, 0, s.length);
				Set<Function> users = new HashSet<>();
				for (MemoryBlock b : u.mem.getBlocks()) {
					if (!b.isInitialized() || b.isExecute()) {
						continue;
					}
					Address at = b.getStart();
					while ((at = u.mem.findBytes(at, b.getEnd(), pat, null, true, monitor)) != null) {
						boolean start = at.equals(b.getStart()) || u.mem.getByte(at.subtract(1)) == 0;
						if (start) {
							for (Address from : u.users(at)) {
								Function fn = getFunctionContaining(from);
								if (fn != null) {
									users.add(fn);
								}
							}
						}
						at = at.add(1);
					}
				}
				if (!users.isEmpty()) {
					found++;
				}
				for (Function fn : users) {
					votes.computeIfAbsent(fn, k -> new HashMap<>()).merge(f[1], 1, Integer::sum);
				}
			}
		}

		// best function per name
		Map<String, Function> owner = new HashMap<>();
		Map<String, Integer> ownerVotes = new HashMap<>();
		Set<String> tied = new HashSet<>();
		for (Map.Entry<Function, Map<String, Integer>> e : votes.entrySet()) {
			int total = e.getValue().values().stream().mapToInt(Integer::intValue).sum();
			for (Map.Entry<String, Integer> v : e.getValue().entrySet()) {
				if (v.getValue() * 3 < total * 2) {
					continue;
				}
				int prev = ownerVotes.getOrDefault(v.getKey(), 0);
				if (v.getValue() > prev) {
					owner.put(v.getKey(), e.getKey());
					ownerVotes.put(v.getKey(), v.getValue());
					tied.remove(v.getKey());
				}
				else if (v.getValue() == prev) {
					tied.add(v.getKey());
				}
			}
		}

		int named = 0, kept = 0;
		for (Map.Entry<String, Function> e : owner.entrySet()) {
			if (tied.contains(e.getKey())) {
				continue;
			}
			Function fn = e.getValue();
			if (fn.getSymbol().getSource() != SourceType.DEFAULT) {
				kept++;
				continue;
			}
			fn.getSymbol().setName(e.getKey(), SourceType.ANALYSIS);
			if (getPlateComment(fn.getEntryPoint()) == null) {
				currentProgram.getListing().setComment(fn.getEntryPoint(), CommentType.PLATE,
					"src: " + sourceFile.get(e.getKey()));
			}
			named++;
		}
		println(String.format("NameFromStrings: %d strings, %d found in use, %d names agreed, %d named, %d already " +
			"named, %d ties skipped", nstrings, found, owner.size(), named, kept, tied.size()));
	}
}
