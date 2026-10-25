// Helpers shared by the Scrapland Ghidra scripts (all .java files of this directory form one script bundle).
// Scrap.exe is 32 bit, pointers are 4 bytes little endian.

import java.util.ArrayList;
import java.util.List;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Program;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;
import ghidra.util.task.TaskMonitor;

public class ScrapUtil {

	final Program program;
	final Memory mem;
	final MemoryBlock text;
	final TaskMonitor monitor;
	final GhidraScript script;

	ScrapUtil(GhidraScript script, Program program, TaskMonitor monitor) {
		this.script = script;
		this.program = program;
		this.mem = program.getMemory();
		this.text = mem.getBlock(".text");
		this.monitor = monitor;
	}

	// the pointer stored at a, or null if it is 0 or outside the image
	Address ptr(Address a) throws Exception {
		long v = mem.getInt(a) & 0xffffffffL;
		if (v == 0) {
			return null;
		}
		Address p = a.getNewAddress(v);
		return mem.contains(p) ? p : null;
	}

	// C string at a, or null if it is not one of 1..max characters (identifier: C identifier characters only)
	String cstr(Address a, int max, boolean identifier) {
		if (a == null) {
			return null;
		}
		StringBuilder b = new StringBuilder();
		try {
			for (int i = 0; i <= max; i++) {
				int c = mem.getByte(a.add(i)) & 0xff;
				if (c == 0) {
					return b.length() > 0 ? b.toString() : null;
				}
				boolean ok = identifier
					? (Character.isLetter(c) || c == '_' || (i > 0 && Character.isDigit(c))) && c < 128
					: (c >= 0x20 && c < 0x7f) || c == '\n' || c == '\t' || c == '\r' || c >= 0xa0;
				if (!ok) {
					return null;
				}
				b.append((char) c);
			}
		}
		catch (Exception e) {
			return null;
		}
		return null;
	}

	static byte[] le32(long v) {
		return new byte[] { (byte) v, (byte) (v >> 8), (byte) (v >> 16), (byte) (v >> 24) };
	}

	// code that uses a: its references, plus "push imm32" instructions Ghidra did not turn into a reference. Only
	// byte matches that are already an instruction count, a match inside other bytes must not become code.
	List<Address> users(Address a) throws Exception {
		List<Address> out = new ArrayList<>();
		for (Reference r : script.getReferencesTo(a)) {
			out.add(r.getFromAddress());
		}
		byte[] v = le32(a.getOffset());
		byte[] pat = { 0x68, v[0], v[1], v[2], v[3] };
		Address at = text.getStart();
		while ((at = mem.findBytes(at, text.getEnd(), pat, null, true, monitor)) != null) {
			if (!out.contains(at) && script.getInstructionAt(at) != null) {
				out.add(at);
			}
			at = at.add(1);
		}
		return out;
	}

	// 4 byte aligned places in initialized data that hold the value a (pointer tables Ghidra did not type)
	List<Address> dataRefs(Address a) throws Exception {
		List<Address> out = new ArrayList<>();
		byte[] pat = le32(a.getOffset());
		for (MemoryBlock b : mem.getBlocks()) {
			if (!b.isInitialized() || b.isExecute()) {
				continue;
			}
			Address at = b.getStart();
			while ((at = mem.findBytes(at, b.getEnd(), pat, null, true, monitor)) != null) {
				if ((at.getOffset() & 3) == 0) {
					out.add(at);
				}
				at = at.add(1);
			}
		}
		return out;
	}

	// renames unless a user or an import gave the name
	static boolean rename(Symbol sym, String name, SourceType src) throws Exception {
		if (sym.getSource() == SourceType.USER_DEFINED || sym.getSource() == SourceType.IMPORTED) {
			return false;
		}
		if (!sym.getName().equals(name)) {
			sym.setName(name, src);
		}
		return true;
	}
}
