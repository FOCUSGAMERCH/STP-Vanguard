/*
    This is a KeyListener project file
    Developer: CyberMor <cyber.mor.2020@gmail.com>

    See more here https://github.com/CyberMor/keylistener

    Copyright (c) Daniel (CyberMor) 2020 All rights reserved
*/

#pragma once

#include <functional>
#include <array>
#include <cstdint>

#include <types.hpp>
#include <unprotect_scope.hpp>
#include <jump_hook.hpp>
#include <scanner.hpp>

#include "raknet/bitstream.h"
#include "raknet/networktypes.h"  // ใช้ชื่อนี้ตาม project ของคุณ

#ifdef _WIN32
#define STDCALL  __stdcall
#define THISCALL __thiscall
#else
#define STDCALL
#define THISCALL
#endif

#ifdef _WIN32
constexpr size_t kRaknetReceiveOffset = 10;
constexpr size_t kRaknetDeallocatePacketOffset = 12;
constexpr size_t kRaknetSendOffset = 7; // SA-MP 0.3.7 ABI, verified against Pawn.RakNet
#else
// Linux offsets (ถ้าใช้ Linux ปรับตามจริง)
constexpr size_t kRaknetReceiveOffset = 11;
constexpr size_t kRaknetDeallocatePacketOffset = 13;
constexpr size_t kRaknetSendOffset = 9;
#endif

typedef Packet* (THISCALL* raknet_receive_t)          (ptr_t);
typedef void    (THISCALL* raknet_deallocatepacket_t) (ptr_t, Packet*);
typedef bool    (THISCALL* raknet_send_t)             (ptr_t, BitStream*, PacketPriority, PacketReliability, char, PlayerID, bool);


struct Network {

    Network(const Network&) = delete;
    Network(Network&&) = delete;
    Network& operator=(const Network&) = delete;
    Network& operator=(Network&&) = delete;

private:

    Network() noexcept = default;
    ~Network() noexcept = default;

public:

    static Network& Instance() noexcept
    {
        static Network instance;
        return instance;
    }

public:

    bool Initialize(const cptr_t base) noexcept
    {
#ifdef _WIN32
        static const char kGetRakServerPattern[] =
            "\x64\xA1\x00\x00\x00\x00\x50\x64\x89\x25\x00\x00\x00\x00\x51\x68\x18\x0E\x00\x00\xE8"
            "\xFF\xFF\xFF\xFF\x83\xC4\x04\x89\x04\x24\x85\xC0\xC7\x44\x24\xFF\x00\x00\x00\x00\x74\x16";
        static const char kGetRakServerMask[] =
            "xxxxxxxxxxxxxxxx????x????xxxxxxxxxxx?xxxxxx";
#else
        static const char kGetRakServerPattern[] =
            "\x04\x24\xFF\xFF\xFF\xFF\x89\x75\xFF\x89\x5D\xFF\xE8\xFF\xFF\xFF\xFF\x89\x04\x24\x89"
            "\xC6\xE8\xFF\xFF\xFF\xFF\x89\xF0\x8B\x5D\xFF\x8B\x75\xFF\x89\xEC\x5D\xC3";
        static const char kGetRakServerMask[] =
            "xx????xx?xx?x????xxxxxx????xxxx?xx?xxxx";
#endif

        const auto [address, length] = GetModuleInfo(base);
        if (address == nullptr) return false;

        const auto pointer = Scanner(address, length).Find(kGetRakServerPattern, kGetRakServerMask);
        if (pointer == nullptr) return false;

        return _hook_getrakserver.Initialize(static_cast<adr_t>(pointer) - 7, GetRakServerHook);
    }

    void Deinitialize() noexcept
    {
        if (_rakserver && _receive) {
            auto table = *static_cast<ptr_t**>(_rakserver);
            if (table[kRaknetReceiveOffset] == reinterpret_cast<ptr_t>(ReceiveHook)) {
                const UnprotectScope<sizeof(ptr_t)> scope{table + kRaknetReceiveOffset};
                table[kRaknetReceiveOffset] = reinterpret_cast<ptr_t>(_receive);
            }
        }
        _hook_getrakserver.Deinitialize();
        _rakserver = nullptr;
    }
    bool SendPacketToPlayer(uword_t player, const void* data, uint_t size) noexcept
    {
        if (!_rakserver || !_send || player >= endpoints.size() || !endpoints[player].online || !data || !size) return false;
        BitStream stream;
        stream.Write(static_cast<const char*>(data), size);
        return _send(_rakserver, &stream, HIGH_PRIORITY, RELIABLE_ORDERED, 7, endpoints[player].id, false);
    }
    bool IsPlayerConnected(uword_t player) const noexcept { return player < endpoints.size() && endpoints[player].online; }

private:

#ifdef _WIN32
    static Packet* __fastcall ReceiveHook(const ptr_t, void*) noexcept
#else
    static Packet* ReceiveHook(const ptr_t) noexcept
#endif
    {
        for (unsigned consumed = 0; consumed < 64; ++consumed) {
            Packet* packet = Instance().Receive();
            Instance().TrackEndpoint(packet);
            if (!packet || !Instance().OnPacket ||
                !Instance().OnPacket(packet->playerIndex, packet->data, packet->length)) return packet;
            Instance().DeallocatePacket(packet);
        }
        return nullptr;
    }

private:

    static ptr_t STDCALL GetRakServerHook() noexcept
    {
        const auto GetRakServer = Instance()._hook_getrakserver.Address();
        Instance()._hook_getrakserver.Deinitialize();

        Instance()._rakserver = reinterpret_cast<ptr_t(*)()>(GetRakServer)();
        if (Instance()._rakserver != nullptr)
        {
            const auto vtable = *static_cast<ptr_t**>(Instance()._rakserver);

            Instance()._receive = reinterpret_cast<raknet_receive_t>(vtable[kRaknetReceiveOffset]);
            Instance()._deallocatepacket = reinterpret_cast<raknet_deallocatepacket_t>(vtable[kRaknetDeallocatePacketOffset]);
            Instance()._send = reinterpret_cast<raknet_send_t>(vtable[kRaknetSendOffset]);

            const UnprotectScope<sizeof(ptr_t)> scope{ vtable + kRaknetReceiveOffset };
            vtable[kRaknetReceiveOffset] = reinterpret_cast<ptr_t>(ReceiveHook);
        }

        return Instance()._rakserver;
    }

private:

    Packet* Receive() noexcept
    {
        return _receive(_rakserver);
    }

    void DeallocatePacket(Packet* const packet) noexcept
    {
        _deallocatepacket(_rakserver, packet);
    }

public:

    std::function<bool(uword_t, cptr_t, uint_t)> OnPacket;
    std::function<void(uword_t)> OnDisconnect;

private:

    ptr_t _rakserver = nullptr;

    raknet_receive_t _receive = nullptr;
    raknet_deallocatepacket_t _deallocatepacket = nullptr;
    raknet_send_t _send = nullptr;  // เพิ่ม


    struct Endpoint { PlayerID id{}; bool online = false; };
    std::array<Endpoint, 1000> endpoints{};
    void TrackEndpoint(const Packet* packet) {
        if (!packet || !packet->data || !packet->length || packet->playerIndex >= endpoints.size()) return;
        auto& endpoint = endpoints[packet->playerIndex];
        auto id = packet->data[0];
        if (id == 21 || id == 22) {
            endpoint.online = false;
            if (OnDisconnect) OnDisconnect(packet->playerIndex);
            return;
        }
        // Endpoint belongs to the actual received RakNet packet, never guessed vtable slots.
        if (!endpoint.online || id == 19 || endpoint.id.binaryAddress != packet->playerId.binaryAddress || endpoint.id.port != packet->playerId.port) {
            if (OnDisconnect) OnDisconnect(packet->playerIndex);
            endpoint.id = packet->playerId; endpoint.online = true;
        }
    }
    JumpHook _hook_getrakserver;

};
