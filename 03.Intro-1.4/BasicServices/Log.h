#pragma once

namespace services {

class Log {
public:
    // Prevent instantiation
    Log() = delete;
    ~Log() = delete;
    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;

    // Logging methods
    static void Trace(const char* fmt, ...);
    static void Debug(const char* fmt, ...);
    static void Info(const char* fmt, ...);
    static void Warn(const char* fmt, ...);
    static void Error(const char* fmt, ...);
    static void Critical(const char* fmt, ...);
};

} // namespace services
