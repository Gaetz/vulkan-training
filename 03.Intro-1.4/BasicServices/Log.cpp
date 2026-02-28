#include "Log.h"
#include "Platform.h"
#include "FileWriter.h"
#include <cstdarg>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <SDL3/SDL_log.h>

using services::FileWriter;
using services::Platform;
using services::ConsoleColor;

// Global log file
static FileWriter logFile;

// Custom SDL log output function with colored output
static void CustomLogOutput(void* userdata, int category, SDL_LogPriority priority, const char* message) {
    // Get current time
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    std::tm now_tm;
#ifdef _WIN32
    localtime_s(&now_tm, &now_time_t);
#else
    localtime_r(&now_time_t, &now_tm);
#endif

    // Map SDL priority to console color and severity string
    ConsoleColor color = ConsoleColor::Reset;
    const char* severityStr = "INFO";
    switch(priority) {
        case SDL_LOG_PRIORITY_VERBOSE:
            color = ConsoleColor::Gray;
            severityStr = "TRACE";
            break;
        case SDL_LOG_PRIORITY_DEBUG:
            color = ConsoleColor::Cyan;
            severityStr = "DEBUG";
            break;
        case SDL_LOG_PRIORITY_INFO:
            color = ConsoleColor::Green;
            severityStr = "INFO";
            break;
        case SDL_LOG_PRIORITY_WARN:
            color = ConsoleColor::Yellow;
            severityStr = "WARN";
            break;
        case SDL_LOG_PRIORITY_ERROR:
            color = ConsoleColor::Red;
            severityStr = "ERROR";
            break;
        case SDL_LOG_PRIORITY_CRITICAL:
            color = ConsoleColor::BoldRed;
            severityStr = "CRITICAL";
            break;
        default: break;
    }

    // Output the message (to stderr for errors/critical, stdout otherwise)
    FILE* output = (priority >= SDL_LOG_PRIORITY_ERROR) ? stderr : stdout;

    // Print timestamp and severity in white
    Platform::setConsoleColor(ConsoleColor::Reset);
    fprintf(output, "[%02d:%02d:%02d.%03d %s] ",
            now_tm.tm_hour, now_tm.tm_min, now_tm.tm_sec, (int)now_ms.count(), severityStr);

    // Print message in severity color
    Platform::setConsoleColor(color);
    fprintf(output, "%s", message);

    // Reset color and newline
    Platform::setConsoleColor(ConsoleColor::Reset);
    fprintf(output, "\n");
    fflush(output);

    // Also write to log file (without colors)
    if (logFile.isOpen()) {
        std::ostringstream oss;
        oss << "["
            << std::setfill('0') << std::setw(2) << now_tm.tm_hour << ":"
            << std::setfill('0') << std::setw(2) << now_tm.tm_min << ":"
            << std::setfill('0') << std::setw(2) << now_tm.tm_sec << "."
            << std::setfill('0') << std::setw(3) << (int)now_ms.count() << " "
            << severityStr << "] " << message;
        logFile.writeLine(oss.str());
    }
}

// Static initializer to set up custom log output
class LogInitializer {
public:
    LogInitializer() {
        // Open log file
        logFile.open("LastRun.log");
        if (logFile.isOpen()) {
            logFile.writeLine("=== Log Started ===");
        }
#if defined(NDEBUG)
        SDL_SetLogPriorities(SDL_LOG_PRIORITY_WARN);
#else
        SDL_SetLogPriorities(SDL_LOG_PRIORITY_DEBUG);
#endif
        SDL_SetLogOutputFunction(CustomLogOutput, nullptr);
    }

    ~LogInitializer() {
        if (logFile.isOpen()) {
            logFile.writeLine("=== Log Ended ===");
            logFile.close();
        }
    }
};

static LogInitializer logInit; // Runs before main()

namespace services {

void Log::Trace(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_VERBOSE, fmt, args);
    va_end(args);
}

void Log::Debug(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG, fmt, args);
    va_end(args);
}

void Log::Info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO, fmt, args);
    va_end(args);
}

void Log::Warn(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_WARN, fmt, args);
    va_end(args);
}

void Log::Error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR, fmt, args);
    va_end(args);
}

void Log::Critical(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    SDL_LogMessageV(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_CRITICAL, fmt, args);
    va_end(args);
}

} // namespace services
