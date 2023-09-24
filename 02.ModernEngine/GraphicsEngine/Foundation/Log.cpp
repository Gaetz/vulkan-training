#include "Log.hpp"

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#include <stdio.h>
#include <stdarg.h>

LogService logService;

static constexpr u32 stringBufferSize = 1024 * 1024;
static char logBuffer[stringBufferSize];

static void OutputConsole(char* logBuffer_) {
    printf("%s", logBuffer_);
}

#if defined(_MSC_VER)
    static void OutputVisualStudio(char* logBuffer_) {
        OutputDebugStringA(logBuffer_);
    }
#endif

LogService* LogService::Instance() {
    return &logService;
}

void LogService::PrintFormat(cstring format, ...) {
    va_list args;

    va_start(args, format);
#if defined(_MSC_VER)
    vsnprintf_s(logBuffer, ArraySize(logBuffer), format, args);
#else
     vsnprintf(logBuffer, ArraySize(logBuffer), format, args);
#endif
    logBuffer[ArraySize(logBuffer) - 1] = '\0';
    va_end(args);

    OutputConsole(logBuffer);
#if defined(_MSC_VER)
    OutputVisualStudio(logBuffer);
#endif // _MSC_VER

    if (printCallback)
        printCallback(logBuffer);
}

void LogService::SetCallback(PrintCallback callback) {
    printCallback = callback;
}

