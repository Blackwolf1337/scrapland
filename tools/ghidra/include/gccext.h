/*
 * Ghidra parse stub, included first by scrap_all.h: the GCC and C23 parts of include/scrap that Ghidra's C parser
 * does not know. They carry no layout (the layouts are plain C and #pragma pack), so they go away.
 */
#define static_assert(e, m)
#define typeof(x) x
#define bool _Bool
#define true 1
#define false 0
/* Ghidra's parser has no __thiscall. Nothing in the headers uses __vectorcall, so it carries the convention through
 * the parse and ImportHeaders -C__vectorcall=__thiscall sets the right one afterwards. */
#define __thiscall __vectorcall
