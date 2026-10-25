// Parses C headers into the data types of the program with Ghidra's preprocessor and C parser. The project headers
// are the source of truth for types; Ghidra files each type under a category named after its header.
//
// Headless: tools/ghidra/headless.sh ImportHeaders.java <header> [-I<include dir> ...] [-D<define> ...]
// rebuild.sh imports third_party/python-1.5.2/Include/Python.h with the stand-in headers of tools/ghidra/include.
// @category Scrapland

import java.util.ArrayList;
import java.util.List;

import ghidra.app.script.GhidraScript;
import ghidra.app.util.cparser.C.CParserUtils;
import ghidra.app.util.cparser.C.CParserUtils.CParseResults;
import ghidra.program.model.data.DataTypeManager;

public class ImportHeaders extends GhidraScript {

	@Override
	protected void run() throws Exception {
		List<String> files = new ArrayList<>(), includes = new ArrayList<>(), defines = new ArrayList<>();
		for (String a : getScriptArgs()) {
			if (a.startsWith("-I")) {
				includes.add(a.substring(2));
			}
			else if (a.startsWith("-D")) {
				defines.add(a);
			}
			else {
				files.add(a);
			}
		}
		DataTypeManager dtm = currentProgram.getDataTypeManager();
		int before = dtm.getDataTypeCount(true);
		CParseResults r = CParserUtils.parseHeaderFiles(new DataTypeManager[0], files.toArray(new String[0]),
			includes.toArray(new String[0]), defines.toArray(new String[0]), dtm, monitor);
		if (!r.successful()) {
			printerr("ImportHeaders: parse failed\n" + r.cppParseMessages() + "\n" + r.cParseMessages());
		}
		println(String.format("ImportHeaders: %s, %d data types added", String.join(" ", files),
			dtm.getDataTypeCount(true) - before));
	}
}
