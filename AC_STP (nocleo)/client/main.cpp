#include <Windows.h>
#include <string>
#include "network.hpp"
#include "runtime.hpp"
#include "diagnostics.hpp"

static bool IsTargetServer(RakClientInterface* client) {
    if (!client || !client->IsConnected()) return false;
    auto id = client->GetServerID();
    auto ip = client->PlayerIDToDottedIP(id);
    return ip && id.port == 7777 && (!strcmp(ip, "168.222.20.248") || !strcmp(ip, "127.0.0.1"));
}

static void NetworkTick() {
    auto client = Network::Instance().GetRakClient();
    bool active = IsTargetServer(client);
    if (!active) {
        if (g_Active.exchange(false)) g_Session.Reset();
        return;
    }
    g_Active = true;
    g_Session.Tick(GetTickCount());
}

static DWORD WINAPI Bootstrap(LPVOID) {
    LogStatus(kClientType == 0 ? "9.11.0 starting: CLEO allowed" : "9.11.0 starting: no CLEO");
    char path[MAX_PATH]{};
    GetModuleFileNameA(g_Self, path, MAX_PATH);
    const char* name = strrchr(path, '\\'); name = name ? name + 1 : path;
    if (_stricmp(name, "STP.Vanguard.asi")) {
        MessageBoxA(nullptr, "Rename the client to STP.Vanguard.asi", "STP Vanguard", MB_OK | MB_ICONERROR); return 0;
    }
    // Pin: callbacks and the scanner must never outlive the module.
    HMODULE pinned = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCSTR>(&Bootstrap), &pinned)) return 0;
    auto& network = Network::Instance();
    g_Session.send = [](const stp::Bytes& b) { return Network::Instance().SendRaw(b); };
    network.QueueReport = [](const void* p, std::size_t n) { return g_Session.Enqueue(p, n); };
    network.OnTick = NetworkTick;
    network.OnDisconnect = [] { g_Active = false; g_Session.Reset(); };
    network.OnPacket = [](const void* data, std::size_t size) {
        if (!data || size < 4) return false;
        auto p = static_cast<const unsigned char*>(data);
        if (p[0] != stp::PacketId || p[1] != 'S' || p[2] != 'T' || p[3] != 'P') return false;
        stp::Frame frame;
        if (g_Active && stp::Decode(data, size, frame)) {
            bool wasReady = g_Session.ready;
            g_Session.Receive(frame, GetTickCount());
            if (!wasReady && g_Session.ready) LogStatus("Server accepted handshake; checks active");
        }
        return true;
    };
    for (auto worker : {IdentityWorker, ScanWorker, IntegrityWorker}) {
        auto h = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr);
        if (!h) { OutputDebugStringA("STP Vanguard: failed to start worker\n"); return 0; }
        CloseHandle(h);
    }
    bool warned = false;
    while (true) {
        auto samp = GetModuleHandleA("samp.dll");
        if (samp && network.Initialize(samp)) { Sleep(2000); continue; }
        if (samp && !warned) { LogStatus("Waiting for SA-MP RakClient (supported R1/R3/R5)"); warned = true; }
        Sleep(250);
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_Self = module;
        DisableThreadLibraryCalls(module);
        auto thread = CreateThread(nullptr, 0, Bootstrap, nullptr, 0, nullptr);
        if (!thread) return FALSE;
        CloseHandle(thread);
    }
    return TRUE;
}
