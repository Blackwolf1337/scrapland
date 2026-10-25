/* Stand-in for the Python 1.5.2 config.h when Ghidra parses third_party/python-1.5.2/Include: only what the
   headers need to describe the layout of the MSVC 32 bit build in Scrap.exe (int, long and pointers 4 bytes). */
#define HAVE_PROTOTYPES 1
#define HAVE_STDARG_PROTOTYPES 1
#define SIZEOF_INT 4
#define SIZEOF_LONG 4
#define SIZEOF_VOID_P 4
#define DL_IMPORT(RTYPE) RTYPE
#define DL_EXPORT(RTYPE) RTYPE
