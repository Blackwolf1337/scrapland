/*
 * Python/import.c with the change of the engine in Scrap.exe: once the startup imports are done (g_PyPackImport,
 * set by Python_Init and Pack_PreloadLibrary), reading or writing a .pyc warns on stderr that the module was not
 * preloaded from the script pack. The exe prints the read warning first thing in read_compiled_module, the write
 * warning last in write_compiled_module (after "# wrote" of -v); the macros below put them there without changing
 * the vendored file: the read through its PyMarshal_ReadObjectFromFile call, the write through the fflush of that
 * function, which is the only one of the file, and the fclose or the -v message that follow it.
 */
#include "Python.h"
#include "marshal.h"

/* rva 0x4e79cc */
int g_PyPackImport;

/* the .pyc that write_compiled_module wrote, until its warning is out */
static const char *pack_wrote;

static void pack_read(const char *cpathname) {
	if (g_PyPackImport)
		PySys_WriteStderr("WARNING: import %s (read)\n the library was not preloaded\n", cpathname);
}

static void pack_wrote_warn(void) {
	const char *cpathname = pack_wrote;

	pack_wrote = NULL;
	if (g_PyPackImport)
		PySys_WriteStderr("WARNING: import %s (wrote)\n the library was not preloaded\n", cpathname);
}

static int pack_fflush(FILE *fp, const char *cpathname) {
	pack_wrote = cpathname;
	return crt_fflush(fp);
}

/* without -v the warning follows the fclose, with -v the "# wrote" message after it */
static int pack_fclose(FILE *fp) {
	int r = crt_fclose(fp);

	if (pack_wrote && !Py_VerboseFlag)
		pack_wrote_warn();
	return r;
}

#define PyMarshal_ReadObjectFromFile(fp) (pack_read(cpathname), PyMarshal_ReadObjectFromFile(fp))
#define crt_fflush(fp) pack_fflush(fp, cpathname)
#define crt_fclose_file(fp) pack_fclose(fp)
#define PySys_WriteStderr(args...) (PySys_WriteStderr(args), pack_wrote ? pack_wrote_warn() : (void)0)

#include "Python/import.c"
