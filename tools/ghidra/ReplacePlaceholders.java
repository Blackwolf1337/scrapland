// Replaces the 1 byte placeholder structures of the analysis (the RTTI classes at the root, the demangler's in
// /Demangler) by the types of the imported headers: a placeholder with the name of a header type, or with the old
// name of one (the Spanish RTTI names, tools/rename/names.tsv), goes, and its uses (the this of the __thiscall
// methods, pointers in signatures) get the header type. A template class has a C name without the brackets
// (cGridElements<InfPointVis*> is cGridElements_InfPointVisPtr). Only structures without any component are taken, and
// only header types that are no placeholder themselves.
//
// Headless: tools/ghidra/headless.sh ReplacePlaceholders.java <names.tsv> <header dir> ...
// rebuild.sh runs it last: tools/rename/names.tsv include/scrap third_party/python-1.5.2/Include
// @category Scrapland

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.DataType;
import ghidra.program.model.data.DataTypeManager;
import ghidra.program.model.data.Structure;

public class ReplacePlaceholders extends GhidraScript {

	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		Map<String, String> english = new HashMap<>();
		for (String line : Files.readAllLines(new File(args[0]).toPath(), StandardCharsets.UTF_8)) {
			String[] f = line.split("\t");
			if (f.length >= 2) {
				english.put(f[0], f[1]);
			}
		}
		Set<String> headers = new HashSet<>();
		for (int i = 1; i < args.length; i++) {
			String[] names = new File(args[i]).list((d, n) -> n.endsWith(".h"));
			if (names != null) {
				headers.addAll(List.of(names));
			}
		}

		DataTypeManager dtm = currentProgram.getDataTypeManager();
		Map<String, DataType> types = new HashMap<>();
		List<Structure> placeholders = new ArrayList<>();
		Iterator<DataType> it = dtm.getAllDataTypes();
		while (it.hasNext()) {
			DataType d = it.next();
			String cat = d.getCategoryPath().isRoot() ? "" : d.getCategoryPath().getPathElements()[0];
			if (headers.contains(cat)) {
				if (!placeholder(d)) {
					types.putIfAbsent(d.getName(), d);
				}
			}
			else if (placeholder(d)) {
				placeholders.add((Structure) d);
			}
		}

		int same = 0, renamed = 0;
		for (Structure p : placeholders) {
			String name = p.getName();
			DataType t = types.get(name);
			if (t == null && english.containsKey(name)) {
				t = types.get(english.get(name));
			}
			if (t == null && name.indexOf('<') > 0) {
				t = types.get(name.replace("*", "Ptr").replaceAll("[<,]", "_").replaceAll("[> ]", ""));
			}
			if (t == null) {
				continue;
			}
			String old = p.getPathName();
			dtm.replaceDataType(p, t, false);
			if (t.getName().equals(name)) {
				same++;
			}
			else {
				renamed++;
				println("ReplacePlaceholders: " + old + " -> " + t.getPathName());
			}
		}
		println(String.format("ReplacePlaceholders: %d placeholders, %d replaced by a header type of the same name, " +
			"%d by the English or template one, %d kept", placeholders.size(), same, renamed,
			placeholders.size() - same - renamed));
	}

	private static boolean placeholder(DataType d) {
		return d instanceof Structure s && s.getNumDefinedComponents() == 0 && s.getLength() <= 1;
	}
}
