#pragma once

#include "Platform.hpp"
#include "Service.hpp"

typedef void (*PrintCallback)(const char*);  // Additional callback for printing

struct LogService : public Service {

    G_DECLARE_SERVICE(LogService);

    void PrintFormat(cstring format, ...);
    void SetCallback(PrintCallback callback);

    PrintCallback printCallback = nullptr;

    static constexpr cstring kName = "log_service";
};

#if defined(_MSC_VER)
#define GPrint(format, ...)          LogService::Instance()->PrintFormat(format, __VA_ARGS__);
#define GPrintRet(format, ...)       LogService::Instance()->PrintFormat(format, __VA_ARGS__); LogService::Instance()->PrintFormat("\n");
#else
#define GPrint(format, ...)          LogService::Instance()->PrintFormat(format, ## __VA_ARGS__);
#define GPrintRet(format, ...)       LogService::Instance()->PrintFormat(format, ## __VA_ARGS__); LogService::Instance()->PrintFormat("\n");
#endif

