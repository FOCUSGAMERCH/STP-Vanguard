#pragma once
#include <Windows.h>
#include <wincrypt.h>
#include <array>
#include <vector>
#include <algorithm>

// SHA-256 of each executable page, captured once after the module has loaded.
// A local integrity check detects later patches; it is not remote attestation.
class CodeIntegrity {
    struct Page { const BYTE* address; DWORD size; std::array<BYTE, 32> hash; };
    std::vector<Page> pages_;
    HCRYPTPROV provider_ = 0;
    std::size_t next_ = 0;
    bool Hash(const BYTE* data, DWORD size, std::array<BYTE, 32>& out) {
        HCRYPTHASH h = 0;
        if (!CryptCreateHash(provider_, CALG_SHA_256, 0, 0, &h)) return false;
        DWORD length = 32;
        bool ok = CryptHashData(h, data, size, 0) && CryptGetHashParam(h, HP_HASHVAL, out.data(), &length, 0);
        CryptDestroyHash(h); return ok;
    }
public:
    ~CodeIntegrity() { if (provider_) CryptReleaseContext(provider_, 0); }
    bool Initialize(HMODULE module) {
        if (!CryptAcquireContextA(&provider_, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) return false;
        auto base = reinterpret_cast<const BYTE*>(module);
        auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        auto pe = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
        auto section = IMAGE_FIRST_SECTION(pe);
        for (unsigned i = 0; i < pe->FileHeader.NumberOfSections; ++i) {
            if (!(section[i].Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
            for (DWORD offset = 0; offset < section[i].Misc.VirtualSize; offset += 4096) {
                Page p{base + section[i].VirtualAddress + offset, (std::min)(DWORD(4096), section[i].Misc.VirtualSize - offset), {}};
                if (!Hash(p.address, p.size, p.hash)) return false;
                pages_.push_back(p);
            }
        }
        return !pages_.empty();
    }
    bool Check() {
        if (pages_.empty()) return false;
        // Eight pages per pass bounds work to 32 KiB.
        for (unsigned i = 0; i < 8; ++i) {
            auto& p = pages_[next_];
            MEMORY_BASIC_INFORMATION m{};
            if (!VirtualQuery(p.address, &m, sizeof(m)) || m.State != MEM_COMMIT ||
                (m.Protect & (PAGE_GUARD | PAGE_NOACCESS | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) return false;
            std::array<BYTE, 32> hash{};
            if (!Hash(p.address, p.size, hash) || hash != p.hash) return false;
            next_ = (next_ + 1) % pages_.size();
        }
        return true;
    }
};
