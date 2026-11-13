// Parses C headers into the data types of the program with Ghidra's preprocessor and C parser. The project headers
// are the source of truth for types; Ghidra files each type under a category named after its header.
//
// Headless: tools/ghidra/headless.sh ImportHeaders.java <header> [-I<include dir> ...] [-D<define> ...]
//                                    [-C<from>=<to> ...]
// rebuild.sh imports tools/ghidra/python152/Include/Python.h and tools/ghidra/include/scrap_all.h (the engine
// headers of include/scrap) with the stand-in headers of tools/ghidra/include.
//
// Between the preprocessor and the parser the bodies of function definitions (the static inline functions of the
// engine headers) are cut to a ';', so they become prototypes: Ghidra's parser is C89 and knows no statement
// expressions, and only the types are wanted. Enum constants in array sizes get their values, which Ghidra's parser
// takes for 0. -C<from>=<to> sets the calling convention <to> on every function definition the parse gave <from>:
// Ghidra's parser has no __thiscall, so the stand-ins write it as __vectorcall (tools/ghidra/include/gccext.h) and
// rebuild.sh passes -C__vectorcall=__thiscall.
// The parser input is saved as ImportHeaders.c in Ghidra's user temp directory, the line numbers of parse errors
// refer to it; the trace of the preprocessor goes to ImportHeaders.cpp.log there.
// @category Scrapland

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.PrintStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import ghidra.app.script.GhidraScript;
import ghidra.app.util.cparser.C.CParser;
import ghidra.app.util.cparser.CPP.PreProcessor;
import ghidra.framework.Application;
import ghidra.program.model.data.DataType;
import ghidra.program.model.data.DataTypeManager;
import ghidra.program.model.data.Enum;
import ghidra.program.model.data.FunctionDefinition;
import ghidra.program.model.data.StandAloneDataTypeManager;

public class ImportHeaders extends GhidraScript {

	@Override
	protected void run() throws Exception {
		List<String> files = new ArrayList<>(), includes = new ArrayList<>(), defines = new ArrayList<>();
		Map<String, String> conventions = new LinkedHashMap<>();
		for (String a : getScriptArgs()) {
			if (a.startsWith("-I")) {
				includes.add(a.substring(2));
			}
			else if (a.startsWith("-D")) {
				defines.add(a);
			}
			else if (a.startsWith("-C") && a.indexOf('=') > 2) {
				conventions.put(a.substring(2, a.indexOf('=')), a.substring(a.indexOf('=') + 1));
			}
			else {
				files.add(a);
			}
		}
		DataTypeManager dtm = currentProgram.getDataTypeManager();
		int before = dtm.getDataTypeCount(true);

		// the steps of CParserUtils.parseHeaderFiles, with the body cut in between
		PreProcessor cpp = new PreProcessor();
		cpp.setArgs(defines.toArray(new String[0]));
		cpp.addIncludePaths(includes.toArray(new String[0]));
		cpp.setMonitor(monitor);
		ByteArrayOutputStream pre = new ByteArrayOutputStream();
		cpp.setOutputStream(pre);
		PrintStream out = System.out;
		File tmp = Application.getUserTempDirectory();
		try (PrintStream log = new PrintStream(new File(tmp, "ImportHeaders.cpp.log"))) {
			System.setOut(log);	// the preprocessor writes its trace there
			for (String f : files) {
				cpp.parse(f);
			}
		}
		finally {
			System.setOut(out);
		}
		// as in CParserUtils: a preprocessor error throws, the parse result only tells of #error lines
		cpp.getDefinitions().populateDefineEquates(new DataTypeManager[0], dtm);

		int[] cut = new int[1], sizes = new int[1];
		String text = cutBodies(pre.toString(StandardCharsets.ISO_8859_1), cut);
		text = enumSizes(text, sizes);
		File input = new File(tmp, "ImportHeaders.c");
		Files.writeString(input.toPath(), text, StandardCharsets.ISO_8859_1);

		CParser parser = new CParser(dtm, true, new DataTypeManager[0]);
		parser.setParseFileName(input.getPath());
		parser.setMonitor(monitor);
		try {
			parser.parse(new ByteArrayInputStream(text.getBytes(StandardCharsets.ISO_8859_1)));
		}
		catch (Exception e) {
			printerr("ImportHeaders: parse failed (" + input + ")\n" + e.getMessage());
			return;
		}
		if (!parser.didParseSucceed()) {
			printerr("ImportHeaders: parse failed (" + input + ")\n" + parser.getParseMessages());
			return;
		}

		int changed = 0;
		if (!conventions.isEmpty()) {
			Iterator<DataType> it = dtm.getAllDataTypes();
			while (it.hasNext()) {
				if (it.next() instanceof FunctionDefinition fd &&
					conventions.containsKey(fd.getCallingConventionName())) {
					fd.setCallingConvention(conventions.get(fd.getCallingConventionName()));
					changed++;
				}
			}
		}
		println(String.format("ImportHeaders: %s, %d data types added, %d function bodies cut, %d enum array sizes, " +
			"%d calling conventions set", String.join(" ", files), dtm.getDataTypeCount(true) - before, cut[0],
			sizes[0], changed));
	}

	private static final Pattern DIM = Pattern.compile("\\[([^\\[\\]\n]*[A-Za-z_][^\\[\\]\n]*)\\]");
	private static final Pattern NAME = Pattern.compile("\\b[A-Za-z_]\\w*\\b");

	// Ghidra's parser takes an enum constant as array size for 0. A first parse into a scratch manager gives the
	// values of the enum constants, which then replace the names in the array sizes.
	private String enumSizes(String text, int[] count) throws Exception {
		if (!DIM.matcher(text).find()) {
			return text;
		}
		Map<String, Long> values = new HashMap<>();
		StandAloneDataTypeManager scratch = new StandAloneDataTypeManager("ImportHeaders");
		int tx = scratch.startTransaction("enums");
		try {
			new CParser(scratch, true, new DataTypeManager[0])
				.parse(new ByteArrayInputStream(text.getBytes(StandardCharsets.ISO_8859_1)));
			Iterator<DataType> it = scratch.getAllDataTypes();
			while (it.hasNext()) {
				if (it.next() instanceof Enum e) {
					for (String n : e.getNames()) {
						values.put(n, e.getValue(n));
					}
				}
			}
		}
		catch (Exception e) {
			return text;	// the real parse reports it
		}
		finally {
			scratch.endTransaction(tx, false);
			scratch.close();
		}
		StringBuilder b = new StringBuilder(text.length());
		for (String line : text.split("\n", -1)) {
			if (!line.startsWith("//") && !line.startsWith("#")) {
				Matcher m = DIM.matcher(line);
				StringBuilder l = new StringBuilder();
				while (m.find()) {
					Matcher w = NAME.matcher(m.group(1));
					StringBuilder d = new StringBuilder();
					while (w.find()) {
						Long v = values.get(w.group());
						if (v != null) {
							count[0]++;
						}
						w.appendReplacement(d, v != null ? v.toString() : w.group());
					}
					w.appendTail(d);
					m.appendReplacement(l, Matcher.quoteReplacement("[" + d + "]"));
				}
				m.appendTail(l);
				line = l.toString();
			}
			b.append(line).append('\n');
		}
		b.setLength(b.length() - 1);
		return b.toString();
	}

	// Replaces every function body at file scope (a '{' after a ')' that does not close an __attribute__ or
	// __declspec) by ';'. Keeps the newlines, so the line numbers stay. Skips strings, character constants,
	// comments and the #line lines of Ghidra's preprocessor.
	private static String cutBodies(String s, int[] count) {
		StringBuilder b = new StringBuilder(s.length());
		int n = s.length(), parens = 0, braces = 0, body = -1, word = -1;
		char last = 0;	// the last character outside comments and blanks, at file scope
		boolean attr = false;	// the open parenthesis at file scope belongs to an __attribute__
		for (int i = 0; i < n; i++) {
			char c = s.charAt(i);
			int skip = i;
			if (c == '"' || c == '\'') {
				for (skip = i + 1; skip < n && s.charAt(skip) != c && s.charAt(skip) != '\n'; skip++) {
					if (s.charAt(skip) == '\\') {
						skip++;
					}
				}
			}
			else if (c == '/' && i + 1 < n && s.charAt(i + 1) == '/' ||
				c == '#' && (i == 0 || s.charAt(i - 1) == '\n')) {
				skip = s.indexOf('\n', i);
				skip = (skip < 0 ? n : skip) - 1;
			}
			else if (c == '/' && i + 1 < n && s.charAt(i + 1) == '*') {
				skip = s.indexOf("*/", i + 2);
				skip = skip < 0 ? n - 1 : skip + 1;
			}
			if (skip != i) {
				if (body < 0) {
					b.append(s, i, Math.min(skip + 1, n));
				}
				else {
					keepNewlines(b, s, i, Math.min(skip + 1, n));
				}
				i = skip;
				continue;
			}
			if (body >= 0) {
				if (c == '{') {
					braces++;
				}
				else if (c == '}' && --braces == 0) {
					body = -1;
					last = ';';
					continue;
				}
				if (c == '\n') {
					b.append('\n');
				}
				continue;
			}
			if (Character.isJavaIdentifierPart(c)) {
				if (i == 0 || !Character.isJavaIdentifierPart(s.charAt(i - 1))) {
					word = i;
				}
			}
			else if (!Character.isWhitespace(c)) {
				if (c == '(' && parens == 0 && braces == 0) {
					int e = word;
					while (e >= 0 && Character.isJavaIdentifierPart(s.charAt(e))) {
						e++;
					}
					String w = word < 0 ? "" : s.substring(word, e);
					attr = w.equals("__attribute__") || w.equals("__attribute") || w.equals("__declspec");
				}
				word = -1;
			}
			if (c == '(') {
				parens++;
			}
			else if (c == ')' && --parens == 0 && braces == 0 && attr) {
				b.append(c);
				last = 'A';
				continue;
			}
			else if (c == '{') {
				if (braces == 0 && parens == 0 && last == ')') {
					body = i;
					braces = 1;
					count[0]++;
					b.append(';');
					continue;
				}
				braces++;
			}
			else if (c == '}') {
				braces--;
			}
			if (!Character.isWhitespace(c)) {
				last = c;
			}
			b.append(c);
		}
		return b.toString();
	}

	private static void keepNewlines(StringBuilder b, String s, int from, int to) {
		for (int i = from; i < to; i++) {
			if (s.charAt(i) == '\n') {
				b.append('\n');
			}
		}
	}
}
