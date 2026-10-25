// Names the C functions behind the Python 1.5.2 modules (engine modules like SAct, SWeap and the Python built in
// ones). Scans the initialized data for PyMethodDef tables, 16 bytes per entry on 32 bit:
//   { char *ml_name; PyCFunction ml_meth; int ml_flags; char *ml_doc; }, ended by an entry with ml_name 0.
// The module name comes from the Py_InitModule4(name, methods, doc, self, 1007) call that pushes the table. Each
// function becomes <module>_<method> (CPython style, "__builtin__" gives "builtin"), its doc string the plate
// comment, the table <module>_methods typed as PyMethodDef[n]. Names a user gave are kept. When the Python headers
// are imported (ImportHeaders.java), their PyMethodDef and PyTypeObject are used and the functions get the
// PyCFunction signature.
// @category Scrapland

import java.util.ArrayList;
import java.util.List;

import ghidra.app.cmd.function.ApplyFunctionSignatureCmd;
import ghidra.app.cmd.function.FunctionRenameOption;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.data.ArrayDataType;
import ghidra.program.model.data.CategoryPath;
import ghidra.program.model.data.CharDataType;
import ghidra.program.model.data.DataType;
import ghidra.program.model.data.DataTypeConflictHandler;
import ghidra.program.model.data.DataUtilities;
import ghidra.program.model.data.FunctionDefinitionDataType;
import ghidra.program.model.data.ParameterDefinition;
import ghidra.program.model.data.ParameterDefinitionImpl;
import ghidra.program.model.data.IntegerDataType;
import ghidra.program.model.data.PointerDataType;
import ghidra.program.model.data.StructureDataType;
import ghidra.program.model.data.VoidDataType;
import ghidra.program.model.listing.CommentType;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;

public class NamePyBindings extends GhidraScript {

	Memory mem;
	MemoryBlock text;
	ScrapUtil u;
	List<String> inittabNames = new ArrayList<>();

	Address ptr(Address a) throws Exception { return u.ptr(a); }
	String cstr(Address a, int max, boolean identifier) { return u.cstr(a, max, identifier); }
	List<Address> users(Address a) throws Exception { return u.users(a); }
	List<Address> dataRefs(Address a) throws Exception { return u.dataRefs(a); }
	boolean rename(Symbol s, String name, SourceType src) throws Exception { return ScrapUtil.rename(s, name, src); }

	boolean validEntry(Address e) throws Exception {
		Address name = ptr(e), meth = ptr(e.add(4)), doc = ptr(e.add(12));
		int flags = mem.getInt(e.add(8));
		if (name == null || meth == null || !text.contains(meth) || flags < 0 || flags > 3) {
			return false;
		}
		// the doc pointer only has to point into the image: SAI has an entry whose doc points to binary data
		if (mem.getInt(e.add(12)) != 0 && doc == null) {
			return false;
		}
		return cstr(name, 64, true) != null;
	}

	// module name from the instructions around the one that pushes the table: push name follows push methods
	String moduleName(Address table, List<Address> initCalls) throws Exception {
		for (Address from : users(table)) {
			Instruction in = getInstructionAt(from);
			for (int i = 0; in != null && i < 4; i++) {
				in = in.getNext();
				if (in == null) {
					break;
				}
				if (in.getFlowType().isCall()) {
					Address[] flows = in.getFlows();
					if (flows.length == 1) {
						initCalls.add(flows[0]);
					}
					break;
				}
				List<Address> targets = new ArrayList<>();
				for (Reference rr : in.getReferencesFrom()) {
					targets.add(rr.getToAddress());
				}
				for (int op = 0; op < in.getNumOperands(); op++) {
					ghidra.program.model.scalar.Scalar sc = in.getScalar(op);
					if (sc != null && sc.getUnsignedValue() > 0x400000L) {
						targets.add(toAddr(sc.getUnsignedValue()));
					}
				}
				for (Address target : targets) {
					String s = cstr(mem.contains(target) ? target : null, 64, true);
					if (s != null) {
						// keep looking for the call target
						Instruction c = in;
						for (int k = 0; c != null && k < 4; k++, c = c.getNext()) {
							if (c.getFlowType().isCall() && c.getFlows().length == 1) {
								initCalls.add(c.getFlows()[0]);
								break;
							}
						}
						return s;
					}
				}
			}
		}
		return null;
	}

	// method table of an object type: the function using it (through Py_FindMethod) is the tp_getattr of a
	// PyTypeObject. 1.5.2 layout on 32 bit: tp_name at +0x0c, tp_getattr at +0x20.
	String typeName(Address table) throws Exception {
		List<Function> cands = new ArrayList<>();
		for (Address from : users(table)) {
			Function f = getFunctionContaining(from);
			if (f != null && !cands.contains(f)) {
				cands.add(f);
				// some getattr functions reach the table through a helper (cPyEntity)
				for (Function c : f.getCallingFunctions(monitor)) {
					if (!cands.contains(c)) {
						cands.add(c);
					}
				}
			}
		}
		for (Function f : cands) {
			for (Address from : dataRefs(f.getEntryPoint())) {
				Address type = from.subtract(0x20);
				String name = cstr(ptr(type.add(0x0c)), 64, false);
				if (name != null && mem.getInt(type.add(0x10)) > 0 && mem.getInt(type.add(0x10)) < 0x10000) {
					name = name.replaceAll("[^A-Za-z0-9_]+", "_").replaceAll("^_+|_+$", "");
					if (getSymbolAt(type) == null || getSymbolAt(type).getSource() == SourceType.DEFAULT) {
						createLabel(type, name + "_Type", true, SourceType.ANALYSIS);
					}
					DataType t = headerType("PyTypeObject");
					if (t != null) {
						try {
							// tp_name and tp_basicsize matched, so data the analysis guessed here gives way
							DataUtilities.createData(currentProgram, type, t, -1,
								DataUtilities.ClearDataMode.CLEAR_ALL_CONFLICT_DATA);
						}
						catch (Exception ex) {
							printerr("NamePyBindings: PyTypeObject at " + type + ": " + ex.getMessage());
						}
					}
					return name;
				}
			}
		}
		return null;
	}

	// _PyImport_Inittab: { char *name; void (*initfunc)(void); } pairs ended by { 0, 0 }. Found through the
	// entry of a module name we already know.
	int nameInittab(List<String> modules) throws Exception {
		int n = 0;
		for (MemoryBlock b : mem.getBlocks()) {
			if (!b.isInitialized() || b.isExecute()) {
				continue;
			}
			for (Address a = b.getStart(); a.add(8).compareTo(b.getEnd()) < 0; a = a.add(4)) {
				String name = cstr(ptr(a), 64, true);
				Address fn = ptr(a.add(4));
				if (name == null || fn == null || !text.contains(fn) || !modules.contains(name)) {
					continue;
				}
				// walk back to the start of the table, then forward to its end
				Address start = a;
				while (cstr(ptr(start.subtract(8)), 64, true) != null && ptr(start.subtract(4)) != null &&
					text.contains(ptr(start.subtract(4)))) {
					start = start.subtract(8);
				}
				Address e = start;
				for (; cstr(ptr(e), 64, true) != null && ptr(e.add(4)) != null && text.contains(ptr(e.add(4)));
						e = e.add(8)) {
					Function f = getFunctionAt(ptr(e.add(4)));
					if (f == null) {
						disassemble(ptr(e.add(4)));
						f = createFunction(ptr(e.add(4)), null);
					}
					inittabNames.add(cstr(ptr(e), 64, true));
					if (f != null && rename(f.getSymbol(), "init" + cstr(ptr(e), 64, true), SourceType.ANALYSIS)) {
						n++;
					}
				}
				if (getSymbolAt(start) == null || getSymbolAt(start).getSource() == SourceType.DEFAULT) {
					createLabel(start, "_PyImport_Inittab", true, SourceType.ANALYSIS);
				}
				println("NamePyBindings: _PyImport_Inittab at " + start + ", " + ((e.subtract(start)) / 8) +
					" modules");
				return n;
			}
		}
		return n;
	}

	// a type parsed from the Python headers (ImportHeaders.java), or null
	DataType headerType(String name) {
		List<DataType> found = new ArrayList<>();
		currentProgram.getDataTypeManager().findDataTypes(name, found);
		for (DataType t : found) {
			if (t.getCategoryPath().getPath().endsWith(".h")) { // ImportHeaders files types by header name
				return t;
			}
		}
		return null;
	}

	// PyCFunction: PyObject *f(PyObject *self, PyObject *args), with METH_KEYWORDS also PyObject *kw
	void applyCFunction(Function f, boolean keywords) {
		DataType obj = headerType("PyObject");
		if (obj == null) {
			return;
		}
		DataType p = new PointerDataType(obj);
		FunctionDefinitionDataType def = new FunctionDefinitionDataType("PyCFunction");
		def.setReturnType(p);
		List<ParameterDefinition> params = new ArrayList<>();
		params.add(new ParameterDefinitionImpl("self", p, null));
		params.add(new ParameterDefinitionImpl("args", p, null));
		if (keywords) {
			params.add(new ParameterDefinitionImpl("kw", p, null));
		}
		def.setArguments(params.toArray(new ParameterDefinition[0]));
		try {
			def.setCallingConvention("__cdecl");
		}
		catch (Exception e) {
			// keep the default convention
		}
		new ApplyFunctionSignatureCmd(f.getEntryPoint(), def, SourceType.ANALYSIS, true, false, null,
			FunctionRenameOption.NO_CHANGE).applyTo(currentProgram);
	}

	DataType methodDefType() {
		DataType h = headerType("PyMethodDef");
		if (h != null) {
			return h;
		}
		CategoryPath cat = new CategoryPath("/Scrap/Python");
		StructureDataType s = new StructureDataType(cat, "PyMethodDef", 0);
		s.add(new PointerDataType(CharDataType.dataType), 4, "ml_name", null);
		s.add(new PointerDataType(VoidDataType.dataType), 4, "ml_meth", "PyCFunction");
		s.add(IntegerDataType.dataType, 4, "ml_flags", "METH_VARARGS 1, METH_KEYWORDS 2");
		s.add(new PointerDataType(CharDataType.dataType), 4, "ml_doc", null);
		return currentProgram.getDataTypeManager().addDataType(s, DataTypeConflictHandler.REPLACE_HANDLER);
	}

	@Override
	protected void run() throws Exception {
		u = new ScrapUtil(this, currentProgram, monitor);
		mem = u.mem;
		text = u.text;
		DataType methodDef = methodDefType();
		List<Address> initCalls = new ArrayList<>();
		List<String> modules = new ArrayList<>();
		int ntables = 0, nfuncs = 0, unnamed = 0;
		// first pass over _PyImport_Inittab so the init functions are code before the tables are read
		inittabNames.clear();
		nameInittab(List.of("sys", "SInput", "Scrap", "imp", "marshal"));
		modules.addAll(inittabNames);

		for (MemoryBlock b : mem.getBlocks()) {
			if (!b.isInitialized() || b.isExecute() || b.getName().equals(".rsrc") || b.getName().equals(".reloc")) {
				continue;
			}
			Address a = b.getStart();
			while (a.add(32).compareTo(b.getEnd()) < 0) {
				int n = 0;
				while (a.add(16L * n + 16).compareTo(b.getEnd()) < 0 && validEntry(a.add(16L * n))) {
					n++;
				}
				if (n == 0 || mem.getInt(a.add(16L * n)) != 0 || (n < 2 && getReferencesTo(a).length == 0)) {
					a = a.add(4);
					continue;
				}
				String mod = moduleName(a, initCalls);
				boolean isType = false;
				if (mod == null) {
					mod = typeName(a);
					isType = mod != null;
				}
				if (mod == null) {
					Function f = null;
					for (Reference r : getReferencesTo(a)) {
						f = getFunctionContaining(r.getFromAddress());
					}
					mod = f != null ? f.getName() : "pymod_" + a.toString();
					unnamed++;
				}
				String prefix = mod.replaceAll("^_+|_+$", "");
				if (isType && modules.contains(mod)) {
					prefix += "obj";
				}
				if (!isType) {
					modules.add(mod);
				}
				ntables++;

				Symbol ts = getSymbolAt(a);
				if (ts == null) {
					createLabel(a, prefix + "_methods", true, SourceType.ANALYSIS);
				}
				else {
					rename(ts, prefix + "_methods", SourceType.ANALYSIS);
				}
				DataUtilities.createData(currentProgram, a, new ArrayDataType(methodDef, n + 1, 16), -1,
					DataUtilities.ClearDataMode.CLEAR_ALL_CONFLICT_DATA);

				for (int i = 0; i < n; i++) {
					Address e = a.add(16L * i);
					String name = cstr(ptr(e), 64, true);
					Address meth = ptr(e.add(4));
					Function f = getFunctionAt(meth);
					if (f == null) {
						disassemble(meth);
						f = createFunction(meth, null);
					}
					if (f == null) {
						printerr("NamePyBindings: no function at " + meth + " for " + mod + "." + name);
						continue;
					}
					if (rename(f.getSymbol(), prefix + "_" + name, SourceType.ANALYSIS)) {
						applyCFunction(f, (mem.getInt(e.add(8)) & 2) != 0);
						nfuncs++;
					}
					String doc = cstr(ptr(e.add(12)), 4096, false);
					if (doc != null && getPlateComment(meth) == null) {
						currentProgram.getListing().setComment(meth, CommentType.PLATE,
							mod + "." + name + "\n" + doc.trim());
					}
				}
				a = a.add(16L * (n + 1));
			}
		}

		// the function every table goes through is Py_InitModule4
		Address init = null;
		int votes = 0;
		for (Address c : initCalls) {
			int v = 0;
			for (Address d : initCalls) {
				if (d.equals(c)) v++;
			}
			if (v > votes) {
				votes = v;
				init = c;
			}
		}
		if (init != null && votes >= 3 && getFunctionAt(init) != null) {
			rename(getFunctionAt(init).getSymbol(), "Py_InitModule4", SourceType.ANALYSIS);
			println("NamePyBindings: Py_InitModule4 at " + init + " (" + votes + " calls)");
		}
		int ninit = nameInittab(modules);
		println(String.format("NamePyBindings: %d module init functions named", ninit));
		println(String.format("NamePyBindings: %d tables, %d functions named, %d tables without module name",
			ntables, nfuncs, unnamed));
	}
}
