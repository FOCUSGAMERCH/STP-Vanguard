// Background detection and signature database. All network reports are queued.
// รองรับตรวจจับ external cheat (AimBot.exe, Emperor Cheat AB ฯลฯ)
// เทคนิค: Compare Encrypted, Dynamic Salt, Runtime Wipe, Polymorphic Checks

#include <Windows.h>
#include <string>
#include <vector>
#include <filesystem>
#include <wincrypt.h>
#include <tlhelp32.h>           // สำหรับ CreateToolhelp32Snapshot
#include <psapi.h>
#include <version.hpp>
#include "network.hpp"
#include "runtime.hpp"
#include <policy.hpp>
#include "integrity.hpp"
#include <unordered_map>
#include <thread>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <intrin.h>
#include <ctime>
#include <random>
#include "obfuscate.h"          // AY_OBFUSCATE
#include <unordered_set>
#include <mutex>
#include <mmsystem.h>     // สำหรับ joyGetNumDevs, joyGetPosEx, joyReleaseCapture
#pragma comment(lib, "winmm.lib")



#pragma comment(lib, "psapi.lib")


// ==================== ส่วนที่ต้องเพิ่มก่อน (ที่ต้นไฟล์ client_old.cpp) ====================
// เพิ่ม global variables เหล่านี้ (วางไว้ใกล้ ๆ กับ g_IsScanningExternal)
// ใกล้กับ static std::unordered_set<std::string> g_ReportedSuspiciousModules;
static std::unordered_set<std::string> g_ReportedInjectedModules;  // ชื่อไฟล์ lowercase หรือ path เต็มก็ได้
static std::mutex                     g_ReportedMutex; // สำหรับ PerformModuleScan

namespace fs = std::filesystem;

// เพิ่มตัวแปร global สำหรับตรวจจับการ inject
static std::unordered_set<std::string> g_InitialModules;
static std::mutex                     g_ModuleMutex;
static bool                           g_FirstModuleScanDone = false;



constexpr ubyte_t kPacketCheatDetected = 251;


// ตัวแปรสำหรับ Dynamic Report 
static std::unordered_set<std::string> g_ReportedDynamicHashes;
static std::mutex g_DynamicHashMutex;
// =============================================================
// [SECTION] Super Thick Security Definitions
// =============================================================
struct SecureEntry {
    int id;
    ubyte_t enc_hash[16]; // MD5 distorted
    uint32_t key;
    uint32_t salt;
};

std::vector<SecureEntry> g_SecureDatabase;

// External Cheat Database
struct ExternalCheat {
    const char* md5Hash;   // MD5 ที่ต้องการตรวจจับ (บังคับ)
    uint8_t cheatId;      // ID แจ้งเตือน
    const char* displayName;
};

// เพิ่มใน struct ExternalCheat (เพิ่ม injectors)
static const ExternalCheat kExternalCheats[] = {
    { AY_OBFUSCATE("E42A00860ED621EDC390316902486D4C"), 101, AY_OBFUSCATE("AimBot.exe") },
    { AY_OBFUSCATE("4D9F6197E7A6178454909D57C2DCAF27"), 102, AY_OBFUSCATE("Emperor Cheat AB") },
    { AY_OBFUSCATE("9FE4F4156B5331C3184D16B501BA0EE6"), 103, AY_OBFUSCATE("Emperor Cheat AB") },
    { AY_OBFUSCATE("EC801A7D4B72A288EC6C207BB9FF0131"), 104, AY_OBFUSCATE("Extreme Injector v3.exe") },
    { AY_OBFUSCATE("109E961FAFCF553EEFC93DFA277194F3"), 105, AY_OBFUSCATE("loader.exe") },
    { AY_OBFUSCATE("3532A6075AFA9316FA390452E26A5EDA"), 107, AY_OBFUSCATE("AimBot.exe") },
    { AY_OBFUSCATE("4737E08DF332B241D3559E361A07D3BE"), 108, AY_OBFUSCATE("Emperor Injector.exe") },
    { AY_OBFUSCATE("5B2655B66D31CC8FD67570B79A0996C0"), 109, AY_OBFUSCATE("Aim po pingu.exe") },
    { AY_OBFUSCATE("6FC4E581181F9573FB27FAF641216095"), 110, AY_OBFUSCATE("NT037.exe") },
    { AY_OBFUSCATE("34D2F14813364EB4B72E27C7AA90521F"), 111, AY_OBFUSCATE("AutoHotkeyUX.exe") },
    { AY_OBFUSCATE("7EC9E2DA59BE0B5C5534B0668B6675BD"), 113, AY_OBFUSCATE("Saiyasart.exe") },
    { AY_OBFUSCATE("BE4E005E83D70E921D054A0CD2C9CA20"), 114, AY_OBFUSCATE("AIMBOT by Borg_Grown.exe") },
    { AY_OBFUSCATE("5C61E74653500E8E5A1165332846E343"), 115, AY_OBFUSCATE("PyAIM.exe") },
    { AY_OBFUSCATE("A7914C546413F493BC8C67798FE47236"), 116, AY_OBFUSCATE("cstlnsoft.exe") },
    { AY_OBFUSCATE("B365AF317AE730A67C936F21432B9C71"), 117, AY_OBFUSCATE("ProcessHacker.exe") },
    { nullptr, 0, nullptr }
};


// =============================================================
// Helper Functions
// =============================================================
void HexToBytes(const char* hex, ubyte_t* out) {
    for (int i = 0; i < 16; ++i) {
        sscanf(hex + 2 * i, "%02hhX", &out[i]);
    }
}

inline void DistortHash(const ubyte_t* input, ubyte_t* output, uint32_t key, uint32_t salt) {
    for (int i = 0; i < 16; ++i) {
        ubyte_t k = (key >> (i % 4)) & 0xFF;
        ubyte_t s = (salt >> (i % 3)) & 0xFF;
        output[i] = (input[i] ^ k) + s - static_cast<ubyte_t>(i * 3);
        output[i] = _rotl8(output[i], i % 5);
    }
}


struct HashCacheEntry {
    ULONGLONG size, write;
    DWORD checked;
    std::vector<ubyte_t> hash;
};
static std::unordered_map<std::string, HashCacheEntry> g_HashCache;
static std::vector<ubyte_t> GetFileMD5Bytes(const std::string& path) {
    WIN32_FILE_ATTRIBUTE_DATA a{};
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &a)) return {};
    ULONGLONG size = (ULONGLONG(a.nFileSizeHigh) << 32) | a.nFileSizeLow;
    ULONGLONG write = (ULONGLONG(a.ftLastWriteTime.dwHighDateTime) << 32) | a.ftLastWriteTime.dwLowDateTime;
    DWORD now = GetTickCount();
    auto found = g_HashCache.find(path);
    if (found != g_HashCache.end() && found->second.size == size && found->second.write == write &&
        now - found->second.checked < 60000) return found->second.hash;
    HANDLE file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) return {};
    HCRYPTPROV provider = 0; HCRYPTHASH hash = 0;
    std::vector<ubyte_t> result(16);
    bool ok = CryptAcquireContextA(&provider, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT) &&
        CryptCreateHash(provider, CALG_MD5, 0, 0, &hash);
    BYTE buffer[65536]; DWORD bytes = 0;
    while (ok) {
        if (!ReadFile(file, buffer, sizeof(buffer), &bytes, nullptr)) { ok = false; break; }
        if (!bytes) break;
        ok = CryptHashData(hash, buffer, bytes, 0) != FALSE;
        g_Session.progress.fetch_add(1);
        Sleep(1); // yield disk/CPU to the game
    }
    DWORD length = 16;
    if (ok) ok = CryptGetHashParam(hash, HP_HASHVAL, result.data(), &length, 0) != FALSE;
    if (hash) CryptDestroyHash(hash);
    if (provider) CryptReleaseContext(provider, 0);
    CloseHandle(file);
    if (!ok) return {};
    if (g_HashCache.size() >= 4096) g_HashCache.clear();
    g_HashCache[path] = {size, write, now, result};
    return result;
}

// =============================================================
// Initialize Secure Database (files in game folder)
// =============================================================
void InitializeSecureDB() {
    struct RawData { int id; const char* hex; };
    std::vector<RawData> raw_cheats = {
        {2,  AY_OBFUSCATE("129EA6210C75DAE6456BA6CAD6C1E15E")}, // 2 SprintHookByDarkP1xel.ASI
        {3,  AY_OBFUSCATE("CE84FACF907329EBDD2C23E377E95D00")}, // 3 InfinityRun_SA.asi
        {4,  AY_OBFUSCATE("9B975DBE6324EB44A6E5A1A04EF4A66F")}, // 4 anti_stun insert+X.cs etc.
        {5,  AY_OBFUSCATE("19655D9C191E33436BFD713FC5E6D4D4")}, // 5 MoonLoader.asi
        {6,  AY_OBFUSCATE("3DFF7448B43FCFB4DC65E0040B0FFB88")}, // 6 lua51.dll
        {7,  AY_OBFUSCATE("DF49FDB935C0C77BF11B2621B324D99E")}, // 7 IREPLAYv3.2.asi
        {8,  AY_OBFUSCATE("1E6C7F013A382DD468257F0C32DB3A68")}, // 8 StealthRemastered.asi
        {9,  AY_OBFUSCATE("A812738743B53332E66777854F34273E")}, // 9 clockweatherset.asi
        {10, AY_OBFUSCATE("08117920C2ABF5D9D3B2721F590B16F7")}, // 10 HeadShotAimBot.cs
        {11, AY_OBFUSCATE("35DA1C5A227D3AA0C28AE1EA5809F2CB")}, // 11 iReplay v.5
        {12, AY_OBFUSCATE("E46D3226FE1AEA8877BEBCF3B4CB2ACB")}, // 12 tr.cs
        {13, AY_OBFUSCATE("0E66DA8FB8383F1BBC7B987644CA31A1")}, // 13 Radarzoom.cs
        {14, AY_OBFUSCATE("06E0F354C6B0556847D67C43582866E8")}, // 14 2in1.cs
        {15, AY_OBFUSCATE("21EEB8C9EB584D7D3CD501D183EC87AA")}, // 15 asfist_1.cs
        {16, AY_OBFUSCATE("68FA6DD03F695D3B7FA42E1434551D09")}, // 16 fightSpeed.cs
        {17, AY_OBFUSCATE("B0B6864B47E330BA0558C5E0756B5B12")}, // 17 OPSpeedHack.cs
        {18, AY_OBFUSCATE("A79469F3BF0F854AD6D01215FEF498C3")}, // 18 Weaponhack.cs
        {19, AY_OBFUSCATE("55793AA53234EBA35B59D0FC8A8E95A9")}, // 19 5Ctroll.cs
        {20, AY_OBFUSCATE("0201C5693A9D93E6B65AA78BEA5DD7C9")}, // 20 Bigtroll.cs
        {21, AY_OBFUSCATE("3058BF35B017D22817D8505D6476C878")}, // 21 cboom.cs
        {22, AY_OBFUSCATE("4FCD83B4E70D4A3C8CFE4FFAA083BAF8")}, // 22 freeze.cs
        {23, AY_OBFUSCATE("2B5B90CA6A099C11157AF85476A615DC")}, // 23 lagpanic.cs
        {24, AY_OBFUSCATE("353ADEE8933CCA9BC508178A8E69D5FD")}, // 24 LagTroll.cs
        {25, AY_OBFUSCATE("D76B140A9C012ABF3486C856EC6DBDA8")}, // 25 slap O+U.cs 
        {26, AY_OBFUSCATE("22188641C1BAE3FB04BA500A66444436")}, // 26 takecar.cs 
        {27, AY_OBFUSCATE("B5C0FB72B4FEB2C7D2F07BCC6AFCA51A")}, // 27 tbkick.cs 
        {28, AY_OBFUSCATE("DCCE46466B0C9D6B37338ED722E6477F")}, // 28 SbivNaR.cs
        {29, AY_OBFUSCATE("1582AD989F1BC30BF8D48174CAE764E5")}, // 29 FastPedSyal.cs
        {30, AY_OBFUSCATE("9FA5263E9C6DF164380B6D9B5855D465")}, // 30 SPREAD.cs
        {31, AY_OBFUSCATE("7A12E471025AB6A3EFBA00679D39931D")}, // 31 bladeSprint.asi
        {32, AY_OBFUSCATE("ACFE3963ECF05252BE716A67A447A56B")}, // 32 arcane.asi
        {33, AY_OBFUSCATE("C22EE6CDC8028026D75F0DDF74EB761E")}, // 33 SKINCHANGER.cs
        {34, AY_OBFUSCATE("A30D8407AC1A2BCBA5361C946C9E5D8A")}, // 34 CrashRpt1402.dll
        {35, AY_OBFUSCATE("3C6C19C335C7BEED3A306A71264DB28E")}, // 35 FakeMobile...
        {36, AY_OBFUSCATE("3B53EF8E1DF6467F339996E7AE6A1C83")}, // 36 UltraFuck 2.36.luac
        {37, AY_OBFUSCATE("5A0F63031582937953B26FCFEB2A2DA6")}, // 37 sprintbind.cs
        {38, AY_OBFUSCATE("2DB3D3958514742A0494FCABCEDB48EF")}, // 38 Infinite Run.cs
        {39, AY_OBFUSCATE("FD5D094C1B8F835F8AA819B6DF7CA346")}, // 39 ddmg.sf
        {40, AY_OBFUSCATE("EC137C0FAB1A69C09F64BA3EB8557CB6")}, // 40 CLEO
        {41, AY_OBFUSCATE("181E79281AE5335EACF646D284F91B1D")}, // 41 SAMPFUNCS.asi
        {42, AY_OBFUSCATE("E473E36EDC290CF123D4F1FE05538B56")}, // 42 fast-enter-exit.cs
        {43, AY_OBFUSCATE("25BA1DDC4C0C9768B3A8529FD413ADA9")}, // 43 2in1.asi
        {43, AY_OBFUSCATE("834570446B3D05E96449A620DC269E62")}, // 44 2in1_sbiv_na_x_1.cs
        {46, AY_OBFUSCATE("63B7F87FF6B72F0D3F229E1516E0C2A3")}, // 46 ultrawh.asi
        {47, AY_OBFUSCATE("8CE24A2F9CB6D93D9B72D6C29788B8F6")}, // 47 cacmods.asi
        //{48, AY_OBFUSCATE("F5EA905A3A2923C8AE894E7F2AF0C3CD")}, // 48 leet.dll ไม่ใช่โปรเอาออก
        {49, AY_OBFUSCATE("754B1286E5689EC72C47BD9FC013082D")}, // 49 crack.asi
        // ---------------------------------- 2026 / 03 -----------------------------------///
        {50, AY_OBFUSCATE("82ED3A90427F5AA961D2F94B6B2C43C0")}, // 50 Anti-Stunshot(num4-5).cs
        {51, AY_OBFUSCATE("5A6C89ECE8D5E20965377045C1F9550B")}, // 51 antistun chanced.asi
        {52, AY_OBFUSCATE("4D4ECDF38B0F4FBEC752B3E6685013CE")}, // 52 Godmode-OPS-ON-OFF.cs
        {53, AY_OBFUSCATE("485337778A4082ADF88717D490A439FF")}, // 53 Godmode202.cs
        {54, AY_OBFUSCATE("1CE3B9D416FA90525D3ED9EC0495D550")}, // 54 HAKU.cs
        {55, AY_OBFUSCATE("9F69478198E175A098E09A42898E5446")}, // 55 HUEVIY AIM 0.3.7 by p1cador.cs
        {56, AY_OBFUSCATE("881FAFB4A035C7A740ED9DF920BA3BC3")}, // 56 LEGION.asi
        {57, AY_OBFUSCATE("7C7F2F3CB5CF18E730CA9C303B378A14")}, // 57 MHACK.sf
        {58, AY_OBFUSCATE("DE35586F415975D45E2600702629AFCE")}, // 58 Motherfuckerz.cs
        {59, AY_OBFUSCATE("B476A5765D030B169E44673BB2D82E48")}, // 59 NT037.cs
        {60, AY_OBFUSCATE("6E0675D90E6BCB8CD7DEF7EB4111F6CB")}, // 60 SAIM.asi
        {61, AY_OBFUSCATE("E93CA34A9D12F814C6827C01630C7061")}, // 61 SAMP 0.3.7 MultiHack.cs
        {62, AY_OBFUSCATE("01F0C7D45BBFE53D283A603D91A9F7BC")}, // 62 WH ALT+F2.cs
        {63, AY_OBFUSCATE("EAA8E7C1E23100131338BFEA7F68DB9E")}, // 63 LEGION v2.asi
        {64, AY_OBFUSCATE("D65679092030C32F0CBB00FE634DBC6A")}, // 64 MixSets.asi
        {65, AY_OBFUSCATE("E120425CEF6EB2DB7FE13202E65404D0")} // 65 Udrakoloader.asi


        // หากมี hash ต่างกันจริง ๆ ให้ใส่ค่าที่ถูกต้อง แล้วห่อด้วย AY_OBFUSCATE ทุกตัว
    };

    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    for (const auto& raw : raw_cheats) {
        SecureEntry entry;
        entry.id = raw.id;
        entry.key = (rand() << 16) | rand();
        entry.salt = (rand() << 16) | rand();

        std::string plain_hex = std::string(raw.hex); // AY_OBFUSCATE จะถูกถอดตอน runtime

        ubyte_t plain_hash[16];
        HexToBytes(plain_hex.c_str(), plain_hash);

        DistortHash(plain_hash, entry.enc_hash, entry.key, entry.salt);
        g_SecureDatabase.push_back(entry);

        // Wipe ทิ้ง
        volatile char* wipe = (volatile char*)plain_hex.data();
        for (size_t i = 0; i < plain_hex.size(); ++i) wipe[i] = 0;
    }
    {
        SecureEntry entry{}; entry.id = 221; entry.key = 0x294AB; entry.salt = 0x123;
        ubyte_t hash[16]; HexToBytes("E213D5630486F2BE79952AAAB3DE4445", hash);
        DistortHash(hash, entry.enc_hash, entry.key, entry.salt); g_SecureDatabase.push_back(entry);
    }
    raw_cheats.clear();
}


static void PerformExternalCheatScan()
{
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32 pe32 = { sizeof(pe32) };
    if (!Process32First(hSnapshot, &pe32))
    {
        CloseHandle(hSnapshot);
        return;
    }

    do
    {
        if (!g_Active) break;
        g_Session.progress.fetch_add(1);
        // ข้าม process ระดับระบบ / เล็กน้อย
        if (pe32.th32ProcessID <= 100) continue;

        std::string pName = pe32.szExeFile;
        std::transform(pName.begin(), pName.end(), pName.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (pe32.th32ProcessID == GetCurrentProcessId()) continue;

        char exePath[MAX_PATH] = { 0 };
        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe32.th32ProcessID);
        if (!hProc) continue;

        DWORD size = MAX_PATH;
        if (!QueryFullProcessImageNameA(hProc, 0, exePath, &size))
        {
            CloseHandle(hProc);
            continue;
        }

        // ตรวจขนาดไฟล์ก่อน (ลดภาระ)
        LARGE_INTEGER fileSize{};
        HANDLE hFile = CreateFileA(exePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (hFile == INVALID_HANDLE_VALUE)
        {
            CloseHandle(hProc);
            continue;
        }
        const bool sizeOK = GetFileSizeEx(hFile, &fileSize) != FALSE;
        CloseHandle(hFile);

        if (!sizeOK || fileSize.QuadPart <= 0 || fileSize.QuadPart > 25 * 1024 * 1024) // 25MB
        {
            CloseHandle(hProc);
            continue;
        }

        // คำนวณ MD5
        auto fileMd5 = GetFileMD5Bytes(exePath);
        if (fileMd5.size() != 16)
        {
            CloseHandle(hProc);
            continue;
        }

        // แปลง MD5 เป็น Hex String (ใช้ร่วมกันทั้ง 2 ส่วน)
        char computedHex[33] = { 0 };
        for (int b = 0; b < 16; ++b)
            sprintf(computedHex + b * 2, "%02X", fileMd5[b]);

        // === [ส่วนเพิ่มใหม่] ส่ง MD5 โปรแกรมภายนอกให้ Server ตรวจสอบแบบ Dynamic ===
        {
            std::string hashStr = computedHex;
            bool shouldReport = false;

            {
                std::lock_guard<std::mutex> lock(g_DynamicHashMutex);
                if (g_ReportedDynamicHashes.find(hashStr) == g_ReportedDynamicHashes.end())
                {
                    g_ReportedDynamicHashes.insert(hashStr);
                    shouldReport = true;
                }
            }

            if (shouldReport)
            {
                std::string dynFilename = pName;

                ubyte_t packet[256] = { 0 };
                packet[0] = kPacketCheatDetected;
                packet[1] = 205;        // ID สำหรับ Dynamic File Report
                packet[2] = 2;          // Type 2 = Process (โปรแกรมภายนอก)

                memcpy(packet + 3, fileMd5.data(), 16);

                ubyte_t nameLen = static_cast<ubyte_t>(std::min(dynFilename.length(), size_t(200)));
                packet[19] = nameLen;
                memcpy(packet + 20, dynFilename.c_str(), nameLen);

                if (!Network::Instance().SendPacket(packet, 20 + nameLen)) {
                    std::lock_guard<std::mutex> lock(g_DynamicHashMutex);
                    g_ReportedDynamicHashes.erase(hashStr);
                }
            }
        }
        // ========================================================

        // ตรวจสอบกับฐานข้อมูล cheat ที่รู้จัก
        for (size_t i = 0; kExternalCheats[i].md5Hash != nullptr; ++i)
        {
            if (_stricmp(computedHex, kExternalCheats[i].md5Hash) == 0)
            {
                ubyte_t packet[2] = { kPacketCheatDetected, kExternalCheats[i].cheatId };
                Network::Instance().SendPacket(packet, sizeof(packet));

                CloseHandle(hProc);
                CloseHandle(hSnapshot);
                return;                 // ออกจากฟังก์ชันทันที
            }
        }

        CloseHandle(hProc);

    } while (Process32Next(hSnapshot, &pe32));

    CloseHandle(hSnapshot);
}


// =============================================================
// ส่วนที่เหลือ (เหมือนเดิม + การเรียกใหม่)
// =============================================================
bool IsDebugged() {
    if (IsDebuggerPresent()) return true;
    BOOL isDbg = FALSE;
    CheckRemoteDebuggerPresent(GetCurrentProcess(), &isDbg);
    return (isDbg == TRUE);
}


static std::string GameDirectory() {
    char path[MAX_PATH]{};
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    return fs::path(path).parent_path().string();
}
static bool IsWhitelistedModule(const std::string& path) {
    char windows[MAX_PATH]{};
    GetWindowsDirectoryA(windows, MAX_PATH);
    std::string prefix = windows;
    std::transform(prefix.begin(), prefix.end(), prefix.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    prefix += '\\';
    if (path.compare(0, prefix.size(), prefix) == 0) return true;
    // Preserve common recording / overlay compatibility with directory boundaries.
    static const char* components[]{"\\nvidia corporation\\", "\\discord\\", "\\steam\\",
        "\\obs-studio\\", "\\rivatuner statistics server\\", "\\bandicam\\", "\\windhawk\\",
        "\\apple application support\\", "\\overwolf\\", "\\nahimic\\", "\\a-volute\\"};
    for (auto component : components) if (path.find(component) != std::string::npos) return true;
    return false;
}

static void PerformModuleScan()
{
    HANDLE hProcess = GetCurrentProcess();
    HMODULE hMods[1024];
    DWORD cbNeeded;
    if (!EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) return;

    int moduleCount = (std::min)(cbNeeded / sizeof(HMODULE), DWORD(sizeof(hMods) / sizeof(hMods[0])));

    // ดึง game directory ปัจจุบัน (ทำให้ match แน่นอน)
    char gameDir[MAX_PATH] = { 0 };
    strncpy_s(gameDir, GameDirectory().c_str(), _TRUNCATE);
    std::string gameDirLower = gameDir;
    std::transform(gameDirLower.begin(), gameDirLower.end(), gameDirLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (!gameDirLower.empty() && gameDirLower.back() != '\\') {
        gameDirLower += '\\';
    }

    std::lock_guard<std::mutex> lock(g_ReportedMutex);

    for (int i = 0; i < moduleCount; ++i)
    {
        char szModPath[MAX_PATH] = { 0 };
        if (GetModuleFileNameA(hMods[i], szModPath, sizeof(szModPath)) == 0) continue;

        std::string modPath = szModPath;
        std::string modPathLower = modPath;
        std::transform(modPathLower.begin(), modPathLower.end(), modPathLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        // ==================== แก้ false positive ====================
        // 1. ถ้าอยู่ในโฟลเดอร์เกม (รวม modloader, .data, plugins ฯลฯ) → ข้ามทันที
        if (modPathLower.find(gameDirLower) == 0) continue;

        // 2. ถ้าเป็น whitelist (Windhawk, Apple CoreAudio, Windows system ฯลฯ) → ข้าม
        if (IsWhitelistedModule(modPathLower)) continue;

        // 3. ตรวจเฉพาะ .asi และ .dll เท่านั้น
        std::string ext = fs::path(modPath).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext != ".asi" && ext != ".dll") continue;

        // ป้องกันสแปมรายงานซ้ำ
        if (g_ReportedInjectedModules.find(modPathLower) != g_ReportedInjectedModules.end()) {
            continue;
        }

        // บันทึกว่าส่งรายงานแล้ว
        g_ReportedInjectedModules.insert(modPathLower);

        // ส่งแจ้งเตือนไป Server (code 200 = Injected Module)
        ubyte_t packet[256] = { 0 };
        packet[0] = kPacketCheatDetected;
        packet[1] = 200;
        ubyte_t len = static_cast<ubyte_t>(std::min(modPath.length(), size_t(253)));
        packet[2] = len;
        memcpy(packet + 3, modPath.c_str(), len);

        // ส่ง 5 ครั้ง (พอแล้ว ไม่ต้องเยอะเกิน)
        Network::Instance().SendPacket(packet, 3 + len);
    }
}

static void PerformSecureCheatScan()
{
    if (IsDebugged())
    {
        ubyte_t buf[2] = { kPacketCheatDetected, 254 };
        Network::Instance().SendPacket(buf, sizeof(buf));
    }

    try
    {
        for (const auto& entry : fs::recursive_directory_iterator(GameDirectory(), fs::directory_options::skip_permission_denied))
        {
            g_Session.progress.fetch_add(1);
            if (!g_Active) return;
            Sleep(1);
            if (entry.is_symlink() || !entry.is_regular_file()) continue;

            // ข้ามไฟล์ที่ใหญ่เกินไป (ป้องกัน lag)
            if (entry.file_size() > 50 * 1024 * 1024) continue;

            std::string path = entry.path().string();
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            // ตรวจเฉพาะนามสกุลที่เกี่ยวข้องกับ mod/cheat/script
            if (ext != ".asi" && ext != ".dll" && ext != ".cs" && ext != ".sf" &&
                ext != ".cleo" && ext != ".lua" && ext != ".luac" &&
                ext != ".cfg" && ext != ".dat" && ext != ".ini" && ext != ".xml")
                continue;

            // ชื่อไฟล์แบบ lowercase เพื่อตรวจสอบ
            std::string filename_lower = entry.path().filename().string();
            std::transform(filename_lower.begin(), filename_lower.end(), filename_lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            // ==================== ตรวจไฟล์ต้องห้าม (Forbidden Files) ====================
            const char* forbiddenName = nullptr;
            ubyte_t forbiddenID = 0;

            if (filename_lower == "moonloader.asi") {
                forbiddenName = "MoonLoader.asi";
                forbiddenID = 200;
            }
            else if (filename_lower == "lua51.dll") {
                forbiddenName = "lua51.dll";
                forbiddenID = 200;
            }

            if (kClientType == 1)  // SA-MP
            {
                if (filename_lower == "cleo.asi") {
                    forbiddenName = "CLEO.asi";
                    forbiddenID = 40;
                }
                else if (filename_lower == "sampfuncs.asi") {
                    forbiddenName = "SAMPFUNCS.asi";
                    forbiddenID = 41;
                }
            }

            if (kClientType == 1 && (ext == ".cs" || ext == ".cleo" || ext == ".sf")) {
                forbiddenName = "CLEO script"; forbiddenID = 40;
            }
            if (forbiddenName != nullptr)
            {
                // 1. ส่งแพ็กเก็ตแจ้งเตือนเซิร์ฟเวอร์
                ubyte_t buf[2] = { kPacketCheatDetected, forbiddenID };
                Network::Instance().SendPacket(buf, sizeof(buf));

                return;
            }

            // ==================== คำนวณ MD5 ====================
            auto fileHash = GetFileMD5Bytes(path);
            if (fileHash.size() != 16) continue;

            // ==================== ตรวจสอบกับฐานข้อมูล Secure (Static Detection) ====================
            for (const auto& dbEntry : g_SecureDatabase)
            {
                if (stp::AllowCleoSignature(kClientType, dbEntry.id)) continue;
                ubyte_t candidate[16];
                DistortHash(fileHash.data(), candidate, dbEntry.key, dbEntry.salt);

                if (memcmp(candidate, dbEntry.enc_hash, 16) == 0)
                {
                    ubyte_t buf[2] = { kPacketCheatDetected, static_cast<ubyte_t>(dbEntry.id) };
                    Network::Instance().SendPacket(buf, sizeof(buf));
                    return;   // พบ cheat แบบ static → หยุดทันที
                }
            }

            // ==================== [Dynamic Report] ส่ง MD5 ทุกไฟล์ไปให้ Server ตรวจสอบ ====================
            {
                char hashHex[33] = { 0 };
                for (int b = 0; b < 16; ++b) {
                    sprintf(hashHex + b * 2, "%02X", fileHash[b]);
                }
                std::string hashStr = hashHex;

                bool shouldReport = false;
                {
                    std::lock_guard<std::mutex> lock(g_DynamicHashMutex);
                    if (g_ReportedDynamicHashes.find(hashStr) == g_ReportedDynamicHashes.end()) {
                        g_ReportedDynamicHashes.insert(hashStr);
                        shouldReport = true;
                    }
                }

                if (shouldReport)
                {
                    std::string dynFilename = entry.path().filename().string();

                    constexpr size_t MAX_NAME_LEN = 200;
                    ubyte_t nameLen = static_cast<ubyte_t>(std::min(dynFilename.length(), MAX_NAME_LEN));

                    ubyte_t packet[256] = { 0 };        // ศูนย์ทั้งหมดเพื่อความปลอดภัย
                    packet[0] = kPacketCheatDetected;
                    packet[1] = 205;                    // Dynamic File Report
                    packet[2] = 1;                      // Type 1 = โฟลเดอร์เกม

                    memcpy(packet + 3, fileHash.data(), 16);
                    packet[19] = nameLen;
                    memcpy(packet + 20, dynFilename.c_str(), nameLen);

                    if (!Network::Instance().SendPacket(packet, 20 + nameLen)) {
                    std::lock_guard<std::mutex> lock(g_DynamicHashMutex);
                    g_ReportedDynamicHashes.erase(hashStr);
                }
                }
            }
            // ========================================================
        }
    }
    catch (...)
    {
        // เงียบไว้เหมือนเดิม (ป้องกัน crash จาก permission หรือ symlink)
    }
}

// ช่วยแปลง path เป็นชื่อไฟล์ lowercase
static std::string GetModuleNameLower(const char* fullPath) {
    if (!fullPath || !*fullPath) return "";
    std::string path = fullPath;
    std::transform(path.begin(), path.end(), path.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    size_t lastSlash = path.find_last_of("\\/");
    if (lastSlash != std::string::npos) {
        path = path.substr(lastSlash + 1);
    }
    return path;
}

// เก็บรายชื่อ module ทั้งหมดตอนเริ่มต้น (เรียกครั้งเดียว)
static void CaptureInitialModuleList() {
    std::lock_guard<std::mutex> lock(g_ModuleMutex);

    HMODULE hMods[2048];  // เพิ่มขนาด buffer เผื่อ module เยอะ
    DWORD cbNeeded;
    if (!EnumProcessModules(GetCurrentProcess(), hMods, sizeof(hMods), &cbNeeded))
        return;

    DWORD count = (std::min)(cbNeeded / sizeof(HMODULE), DWORD(sizeof(hMods) / sizeof(hMods[0])));

    g_InitialModules.clear();

    for (DWORD i = 0; i < count; ++i) {
        char szPath[MAX_PATH] = { 0 };
        if (GetModuleFileNameA(hMods[i], szPath, MAX_PATH)) {
            std::string name = szPath;
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (!name.empty()) {
                g_InitialModules.insert(name);
            }
        }
    }

    g_FirstModuleScanDone = true;
}

static bool DetectNewInjectedModules() {
    if (!g_FirstModuleScanDone) return false;

    std::lock_guard<std::mutex> lock(g_ModuleMutex);

    HMODULE hMods[2048];
    DWORD cbNeeded;
    if (!EnumProcessModules(GetCurrentProcess(), hMods, sizeof(hMods), &cbNeeded))
        return false;

    DWORD count = (std::min)(cbNeeded / sizeof(HMODULE), DWORD(sizeof(hMods) / sizeof(hMods[0])));

    // ดึง game directory ปัจจุบัน
    char gameDir[MAX_PATH] = { 0 };
    strncpy_s(gameDir, GameDirectory().c_str(), _TRUNCATE);

    std::string gameDirLower = gameDir;
    std::transform(gameDirLower.begin(), gameDirLower.end(), gameDirLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (!gameDirLower.empty() && gameDirLower.back() != '\\') {
        gameDirLower += '\\';
    }

    for (DWORD i = 0; i < count; ++i) {
        char szPath[MAX_PATH] = { 0 };
        if (GetModuleFileNameA(hMods[i], szPath, MAX_PATH) == 0) continue;

        std::string modPathLower = szPath;
        std::transform(modPathLower.begin(), modPathLower.end(), modPathLower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        std::string nameLower = GetModuleNameLower(szPath);
        if (nameLower.empty()) continue;

        // === เงื่อนไขใหม่ (แก้ false positive) ===
        // 1. ถ้าอยู่ในโฟลเดอร์เกม → ข้ามเลย (เป็น mod ปกติ)
        if (modPathLower.find(gameDirLower) == 0) continue;

        // 2. ถ้าเคยมีตั้งแต่แรก → ข้าม
        if (g_InitialModules.find(modPathLower) != g_InitialModules.end()) continue;

        // 3. ถ้าอยู่ใน whitelist (Windhawk, Apple, etc.) → ข้าม
        if (IsWhitelistedModule(modPathLower)) continue;

        // === ถ้าผ่านทุกเงื่อนไข → เป็น injection จริง ===
        char msg[512];
        snprintf(msg, sizeof(msg),
            AY_OBFUSCATE("Suspicious injected module detected!\n"
                "Module: %s\n"
                "Path: %s\n\n"
                "This is NOT a normal game file."),
            nameLower.c_str(), szPath);

        ubyte_t packet[256] = { kPacketCheatDetected, 210 };
        size_t len = std::min(strlen(msg), size_t(200u));
        packet[2] = static_cast<ubyte_t>(len);
        memcpy(packet + 3, msg, len);

        Network::Instance().SendPacket(packet, 3 + len);

        g_InitialModules.insert(modPathLower);
        return true;
    }
    return false;
}



static void CheckGameFlags() {
    struct Flag { std::uintptr_t address; ubyte_t code; };
    static const Flag flags[]{{0xB7CEE4, 106}, {0x96916C, 112}};
    for (const auto& flag : flags) {
        auto p = reinterpret_cast<volatile BYTE*>(flag.address);
        if (Network::Readable(const_cast<const BYTE*>(p), 1) && *p == 1) {
            const ubyte_t report[]{251, flag.code};
            Network::Instance().SendPacket(report, sizeof(report));
            // Existing GTA SA 1.0 US policy: reset these cheat flags only.
            *p = 0;
        }
    }
}

static void CheckJoypad() {
    const UINT devices = joyGetNumDevs();
    for (UINT i = 0; i < devices; ++i) {
        JOYINFOEX info{}; info.dwSize = sizeof(info); info.dwFlags = JOY_RETURNALL;
        if (joyGetPosEx(i, &info) == JOYERR_NOERROR &&
            (info.dwButtons || abs(int(info.dwXpos) - 32767) > 6000 || abs(int(info.dwYpos) - 32767) > 6000)) joyReleaseCapture(i);
    }
}

DWORD WINAPI ScanWorker(LPVOID) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    InitializeSecureDB();
    CaptureInitialModuleList();
    DWORD files = 0, modules = 0, external = 0, flags = 0, joypad = 0;
    bool initial = true;
    auto generation = g_Session.generation.load();
    while (true) {
        g_Session.progress.fetch_add(1);
        if (generation != g_Session.generation.load()) {
            generation = g_Session.generation.load();
            { std::lock_guard<std::mutex> lock(g_DynamicHashMutex); g_ReportedDynamicHashes.clear(); }
            { std::lock_guard<std::mutex> lock(g_ReportedMutex); g_ReportedInjectedModules.clear(); }
            initial = true;
        }
        if (g_Active && g_Session.ready) {
            const DWORD now = GetTickCount();
            try {
                if (initial || now - files >= 45000) { PerformSecureCheatScan(); files = GetTickCount(); initial = false; }
                if (now - modules >= 7000) { PerformModuleScan(); DetectNewInjectedModules(); modules = GetTickCount(); }
                if (now - external >= 9000) { PerformExternalCheatScan(); external = GetTickCount(); }
                if (now - flags >= 1800) { CheckGameFlags(); flags = now; }
                if (now - joypad >= 3500) { CheckJoypad(); joypad = now; }
            } catch (...) { OutputDebugStringA("STP Vanguard: scan pass failed; retrying\n"); }
        } else { initial = true; }
        Sleep(250);
    }
}

DWORD WINAPI IntegrityWorker(LPVOID) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    CodeIntegrity integrity;
    g_Session.integrity = integrity.Initialize(g_Self);
    while (true) {
        if (!integrity.Check()) {
            g_Session.integrity = false;
            const ubyte_t report[]{251, 222};
            Network::Instance().SendPacket(report, sizeof(report));
        }
        Sleep(250);
    }
}
