// Names the virtual functions of the RTTI classes. Each <Class>::vftable label (made by the RTTI analyzer) is
// walked; an unnamed entry becomes <Class>::vfNN (NN = slot number, decimal) with the __thiscall convention. A
// function shared by several vtables (inherited, not overridden) goes to the most basic class: classes are handled
// in order of their base class count. MSVC 32 bit RTTI: vftable[-1] -> Complete Object Locator, COL+0x10 -> Class
// Hierarchy Descriptor, CHD+0x08 = number of base classes. Names a user or another tool gave are kept.
// @category Scrapland

import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

public class NameVtables extends GhidraScript {

	record Vtable(Symbol sym, int bases, int slots) {}

	Memory mem;
	MemoryBlock text;
	ScrapUtil u;

	Address ptr(Address a) throws Exception { return u.ptr(a); }

	int baseCount(Address vft) {
		try {
			Address col = ptr(vft.subtract(4));
			Address chd = ptr(col.add(0x10));
			int n = mem.getInt(chd.add(8));
			return n > 0 && n < 64 ? n : 64;
		}
		catch (Exception e) {
			return 64;
		}
	}

	int slotCount(Address vft) throws Exception {
		Data d = getDataAt(vft);
		if (d != null && d.isArray() && d.getNumComponents() > 1) {
			return d.getNumComponents();
		}
		int n = 0;
		while (true) {
			Address e = vft.add(4L * n);
			Address p = ptr(e);
			if (p == null || !text.contains(p) || (n > 0 && getSymbolAt(e) != null)) {
				return n;
			}
			n++;
		}
	}

	@Override
	protected void run() throws Exception {
		u = new ScrapUtil(this, currentProgram, monitor);
		mem = u.mem;
		text = u.text;
		List<Vtable> vts = new ArrayList<>();
		SymbolIterator it = currentProgram.getSymbolTable().getSymbols("vftable");
		while (it.hasNext()) {
			Symbol s = it.next();
			vts.add(new Vtable(s, baseCount(s.getAddress()), slotCount(s.getAddress())));
		}
		vts.sort(Comparator.comparingInt(Vtable::bases).thenComparing(v -> v.sym().getName(true)));

		int named = 0, slots = 0, created = 0;
		for (Vtable v : vts) {
			for (int i = 0; i < v.slots(); i++) {
				slots++;
				Address target = ptr(v.sym().getAddress().add(4L * i));
				Function f = getFunctionAt(target);
				if (f == null) {
					disassemble(target);
					f = createFunction(target, null);
					if (f == null) {
						continue;
					}
					created++;
				}
				if (f.getSymbol().getSource() != SourceType.DEFAULT) {
					continue;
				}
				f.getSymbol().setNameAndNamespace(String.format("vf%02d", i), v.sym().getParentNamespace(),
					SourceType.ANALYSIS);
				String cc = f.getCallingConventionName();
				if (cc == null || cc.equals("unknown") || cc.equals("default") || cc.equals("__cdecl")) {
					f.setCallingConvention("__thiscall");
				}
				named++;
			}
		}
		println(String.format("NameVtables: %d vtables, %d slots, %d functions named (%d created)", vts.size(),
			slots, named, created));
	}
}
