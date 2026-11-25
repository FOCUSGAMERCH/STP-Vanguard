#pragma once
#include <Windows.h>
#include <atomic>
#include <functional>
#include <mutex>
#include <cstdint>
#include <vector>
#include <types.hpp>
#include "raknet/rakclient.h"

// Attach to the existing SA-MP RakClient, including a sampvoice wrapper if present.
// No LoadLibrary(samp.dll), factory detour, or dependency on ASI load order.
class Network {
public:
    static Network& Instance() { static Network n; return n; }
    std::function<void()> OnTick;
    std::function<bool(const void*, std::size_t)> OnPacket;
    std::function<bool(const void*, std::size_t)> QueueReport;
    std::function<void()> OnDisconnect;

    static bool Readable(const void* p, std::size_t n) {
        MEMORY_BASIC_INFORMATION m{};
        if (!p || !VirtualQuery(p, &m, sizeof(m)) || m.State != MEM_COMMIT || (m.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return false;
        auto begin = reinterpret_cast<std::uintptr_t>(p);
        auto end = reinterpret_cast<std::uintptr_t>(m.BaseAddress) + m.RegionSize;
        return begin <= end && n <= end - begin;
    }
    static bool Executable(const void* p) {
        MEMORY_BASIC_INFORMATION m{};
        return p && VirtualQuery(p, &m, sizeof(m)) && m.State == MEM_COMMIT &&
            !(m.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
            (m.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY));
    }
    bool Initialize(HMODULE samp) {
        if (!samp) return false;
        auto base = reinterpret_cast<std::uint8_t*>(samp);
        auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
        auto pe = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
        std::uint32_t netOffset = 0, rakOffset = 0;
        switch (pe->OptionalHeader.AddressOfEntryPoint) {
        case 0x31DF13: netOffset = 0x21A0F8; rakOffset = 0x3C9; break;
        case 0xCC4D0: netOffset = 0x26E8DC; rakOffset = 0x2C; break;
        case 0xCBC90: netOffset = 0x26EB94; rakOffset = 0; break;
        default: return false; // Unknown versions require a verified profile.
        }
        if (!Readable(base + netOffset, sizeof(void*))) return false;
        auto game = *reinterpret_cast<std::uint8_t**>(base + netOffset);
        if (!Readable(game, rakOffset + sizeof(void*))) return false;
        auto client = *reinterpret_cast<RakClientInterface**>(game + rakOffset);
        if (!Readable(client, sizeof(void*))) return false;
        auto table = *reinterpret_cast<void***>(client);
        if (!Readable(table, 10 * sizeof(void*))) return false;
        if (!Executable(table[7]) || !Executable(table[8]) || !Executable(table[9])) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        if (table[8] == reinterpret_cast<void*>(ReceiveHook)) { client_.store(client); return true; }
        // Never overwrite another hook after installing ours: it may chain to us.
        if (installed_) return false;
        DWORD old;
        if (!VirtualProtect(table + 8, sizeof(void*), PAGE_READWRITE, &old)) return false;
        receive_ = reinterpret_cast<ReceiveFn>(table[8]);
        client_.store(client);
        InterlockedExchangePointer(table + 8, reinterpret_cast<void*>(ReceiveHook));
        VirtualProtect(table + 8, sizeof(void*), old, &old);
        installed_ = true;
        return true;
    }
    RakClientInterface* GetRakClient() const { return client_.load(); }
    bool SendPacket(const void* data, std::size_t size) {
        return QueueReport && QueueReport(data, size); // workers enqueue; only game thread calls RakNet
    }
    bool SendRaw(const std::vector<std::uint8_t>& data) {
        auto client = GetRakClient();
        if (!client || data.empty() || !client->IsConnected()) return false;
        BitStream stream(const_cast<unsigned char*>(data.data()), static_cast<unsigned>(data.size()), false);
        return client->Send(&stream, HIGH_PRIORITY, RELIABLE_ORDERED, 7);
    }
private:
    using ReceiveFn = Packet* (__thiscall*)(RakClientInterface*);
    static Packet* __fastcall ReceiveHook(RakClientInterface* self, void*) {
        auto& n = Instance();
        // Limit consumed packets per frame, leaving normal GTA packets untouched.
        if (n.OnTick) n.OnTick();
        for (unsigned i = 0; i < 32; ++i) {
            auto packet = n.receive_(self);
            if (packet && packet->data && packet->length && (packet->data[0] == 21 || packet->data[0] == 22)) {
                if (n.OnDisconnect) n.OnDisconnect();
            }
            if (!packet || !n.OnPacket || !n.OnPacket(packet->data, packet->length)) return packet;
            self->DeallocatePacket(packet);
        }
        return nullptr;
    }
    std::atomic<RakClientInterface*> client_{nullptr};
    ReceiveFn receive_ = nullptr;
    bool installed_ = false;
    std::mutex mutex_;
};
