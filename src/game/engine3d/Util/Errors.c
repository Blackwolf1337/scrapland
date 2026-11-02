/*
 * Error codes and reports of the engine3d, original source Util/Errores.cpp (include/scrap/e3dutil.h): a table of
 * 200 texts, 50 and up free for Err_Register, and the debug lines over the frame.
 */
#include "scrap/console.h"
#include "scrap/e3dutil.h"
#include "scrap/render.h"
#include "scrap/platform.h"

#define ERR_COUNT 200
#define ERR_FIRST_FREE 50
#define ERR_CUSTOM -2	/* Err_Printf: no text of its own (g_ErrCustom) */

static char str_UnknownError2[] = "Unknown error";

#define g_ErrUnknown str_UnknownError2	/* "Unknown error" */

/* a line in pixels over the frame (the original drew it with GDI on the window), color 0xRRGGBB */
void Debug_Line(int x0, int y0, int x1, int y1, uint32_t color, int width) {
	(void)width;
	if (g_Engine && g_Engine->device) {
		Render_Line2D((float)x0, (float)y0, (float)x1, (float)y1, color | 0xff000000);
	}
}

static const char str_FileNotFound[];
static const char str_UnknownError[];
static const char str_Error0[];
static const char str_OperationNotPermitted[];
static const char str_NoSuchProcess[];
static const char str_InterruptedFunctionCall[];
static const char str_InputOutputError[];
static const char str_NoSuchDeviceOrAddress[];
static const char str_ArgListTooLong[];
static const char str_ExecFormatError[];
static const char str_BadFileDescriptor[];
static const char str_NoChildProcesses[];
static const char str_ResourceTemporarilyUnavailable[];
static const char str_NotEnoughMemory[];
static const char str_PermissionDenied[];
static const char str_BadAddress[];
static const char str_ResourceDevice[];
static const char str_FileAlreadyExists[];
static const char str_ImproperLink[];
static const char str_NoSuchDevice[];
static const char str_NotADirectory[];
static const char str_IsADirectory[];
static const char str_InvalidArgument[];
static const char str_TooManyOpenFilesInSystem[];
static const char str_TooManyOpenFiles[];
static const char str_InappropriateIOControlOperation[];
static const char str_FileTooLarge[];
static const char str_NoSpaceLeftOnDevice[];
static const char str_InvalidSeek[];
static const char str_ReadOnlyFileSystem[];
static const char str_TooManyLinks[];
static const char str_BrokenPipe[];
static const char str_DomainError[];
static const char str_ResultTooLarge[];
static const char str_ResourceDeadlockAvoided[];
static const char str_FilenameTooLong[];
static const char str_NoLocksAvailable[];
static const char str_FunctionNotImplemented[];
static const char str_DirectoryNotEmpty[];
static const char str_IllegalByteSequence[];
static const char str_InvalidFileFormat[];
static const char str_FileHasZeroSize[];
static const char str_InvalidPath[];
static const char str_InvalidParameter[];
static const char *g_ErrTable[200] = {
	(void *)str_Error0, (void *)str_OperationNotPermitted, (void *)str_FileNotFound, (void *)str_NoSuchProcess,
	(void *)str_InterruptedFunctionCall, (void *)str_InputOutputError, (void *)str_NoSuchDeviceOrAddress, (void *)str_ArgListTooLong,
	(void *)str_ExecFormatError, (void *)str_BadFileDescriptor, (void *)str_NoChildProcesses, (void *)str_ResourceTemporarilyUnavailable,
	(void *)str_NotEnoughMemory, (void *)str_PermissionDenied, (void *)str_BadAddress, (void *)str_UnknownError,
	(void *)str_ResourceDevice, (void *)str_FileAlreadyExists, (void *)str_ImproperLink, (void *)str_NoSuchDevice,
	(void *)str_NotADirectory, (void *)str_IsADirectory, (void *)str_InvalidArgument, (void *)str_TooManyOpenFilesInSystem,
	(void *)str_TooManyOpenFiles, (void *)str_InappropriateIOControlOperation, (void *)str_UnknownError, (void *)str_FileTooLarge,
	(void *)str_NoSpaceLeftOnDevice, (void *)str_InvalidSeek, (void *)str_ReadOnlyFileSystem, (void *)str_TooManyLinks,
	(void *)str_BrokenPipe, (void *)str_DomainError, (void *)str_ResultTooLarge, (void *)str_UnknownError,
	(void *)str_ResourceDeadlockAvoided, (void *)str_UnknownError, (void *)str_FilenameTooLong, (void *)str_NoLocksAvailable,
	(void *)str_FunctionNotImplemented, (void *)str_DirectoryNotEmpty, (void *)str_IllegalByteSequence, (void *)str_UnknownError, 0, 0, 0, 0, 0, 0,
	(void *)str_InvalidFileFormat, (void *)str_FileHasZeroSize, (void *)str_InvalidPath, (void *)str_InvalidParameter,
};

static char g_ErrCustom[4];

const char *Err_Str(int code) {
	if ((unsigned)code < ERR_COUNT) {
		return g_ErrTable[code] ? g_ErrTable[code] : g_ErrUnknown;
	}
	return code == ERR_CUSTOM ? g_ErrCustom : g_ErrUnknown;
}

int Err_Register(const char *text) {
	for (int i = ERR_FIRST_FREE; i < ERR_COUNT; i++) {
		if (!g_ErrTable[i]) {
			g_ErrTable[i] = text;
			return i;
		}
	}
	return -1;
}

static void Err_VReport(int code, int fatal, const char *fmt, va_list ap);
void Err_Printf(int fatal, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	Err_VReport(ERR_CUSTOM, fatal, fmt, ap);
	va_end(ap);
}

void Err_Report(int code, int fatal, const char *fmt, ...) {
	va_list ap;

	va_start(ap, fmt);
	Err_VReport(code, fatal, fmt, ap);
	va_end(ap);
}

static int g_ErrLastCode;
static int g_ErrLastFatal;

/* the text of the code and fmt after it; fatal into the log, else a message box */
static void Err_VReport(int code, int fatal, const char *fmt, va_list ap) {
	char buf[0x100];

	g_ErrLastCode = code;
	g_ErrLastFatal = fatal;
	int n = Bprintf(buf, sizeof(buf), "%s", Err_Str(code));
	if (fmt) {
		VBprintf(buf + n, sizeof(buf) - n, fmt, ap);
	}
	if (fatal) {
		Con_FatalErrorf("%s", buf);
		return;
	}
	Con_Warnf("WARN: %s\n", buf);
	Plat_MessageBox(buf, "Warning", 0x11010);
}

static const char str_Error0[] = "Error 0";
static const char str_OperationNotPermitted[] = "Operation not permitted";
static const char str_NoSuchProcess[] = "No such process";
static const char str_InterruptedFunctionCall[] = "Interrupted function call";
static const char str_InputOutputError[] = "Input/output error";
static const char str_NoSuchDeviceOrAddress[] = "No such device or address";
static const char str_ArgListTooLong[] = "Arg list too long";
static const char str_ExecFormatError[] = "Exec format error";
static const char str_BadFileDescriptor[] = "Bad file descriptor";
static const char str_NoChildProcesses[] = "No child processes";
static const char str_ResourceTemporarilyUnavailable[] = "Resource temporarily unavailable";
static const char str_NotEnoughMemory[] = "Not enough memory";
static const char str_PermissionDenied[] = "Permission denied";
static const char str_BadAddress[] = "Bad address";
static const char str_ResourceDevice[] = "Resource device";
static const char str_FileAlreadyExists[] = "File already exists";
static const char str_ImproperLink[] = "Improper link";
static const char str_NoSuchDevice[] = "No such device";
static const char str_NotADirectory[] = "Not a directory";
static const char str_IsADirectory[] = "Is a directory";
static const char str_InvalidArgument[] = "Invalid argument";
static const char str_TooManyOpenFilesInSystem[] = "Too many open files in system";
static const char str_TooManyOpenFiles[] = "Too many open files";
static const char str_InappropriateIOControlOperation[] = "Inappropriate I/O control operation";
static const char str_FileTooLarge[] = "File too large";
static const char str_NoSpaceLeftOnDevice[] = "No space left on device";
static const char str_InvalidSeek[] = "Invalid seek";
static const char str_ReadOnlyFileSystem[] = "Read-only file system";
static const char str_TooManyLinks[] = "Too many links";
static const char str_BrokenPipe[] = "Broken pipe";
static const char str_DomainError[] = "Domain error";
static const char str_ResultTooLarge[] = "Result too large";
static const char str_ResourceDeadlockAvoided[] = "Resource deadlock avoided";
static const char str_FilenameTooLong[] = "Filename too long";
static const char str_NoLocksAvailable[] = "No locks available";
static const char str_FunctionNotImplemented[] = "Function not implemented";
static const char str_DirectoryNotEmpty[] = "Directory not empty";
static const char str_IllegalByteSequence[] = "Illegal byte sequence";
static const char str_InvalidFileFormat[] = "Invalid file format";
static const char str_FileHasZeroSize[] = "File has zero size";
static const char str_InvalidPath[] = "Invalid path";
static const char str_InvalidParameter[] = "Invalid parameter";
static const char str_FileNotFound[] = "File not found";
static const char str_UnknownError[] = "Unknown error";

