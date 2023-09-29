#include "Process.hpp"
#include "Assert.hpp"
#include "Log.hpp"

#include "Memory.hpp"
#include "String.hpp"

#include <stdio.h>

#if defined(_WIN64)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#endif

// Static buffer to log the error coming from windows.
static const u32    processLogBuffer = 256;
char                sProcessLogBuffer[processLogBuffer];
static char         processOutputBuffer[1025];

#if defined(_WIN64)


void Win32GetError(char* buffer, u32 size) {
    DWORD errorCode = GetLastError();

    char* error_string;
    if (!FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER,
        NULL, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&error_string, 0, NULL))
        return;

    sprintf_s(buffer, size, "%s", error_string);

    LocalFree(error_string);
}

bool ProcessExecute(cstring workingDirectory, cstring processFullpath, cstring arguments, cstring searchErrorString) {
    // From the post in https://stackoverflow.com/questions/35969730/how-to-read-output-from-cmd-exe-using-createprocess-and-createpipe/55718264#55718264
    // Create pipes for redirecting output
    HANDLE handleStdinPipeRead = NULL;
    HANDLE handleStdinPipeWrite = NULL;
    HANDLE handleStdoutPipeRead = NULL;
    HANDLE handleStdPipeWrite = NULL;

    SECURITY_ATTRIBUTES securityAttributes = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    BOOL ok = CreatePipe(&handleStdinPipeRead, &handleStdinPipeWrite, &securityAttributes, 0);
    if (ok == FALSE)
        return false;
    ok = CreatePipe(&handleStdoutPipeRead, &handleStdPipeWrite, &securityAttributes, 0);
    if (ok == FALSE)
        return false;

    // Create startup informations with std redirection
    STARTUPINFOA startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    startupInfo.hStdInput = handleStdinPipeRead;
    startupInfo.hStdError = handleStdPipeWrite;
    startupInfo.hStdOutput = handleStdPipeWrite;
    startupInfo.wShowWindow = SW_SHOW;

    bool execution_success = false;
    // Execute the process
    PROCESS_INFORMATION processInfo = {};
    BOOL inherit_handles = TRUE;
    if (CreateProcessA(processFullpath, (char*)arguments, 0, 0, inherit_handles, 0, 0, workingDirectory, &startupInfo, &processInfo)) {

        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);

        execution_success = true;
    }
    else {
        Win32GetError(&sProcessLogBuffer[0], processLogBuffer);

        GPrint("Execute process error.\n Exe: \"%s\" - Args: \"%s\" - Work_dir: \"%s\"\n", processFullpath, arguments, workingDirectory);
        GPrint("Message: %s\n", sProcessLogBuffer);
    }
    CloseHandle(handleStdinPipeRead);
    CloseHandle(handleStdPipeWrite);

    // Output
    DWORD bytes_read;
    ok = ReadFile(handleStdoutPipeRead, processOutputBuffer, 1024, &bytes_read, nullptr);

    // Consume all outputs.
    // Terminate current read and initialize the next.
    while (ok == TRUE) {
        processOutputBuffer[bytes_read] = 0;
        GPrint("%s", processOutputBuffer);

        ok = ReadFile(handleStdoutPipeRead, processOutputBuffer, 1024, &bytes_read, nullptr);
    }

    if (strlen(searchErrorString) > 0 && strstr(processOutputBuffer, searchErrorString)) {
        execution_success = false;
    }

    GPrint("\n");

    // Close handles.
    CloseHandle(handleStdoutPipeRead);
    CloseHandle(handleStdinPipeWrite);

    DWORD process_exit_code = 0;
    GetExitCodeProcess(processInfo.hProcess, &process_exit_code);

    return execution_success;
}

cstring ProcessGetOutput() {
    return processOutputBuffer;
}

#else

bool ProcessExecute(cstring workingDirectory, cstring processFullpath, cstring arguments, cstring searchErrorString) {
    char current_dir[4096];
    getcwd(current_dir, 4096);

    int result = chdir(workingDirectory);
    GASSERT(result == 0);

    Allocator* allocator = &MemoryService::instance()->system_allocator;

    sizet full_cmd_size = strlen(processFullpath) + 1 + strlen(arguments) + 1;
    StringBuffer full_cmd_buffer;
    full_cmd_buffer.Init(full_cmd_size, allocator);

    char* full_cmd = full_cmd_buffer.append_use_f("%s %s", processFullpath, arguments);

    // TODO(marco): this works only if one process is started at a time
    FILE* cmd_stream = popen(full_cmd, "r");
    bool execute_success = false;
    if (cmd_stream != NULL) {
        result = wait(NULL);

        sizet read_chunk_size = 1024;
        if (result != -1) {
            sizet bytes_read = fread(processOutputBuffer, 1, read_chunk_size, cmd_stream);
            while (bytes_read == read_chunk_size) {
                processOutputBuffer[bytes_read] = 0;
                GPrint("%s", processOutputBuffer);

                bytes_read = fread(processOutputBuffer, 1, read_chunk_size, cmd_stream);
            }

            processOutputBuffer[bytes_read] = 0;
            GPrint("%s", processOutputBuffer);

            if (strlen(searchErrorString) > 0 && strstr(processOutputBuffer, searchErrorString)) {
                execute_success = false;
            }

            GPrint("\n");
        }
        else {
            int err = errno;

            GPrint("Execute process error.\n Exe: \"%s\" - Args: \"%s\" - Work_dir: \"%s\"\n", processFullpath, arguments, workingDirectory);
            GPrint("Error: %d\n", err);

            execute_success = false;
        }

        pclose(cmd_stream);
    }
    else {
        int err = errno;

        GPrint("Execute process error.\n Exe: \"%s\" - Args: \"%s\" - Work_dir: \"%s\"\n", processFullpath, arguments, workingDirectory);
        GPrint("Error: %d\n", err);
        execute_success = false;
    }

    chdir(current_dir);

    full_cmd_buffer.shutdown();

    return execute_success;
}

cstring ProcessGetOutput() {
    return processOutputBuffer;
}

#endif // WIN64

