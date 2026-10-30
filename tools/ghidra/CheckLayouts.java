// Checks the struct layouts of the imported headers against the SD_SIZE and SD_OFFSET asserts of include/scrap: the
// size of each type and the offset of each member, as Ghidra laid them out. Prints the mismatches and a count.
//
// Headless: tools/ghidra/headless.sh CheckLayouts.java [header dir]   default include/scrap
// rebuild.sh runs it after ImportHeaders.
// @category Scrapland

import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.Array;
import ghidra.program.model.data.Composite;
import ghidra.program.model.data.DataType;
import ghidra.program.model.data.DataTypeComponent;
import ghidra.program.model.data.DataTypeManager;
import ghidra.program.model.data.TypeDef;

public class CheckLayouts extends GhidraScript {

	private static final Pattern DEFINE =
		Pattern.compile("^#define\\s+(\\w+)\\s+([^\\s/][^/\\n]*?)\\s*(/\\*.*)?$", Pattern.MULTILINE);
	private static final Pattern ASSERT =
		Pattern.compile("^\\s*SD_(OFFSET|SIZE)\\(\\s*(\\w+)\\s*,\\s*([^;]*)\\)\\s*;", Pattern.MULTILINE);

	private final Map<String, String> defines = new HashMap<>();

	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		File dir = new File(args.length > 0 ? args[0] : "include/scrap");
		// the types of the parsed headers by name: categories named after a header
		Map<String, DataType> types = new HashMap<>();
		DataTypeManager dtm = currentProgram.getDataTypeManager();
		Iterator<DataType> it = dtm.getAllDataTypes();
		while (it.hasNext()) {
			DataType d = it.next();
			if (d.getCategoryPath().getName().endsWith(".h")) {
				types.putIfAbsent(d.getName(), d);
			}
		}
		int sizes = 0, offsets = 0, skipped = 0;
		List<String> bad = new ArrayList<>();
		File[] files = dir.listFiles((d, n) -> n.endsWith(".h"));
		Arrays.sort(files);
		Map<String, String> texts = new LinkedHashMap<>();
		for (File f : files) {
			String text = Files.readString(f.toPath(), StandardCharsets.ISO_8859_1);
			texts.put(f.getName(), text);
			Matcher d = DEFINE.matcher(text);
			while (d.find()) {
				defines.putIfAbsent(d.group(1), d.group(2));
			}
		}
		for (Map.Entry<String, String> f : texts.entrySet()) {
			Matcher m = ASSERT.matcher(f.getValue());
			while (m.find()) {
				String where = f.getKey() + ": SD_" + m.group(1) + "(" + m.group(2) + ", " + m.group(3) + ")";
				DataType t = types.get(m.group(2));
				String[] a = m.group(3).split(",");
				Long want = eval(a[a.length - 1]);
				if (t == null || want == null || m.group(1).equals("OFFSET") && a.length != 2) {
					skipped++;
					printerr("CheckLayouts: not checked, " + where + (t == null ? " (type not imported)" : ""));
					continue;
				}
				t = strip(t);
				if (m.group(1).equals("SIZE")) {
					sizes++;
					if (t.getLength() != want) {
						bad.add(where + ": Ghidra 0x" + Integer.toHexString(t.getLength()));
					}
					continue;
				}
				offsets++;
				Long got = offset(t, a[0].trim());
				if (got == null || !got.equals(want)) {
					bad.add(where + ": Ghidra " + (got == null ? "no such member" : "0x" + Long.toHexString(got)));
				}
			}
		}
		for (String b : bad) {
			printerr("CheckLayouts: " + b);
		}
		println(String.format("CheckLayouts: %d sizes and %d offsets checked, %d wrong, %d not checked", sizes,
			offsets, bad.size(), skipped));
	}

	private static DataType strip(DataType t) {
		while (t instanceof TypeDef td) {
			t = td.getBaseDataType();
		}
		return t;
	}

	private long memberOffset;

	// the member name of c, also in its anonymous structs and unions; its offset in memberOffset
	private DataTypeComponent member(Composite c, String name) {
		for (DataTypeComponent dc : c.getDefinedComponents()) {
			if (name.equals(dc.getFieldName())) {
				memberOffset = dc.getOffset();
				return dc;
			}
			if (dc.getFieldName() == null && strip(dc.getDataType()) instanceof Composite inner) {
				DataTypeComponent found = member(inner, name);
				if (found != null) {
					memberOffset += dc.getOffset();
					return found;
				}
			}
		}
		return null;
	}

	// the offset of a member path like "a.b[2].c" in t, null if there is none
	private Long offset(DataType t, String path) {
		long off = 0;
		Matcher m = Pattern.compile("\\.?(\\w+)|\\[(\\w+)\\]").matcher(path);
		int at = 0;
		while (m.find() && m.start() == at) {
			at = m.end();
			t = strip(t);
			if (m.group(1) != null) {
				if (!(t instanceof Composite c)) {
					return null;
				}
				DataTypeComponent found = member(c, m.group(1));
				if (found == null) {
					return null;
				}
				off += memberOffset;
				t = found.getDataType();
			}
			else {
				Long i = eval(m.group(2));
				if (!(t instanceof Array arr) || i == null) {
					return null;
				}
				off += i * arr.getElementLength();
				t = arr.getDataType();
			}
		}
		return at == path.length() ? off : null;
	}

	// an integer expression of numbers, the defines of the headers, + - * / and parentheses; null for anything else
	private Long eval(String s) {
		pos = 0;
		expr = s;
		try {
			long v = sum(0);
			skipBlanks();
			return pos == expr.length() ? v : null;
		}
		catch (RuntimeException e) {
			return null;
		}
	}

	private String expr;
	private int pos;

	private void skipBlanks() {
		while (pos < expr.length() && Character.isWhitespace(expr.charAt(pos))) {
			pos++;
		}
	}

	private long sum(int depth) {
		long v = product(depth);
		for (skipBlanks(); pos < expr.length() && "+-".indexOf(expr.charAt(pos)) >= 0; skipBlanks()) {
			char op = expr.charAt(pos++);
			long r = product(depth);
			v = op == '+' ? v + r : v - r;
		}
		return v;
	}

	private long product(int depth) {
		long v = atom(depth);
		for (skipBlanks(); pos < expr.length() && "*/".indexOf(expr.charAt(pos)) >= 0; skipBlanks()) {
			char op = expr.charAt(pos++);
			long r = atom(depth);
			v = op == '*' ? v * r : v / r;
		}
		return v;
	}

	private long atom(int depth) {
		skipBlanks();
		char c = expr.charAt(pos);
		if (c == '(') {
			pos++;
			long v = sum(depth);
			skipBlanks();
			if (expr.charAt(pos++) != ')') {
				throw new IllegalArgumentException();
			}
			return v;
		}
		if (c == '-') {
			pos++;
			return -atom(depth);
		}
		int start = pos;
		while (pos < expr.length() && Character.isJavaIdentifierPart(expr.charAt(pos))) {
			pos++;
		}
		String w = expr.substring(start, pos);
		if (Character.isDigit(c)) {
			return Long.decode(w.replaceAll("[uUlL]+$", ""));
		}
		String d = defines.get(w);
		if (d == null || depth > 20) {
			throw new IllegalArgumentException();
		}
		String saved = expr;
		int savedPos = pos;
		pos = 0;
		expr = d;
		long v = sum(depth + 1);
		skipBlanks();
		if (pos != expr.length()) {
			throw new IllegalArgumentException();
		}
		expr = saved;
		pos = savedPos;
		return v;
	}
}
