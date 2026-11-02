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

char str_UnknownError2[] = "Unknown error";

#define g_ErrUnknown str_UnknownError2	/* "Unknown error" */

/* a line in pixels over the frame (the original drew it with GDI on the window), color 0xRRGGBB */
void Debug_Line(int x0, int y0, int x1, int y1, uint32_t color, int width) {
	(void)width;
	if (g_Engine && g_Engine->device) {
		Render_Line2D((float)x0, (float)y0, (float)x1, (float)y1, color | 0xff000000);
	}
}

extern const char str_FileNotFound[];
extern const char str_UnknownError[];
extern const char str_Error0[];
extern const char str_OperationNotPermitted[];
extern const char str_NoSuchProcess[];
extern const char str_InterruptedFunctionCall[];
extern const char str_InputOutputError[];
extern const char str_NoSuchDeviceOrAddress[];
extern const char str_ArgListTooLong[];
extern const char str_ExecFormatError[];
extern const char str_BadFileDescriptor[];
extern const char str_NoChildProcesses[];
extern const char str_ResourceTemporarilyUnavailable[];
extern const char str_NotEnoughMemory[];
extern const char str_PermissionDenied[];
extern const char str_BadAddress[];
extern const char str_ResourceDevice[];
extern const char str_FileAlreadyExists[];
extern const char str_ImproperLink[];
extern const char str_NoSuchDevice[];
extern const char str_NotADirectory[];
extern const char str_IsADirectory[];
extern const char str_InvalidArgument[];
extern const char str_TooManyOpenFilesInSystem[];
extern const char str_TooManyOpenFiles[];
extern const char str_InappropriateIOControlOperation[];
extern const char str_FileTooLarge[];
extern const char str_NoSpaceLeftOnDevice[];
extern const char str_InvalidSeek[];
extern const char str_ReadOnlyFileSystem[];
extern const char str_TooManyLinks[];
extern const char str_BrokenPipe[];
extern const char str_DomainError[];
extern const char str_ResultTooLarge[];
extern const char str_ResourceDeadlockAvoided[];
extern const char str_FilenameTooLong[];
extern const char str_NoLocksAvailable[];
extern const char str_FunctionNotImplemented[];
extern const char str_DirectoryNotEmpty[];
extern const char str_IllegalByteSequence[];
extern const char str_InvalidFileFormat[];
extern const char str_FileHasZeroSize[];
extern const char str_InvalidPath[];
extern const char str_InvalidParameter[];
const char *g_ErrTable[200] = {
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

const char *Err_Unregister(int code) {
	if ((unsigned)(code - ERR_FIRST_FREE) < ERR_COUNT - ERR_FIRST_FREE) {
		const char *s = g_ErrTable[code];
		g_ErrTable[code] = NULL;
		return s;
	}
	return NULL;
}

char g_ErrCustom[4];

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

int g_ErrLastCode;
int g_ErrLastFatal;

/* the text of the code and fmt after it; fatal into the log, else a message box */
void Err_VReport(int code, int fatal, const char *fmt, va_list ap) {
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

const char str_Error0[] = "Error 0";
const char str_OperationNotPermitted[] = "Operation not permitted";
const char str_NoSuchProcess[] = "No such process";
const char str_InterruptedFunctionCall[] = "Interrupted function call";
const char str_InputOutputError[] = "Input/output error";
const char str_NoSuchDeviceOrAddress[] = "No such device or address";
const char str_ArgListTooLong[] = "Arg list too long";
const char str_ExecFormatError[] = "Exec format error";
const char str_BadFileDescriptor[] = "Bad file descriptor";
const char str_NoChildProcesses[] = "No child processes";
const char str_ResourceTemporarilyUnavailable[] = "Resource temporarily unavailable";
const char str_NotEnoughMemory[] = "Not enough memory";
const char str_PermissionDenied[] = "Permission denied";
const char str_BadAddress[] = "Bad address";
const char str_ResourceDevice[] = "Resource device";
const char str_FileAlreadyExists[] = "File already exists";
const char str_ImproperLink[] = "Improper link";
const char str_NoSuchDevice[] = "No such device";
const char str_NotADirectory[] = "Not a directory";
const char str_IsADirectory[] = "Is a directory";
const char str_InvalidArgument[] = "Invalid argument";
const char str_TooManyOpenFilesInSystem[] = "Too many open files in system";
const char str_TooManyOpenFiles[] = "Too many open files";
const char str_InappropriateIOControlOperation[] = "Inappropriate I/O control operation";
const char str_FileTooLarge[] = "File too large";
const char str_NoSpaceLeftOnDevice[] = "No space left on device";
const char str_InvalidSeek[] = "Invalid seek";
const char str_ReadOnlyFileSystem[] = "Read-only file system";
const char str_TooManyLinks[] = "Too many links";
const char str_BrokenPipe[] = "Broken pipe";
const char str_DomainError[] = "Domain error";
const char str_ResultTooLarge[] = "Result too large";
const char str_ResourceDeadlockAvoided[] = "Resource deadlock avoided";
const char str_FilenameTooLong[] = "Filename too long";
const char str_NoLocksAvailable[] = "No locks available";
const char str_FunctionNotImplemented[] = "Function not implemented";
const char str_DirectoryNotEmpty[] = "Directory not empty";
const char str_IllegalByteSequence[] = "Illegal byte sequence";
const char str_InvalidFileFormat[] = "Invalid file format";
const char str_FileHasZeroSize[] = "File has zero size";
const char str_InvalidPath[] = "Invalid path";
const char str_InvalidParameter[] = "Invalid parameter";
const char str_FileNotFound[] = "File not found";
const char str_UnknownError[] = "Unknown error";

