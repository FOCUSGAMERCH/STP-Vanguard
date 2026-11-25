#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace stp {
using Bytes = std::vector<std::uint8_t>;
constexpr std::uint8_t PacketId = 249;
constexpr std::size_t MaxPayload = 256;
constexpr std::uint32_t HeartbeatMs = 10000;
constexpr std::uint32_t TimeoutMs = 45000;
enum class Kind : std::uint8_t { Hello = 1, Challenge, Response, Accepted, Report, ReportAck };
inline std::uint32_t Elapsed(std::uint32_t now, std::uint32_t then) { return now - then; }
inline std::uint32_t RetryMs(unsigned attempt) { return 1000u << (attempt > 3 ? 3 : attempt); }
inline void Put32(Bytes& out, std::uint32_t n) {
    for (unsigned i = 0; i < 4; ++i) out.push_back(static_cast<std::uint8_t>(n >> (i * 8)));
}
inline std::uint32_t Get32(const std::uint8_t* p) {
    return std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8 | std::uint32_t(p[2]) << 16 | std::uint32_t(p[3]) << 24;
}
struct Frame { Kind kind{}; std::uint32_t session = 0, sequence = 0; Bytes payload; };
inline Bytes Encode(Kind kind, std::uint32_t session, std::uint32_t sequence, const Bytes& payload = {}) {
    if (payload.size() > MaxPayload) return {};
    Bytes out{PacketId, 'S', 'T', 'P', 1, static_cast<std::uint8_t>(kind)};
    Put32(out, session); Put32(out, sequence);
    out.push_back(static_cast<std::uint8_t>(payload.size()));
    out.push_back(static_cast<std::uint8_t>(payload.size() >> 8));
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}
inline bool Decode(const void* raw, std::size_t size, Frame& f) {
    if (!raw || size < 16 || size > 16 + MaxPayload) return false;
    auto p = static_cast<const std::uint8_t*>(raw);
    if (p[0] != PacketId || p[1] != 'S' || p[2] != 'T' || p[3] != 'P' || p[4] != 1 || p[5] < 1 || p[5] > 6) return false;
    const auto n = std::size_t(p[14]) | std::size_t(p[15]) << 8;
    if (n != size - 16) return false;
    f.kind = static_cast<Kind>(p[5]); f.session = Get32(p + 6); f.sequence = Get32(p + 10);
    f.payload.assign(p + 16, p + size); return true;
}
inline bool IsHwid(const std::string& s) {
    if (s.size() != 36) return false;
    for (std::size_t n = 0; n < s.size(); ++n) {
        unsigned char c = s[n];
        if (n == 8 || n == 13 || n == 18 || n == 23) { if (c != '-') return false; continue; }
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}
struct Identity { std::uint8_t major = 9, minor = 11, patch = 0, type = 0; std::string hwid, hwid2; };
inline Bytes EncodeIdentity(const Identity& i) {
    if (!IsHwid(i.hwid) || !IsHwid(i.hwid2) || i.type > 1) return {};
    Bytes b{i.major, i.minor, i.patch, i.type, 36};
    b.insert(b.end(), i.hwid.begin(), i.hwid.end()); b.push_back(36);
    b.insert(b.end(), i.hwid2.begin(), i.hwid2.end()); return b;
}
inline bool DecodeIdentity(const Bytes& b, Identity& i) {
    if (b.size() != 78 || b[3] > 1 || b[4] != 36 || b[41] != 36) return false;
    i.major = b[0]; i.minor = b[1]; i.patch = b[2]; i.type = b[3];
    i.hwid.assign(b.begin() + 5, b.begin() + 41); i.hwid2.assign(b.begin() + 42, b.end());
    return IsHwid(i.hwid) && IsHwid(i.hwid2);
}
inline bool ValidReport(const Bytes& b) {
    if (b.size() < 2 || b[0] != 251) return false;
    if (b[1] == 205) return b.size() >= 21 && (b[2] == 1 || b[2] == 2) && b[19] > 0 && b[19] <= 200 && b.size() == 20u + b[19];
    if ((b[1] == 200 || b[1] == 210 || b[1] == 220) && b.size() != 2)
        return b.size() >= 4 && b[2] > 0 && b[2] <= 253 && b.size() == 3u + b[2];
    return b.size() == 2;
}
// Only one outstanding report is sent. Server callbacks run once even if ACKs are lost.
struct ReportSequence {
    std::uint32_t last = 0;
    bool Duplicate(std::uint32_t n) const { return n != 0 && n == last; }
    bool Next(std::uint32_t n) const { return n != 0 && n == last + 1; }
};
}
