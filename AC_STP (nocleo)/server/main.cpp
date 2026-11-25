// main_server.cpp - Anti-Cheat Server Plugin v1.5.2+ 
// รองรับ Client v9.10.0 (HWID_V2) + Client เวอร์ชันเก่า + Dynamic File Report

#include <set>
#include <vector>
#include <string>
#include <version.hpp>
#include "pawn.hpp"
#include "network.hpp"
#include <session.hpp>
#include <chrono>
#include <array>
#ifdef _WIN32
#include <wincrypt.h>
#pragma comment(lib, "advapi32.lib")
#else
#include <cstdio>
#endif

constexpr Version kServerRequiredVersion = MakeVersion(9, 10, 0);

constexpr ubyte_t kPacketHandshake = 250;
constexpr ubyte_t kPacketCheatDetected = 251;
constexpr ubyte_t kPacketRequestHandshake = 252;

extern void* pAMXFunctions;
static std::set<AMX*> g_AmxInstances;

static void (*logprintf)(const char* format, ...);

static std::array<stp::ServerSession, 1000> g_Sessions;
static std::array<std::uint32_t, 1000> g_JoinTime{};
static std::uint32_t Now() { return static_cast<std::uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()); }
static bool RandomNonce(std::uint32_t& session, stp::Nonce& nonce) {
#ifdef _WIN32
    HCRYPTPROV provider = 0;
    if (!CryptAcquireContextA(&provider, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) return false;
    bool ok = CryptGenRandom(provider, sizeof(session), reinterpret_cast<BYTE*>(&session)) && CryptGenRandom(provider, static_cast<DWORD>(nonce.size()), nonce.data());
    CryptReleaseContext(provider, 0); return ok;
#else
    auto file = std::fopen("/dev/urandom", "rb"); if (!file) return false;
    bool ok = std::fread(&session, 1, sizeof(session), file) == sizeof(session) && std::fread(nonce.data(), 1, nonce.size(), file) == nonce.size();
    std::fclose(file); return ok;
#endif
}

// A RakNet connection can exist before OnPlayerConnect. Delay acceptance until
// IsPlayerConnected says the SA-MP player exists, avoiding an early callback
// being erased by gamemode initialization. Resolve the registered Pawn native.
static bool GamePlayerReady(uword_t player) {
    bool nativeFound = false;
    for (auto amx : g_AmxInstances) {
        int index = -1;
        if (amx_FindNative(amx, "IsPlayerConnected", &index) != AMX_ERR_NONE || !amx->base) continue;
        nativeFound = true;
        auto header = reinterpret_cast<const AMX_HEADER*>(amx->base);
        if (index < 0 || header->defsize < sizeof(ucell) || header->natives < 0 || header->libraries > header->size) continue;
        auto offset = header->natives + index * header->defsize;
        if (offset < header->natives || offset + int(sizeof(ucell)) > header->libraries) continue;
        ucell address = 0;
        std::memcpy(&address, amx->base + offset, sizeof(address));
        if (!address) continue;
        cell params[]{sizeof(cell), player};
        if (reinterpret_cast<AMX_NATIVE>(address)(amx, params)) return true;
    }
    // A script which never imports IsPlayerConnected cannot expose its native.
    return !nativeFound && Now() - g_JoinTime[player] >= 1500;
}

// ====================== Callback Functions ======================

static void CallPawnCallback(const char* funcName, int playerid, int value)
{
    int idx;
    for (AMX* amx : g_AmxInstances)
    {
        if (amx_FindPublic(amx, funcName, &idx) == AMX_ERR_NONE)
        {
            amx_Push(amx, static_cast<cell>(value));
            amx_Push(amx, static_cast<cell>(playerid));
            amx_Exec(amx, nullptr, idx);
        }
    }
}

static void CallPawnCallbackStr(const char* funcName, int playerid, const std::string& strVal)
{
    int idx;
    for (AMX* amx : g_AmxInstances)
    {
        if (amx_FindPublic(amx, funcName, &idx) == AMX_ERR_NONE)
        {
            cell amx_addr, * phys_addr;
            amx_PushString(amx, &amx_addr, &phys_addr, strVal.c_str(), 0, 0);
            amx_Push(amx, static_cast<cell>(playerid));
            amx_Exec(amx, nullptr, idx);
            amx_Release(amx, amx_addr);
        }
    }
}

// ====================== NEW: Dynamic File Callback ======================
static void CallPawnCallbackDynamic(const char* funcName, int playerid, int fileType,
    const std::string& filename, const std::string& md5hash)
{
    int idx;
    for (AMX* amx : g_AmxInstances)
    {
        if (amx_FindPublic(amx, funcName, &idx) == AMX_ERR_NONE)
        {
            cell amx_addr1, * phys_addr1, amx_addr2, * phys_addr2;

            amx_PushString(amx, &amx_addr2, &phys_addr2, md5hash.c_str(), 0, 0);
            amx_PushString(amx, &amx_addr1, &phys_addr1, filename.c_str(), 0, 0);
            amx_Push(amx, static_cast<cell>(fileType));
            amx_Push(amx, static_cast<cell>(playerid));

            amx_Exec(amx, nullptr, idx);

            amx_Release(amx, amx_addr2);
            amx_Release(amx, amx_addr1);
        }
    }
}

// ====================== Packet Handler ======================

static bool DispatchReport(const uword_t player, const cptr_t data, const uint_t size) noexcept
{
    if (size < 1 || data == nullptr) return false;

    const ubyte_t packetId = static_cast<const ubyte_t*>(data)[0];

    // ==================== Cheat Detected ====================
    if (packetId == kPacketCheatDetected)
    {
        if (size < 2) return false;

        int cheatId = static_cast<int>(static_cast<const ubyte_t*>(data)[1]);

        if (cheatId == 200 && size > 2) // Injected Module
        {
            if (size < 4) return false;
            ubyte_t strLen = static_cast<const ubyte_t*>(data)[2];
            if (size < 3 + strLen) return false;

            std::string modulePath(reinterpret_cast<const char*>(data) + 3, strLen);
            CallPawnCallbackStr("OnPlayerDetectedInjectedModule", static_cast<int>(player), modulePath);
            return true;
        }

        else if (cheatId == 210 || cheatId == 220) // New Injected Module (Runtime)
        {
            if (size < 4) return false;
            ubyte_t strLen = static_cast<const ubyte_t*>(data)[2];
            if (size < 3 + strLen) return false;

            std::string modulePath(reinterpret_cast<const char*>(data) + 3, strLen);
            CallPawnCallbackStr("OnPlayerDetectedNewInjectedModule", static_cast<int>(player), modulePath);
            return true;
        }

        // ==================== DYNAMIC FILE REPORT (205) ====================
        else if (cheatId == 205)
        {
            if (size < 20) return false;

            const ubyte_t fileType = static_cast<const ubyte_t*>(data)[2];

            const ubyte_t* md5Bytes = static_cast<const ubyte_t*>(data) + 3;
            ubyte_t nameLen = static_cast<const ubyte_t*>(data)[19];

            if (size < 20 + nameLen) return false;

            std::string filename(reinterpret_cast<const char*>(data) + 20, nameLen);

            // แปลง MD5 16 bytes เป็น Hex String
            char md5Hex[33] = { 0 };
            for (int i = 0; i < 16; ++i) {
                sprintf(md5Hex + (i * 2), "%02X", md5Bytes[i]);
            }

            CallPawnCallbackDynamic("OnPlayerDetectedDynamicFile",
                static_cast<int>(player),
                static_cast<int>(fileType),
                filename,
                std::string(md5Hex));

            return true;
        }

        // การตรวจจับปกติ (Static Cheat)
        if (size == 2)
        {
            CallPawnCallback("OnPlayerDetectedIllegalFile", static_cast<int>(player), cheatId);
            return true;
        }
    }

    return false;
}

static bool OnPacket(uword_t player, cptr_t data, uint_t size) noexcept {
    if (!data || size < 4) return false;
    auto p = static_cast<const ubyte_t*>(data);
    if (p[0] != stp::PacketId || p[1] != 'S' || p[2] != 'T' || p[3] != 'P') return false;
    if (player >= g_Sessions.size()) return true;
    try {
        stp::Frame frame;
        if (stp::Decode(data, size, frame)) g_Sessions[player].Receive(frame, Now());
    } catch (...) { logprintf("[STP Vanguard] Packet processing failed for %u", unsigned(player)); }
    return true;
}

PLUGIN_EXPORT void PLUGIN_CALL ProcessTick() noexcept {
    static std::uint32_t last = 0;
    auto now = Now();
    if (stp::Elapsed(now, last) < 100) return;
    last = now;
    for (uword_t player = 0; player < g_Sessions.size(); ++player) {
        if (Network::Instance().IsPlayerConnected(player)) g_Sessions[player].Tick(now);
    }
}

// ====================== Plugin Load ======================

PLUGIN_EXPORT bool PLUGIN_CALL Load(void** ppData) noexcept
{
    pAMXFunctions = ppData[PLUGIN_DATA_AMX_EXPORTS];
    logprintf = reinterpret_cast<void(*)(const char*, ...)>(ppData[PLUGIN_DATA_LOGPRINTF]);

    for (uword_t player = 0; player < g_Sessions.size(); ++player) {
        auto& session = g_Sessions[player];
        session.Reset(); session.random = RandomNonce;
        session.canAccept = [player] { return GamePlayerReady(player); };
        session.send = [player](const stp::Bytes& b) { return Network::Instance().SendPacketToPlayer(player, b.data(), static_cast<uint_t>(b.size())); };
        session.connected = [player](const stp::Identity& i) {
            CallPawnCallback("OnPlayerAntiCheatConnect", player, 1);
            CallPawnCallback("OnPlayerAntiCheatType", player, i.type);
            CallPawnCallbackStr("OnPlayerCheckHwid", player, i.hwid);
            CallPawnCallbackStr("OnPlayerCheckHwidV2", player, i.hwid2);
        };
        session.report = [player](const stp::Bytes& b) { DispatchReport(player, b.data(), static_cast<uint_t>(b.size())); };
        session.health = [player](bool healthy) {
            logprintf("[STP Vanguard] Player %u channel %s", unsigned(player), healthy ? "recovered" : "timed out");
            CallPawnCallback(healthy ? "OnPlayerAntiCheatRecovered" : "OnPlayerAntiCheatTimeout", player, healthy ? 1 : 0);
        };
    }
    Network::Instance().OnDisconnect = [](uword_t player) {
        if (player < g_Sessions.size()) { g_Sessions[player].Reset(); g_JoinTime[player] = Now(); }
    };
    Network::Instance().OnPacket = OnPacket;

    if (!Network::Instance().Initialize(ppData[PLUGIN_DATA_LOGPRINTF]))
    {
        logprintf("[Anti-Cheat] ERROR: Network initialization failed!");
        return false;
    }

    logprintf("=============================================================");
    logprintf("STP Vanguard 9.11.0 LOADED (request/response/ACK)");
    logprintf("Requires client 9.11.0; heartbeat 10s; grace 45s; no sampvoice dependency");
    logprintf("By FocusGamerCH");
    logprintf("=============================================================");

    return true;
}

PLUGIN_EXPORT void PLUGIN_CALL Unload() noexcept
{
    Network::Instance().Deinitialize();
    g_AmxInstances.clear();
}

PLUGIN_EXPORT int PLUGIN_CALL AmxLoad(AMX* amx) noexcept
{
    g_AmxInstances.insert(amx);
    // Reloaded gamemodes initialize their player state again. Start fresh
    // handshakes so the new script receives its initial callbacks as well.
    for (auto& session : g_Sessions) session.Reset();
    return AMX_ERR_NONE;
}

PLUGIN_EXPORT int PLUGIN_CALL AmxUnload(AMX* amx) noexcept
{
    g_AmxInstances.erase(amx);
    return AMX_ERR_NONE;
}

PLUGIN_EXPORT unsigned int PLUGIN_CALL Supports() noexcept
{
    return SUPPORTS_VERSION | SUPPORTS_AMX_NATIVES | SUPPORTS_PROCESS_TICK;
}
