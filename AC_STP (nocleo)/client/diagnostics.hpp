#pragma once
#include <Windows.h>
#include <mutex>
#include <string>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include "runtime.hpp"
inline void LogStatus(const char* message) {
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    char path[MAX_PATH]{};
    if (!GetModuleFileNameA(g_Self, path, MAX_PATH)) return;
    auto slash = strrchr(path, '\\'); if (!slash) return;
    *(slash + 1) = '\0';
    std::string filename = std::string(path) + "STP.Vanguard.log";
    auto file = CreateFileA(filename.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    SYSTEMTIME time{}; GetLocalTime(&time);
    char line[512]{};
    int count = snprintf(line, sizeof(line), "%04u-%02u-%02u %02u:%02u:%02u %s\r\n",
        time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, message);
    DWORD written = 0;
    if (count > 0) WriteFile(file, line, static_cast<DWORD>((std::min)(count, int(sizeof(line) - 1))), &written, nullptr);
    CloseHandle(file);
}
