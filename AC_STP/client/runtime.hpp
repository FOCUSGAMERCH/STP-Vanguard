#pragma once
#include <Windows.h>
#include <atomic>
#include <session.hpp>
inline stp::ClientSession g_Session;
inline std::atomic<bool> g_Active{false};
inline HMODULE g_Self = nullptr;
#ifndef STP_CLIENT_TYPE
#define STP_CLIENT_TYPE 0
#endif
constexpr unsigned char kClientType = STP_CLIENT_TYPE;
DWORD WINAPI ScanWorker(LPVOID);
DWORD WINAPI IdentityWorker(LPVOID);
DWORD WINAPI IntegrityWorker(LPVOID);
