// Hardware identity is computed once, independently of networking and scanning.
#include <Windows.h>
#include <wincrypt.h>
#include <comdef.h>
#include <wbemidl.h>
#include <string>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <intrin.h>
#include "obfuscate.h"
#include "runtime.hpp"
#pragma comment(lib, "wbemuuid.lib")
std::string GetLocalHWID() {
    std::stringstream raw;
    int cpuInfo[4] = { 0 };
    __cpuid(cpuInfo, 1);
    char cpuHex[17] = { 0 };
    sprintf(cpuHex, AY_OBFUSCATE("%08X%08X"), cpuInfo[3], cpuInfo[0]);
    raw << cpuHex;

    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, AY_OBFUSCATE("SOFTWARE\\Microsoft\\Cryptography"), 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
        char guid[128] = { 0 };
        DWORD size = sizeof(guid);
        if (RegQueryValueExA(hKey, AY_OBFUSCATE("MachineGuid"), NULL, NULL, (BYTE*)guid, &size) == ERROR_SUCCESS) {
            std::string guidStr(guid);
            guidStr.erase(std::remove_if(guidStr.begin(), guidStr.end(), [](char c) { return c == '-' || isspace(c); }), guidStr.end());
            if (!guidStr.empty()) raw << guidStr;
        }
        RegCloseKey(hKey);
    }

    char volumePath[4] = "C:\\";
    DWORD serial = 0;
    GetVolumeInformationA(volumePath, NULL, 0, &serial, NULL, NULL, NULL, 0);
    char serialHex[9] = { 0 };
    sprintf(serialHex, "%08X", serial);
    raw << serialHex;

    std::string input = raw.str();
    if (input.empty()) return std::string((const char*)AY_OBFUSCATE("00000000-0000-0000-0000-000000000000"));

    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    std::string finalHWID = AY_OBFUSCATE("00000000-0000-0000-0000-000000000000");
    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        if (CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash)) {
            CryptHashData(hHash, reinterpret_cast<const BYTE*>(input.c_str()), static_cast<DWORD>(input.length()), 0);
            BYTE hash[16];
            DWORD hashLen = 16;
            if (CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0)) {
                char hex[33] = { 0 };
                for (int i = 0; i < 16; ++i) sprintf(hex + (i * 2), "%02X", hash[i]);
                char formatted[37] = { 0 };
                sprintf(formatted, "%8.8s-%4.4s-%4.4s-%4.4s-%12.12s", hex, hex + 8, hex + 12, hex + 16, hex + 20);
                finalHWID = formatted;
            }
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }
    return finalHWID;
}

// =============================================================
// GetLocalHWID_V2() - Persistent Hardware ID (Valorant-style)
// เปลี่ยน Windows ใหม่ก็ไม่หาย ต้องเปลี่ยนเมนบอร์ด/ดิสก์จริง ๆ
// =============================================================
static std::string g_CachedHWID_V2 = "";

std::string GetWMISerial(const std::wstring& wqlQuery, const std::wstring& propertyName)
{
    std::string result;
    HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres)) return result;

    hres = CoInitializeSecurity(NULL, -1, NULL, NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE, NULL);

    IWbemLocator* pLoc = NULL;
    IWbemServices* pSvc = NULL;
    IEnumWbemClassObject* pEnumerator = NULL;

    if (SUCCEEDED(CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&pLoc)))
    {
        if (SUCCEEDED(pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0L, 0L, NULL, NULL, &pSvc)))
        {
            CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);

            if (SUCCEEDED(pSvc->ExecQuery(_bstr_t("WQL"), _bstr_t(wqlQuery.c_str()),
                WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator)))
            {
                IWbemClassObject* pclsObj = NULL;
                ULONG uReturn = 0;
                while (pEnumerator)
                {
                    HRESULT hr = pEnumerator->Next(5000, 1, &pclsObj, &uReturn);
                    if (hr != WBEM_S_NO_ERROR || uReturn == 0)
                        break;

                    VARIANT vtProp;
                    VariantInit(&vtProp);
                    if (SUCCEEDED(pclsObj->Get(propertyName.c_str(), 0, &vtProp, 0, 0)) &&
                        vtProp.vt == VT_BSTR && vtProp.bstrVal)
                    {
                        result = _bstr_t(vtProp.bstrVal);
                        VariantClear(&vtProp);
                        pclsObj->Release();
                        break;
                    }
                    VariantClear(&vtProp);
                    pclsObj->Release();
                }
                pEnumerator->Release();
            }
            pSvc->Release();
        }
        pLoc->Release();
    }
    CoUninitialize();
    return result;
}

// =============================================================
// GetLocalHWID_V2() - Persistent Hardware ID (ใช้ UUID จาก Motherboard)
// ลง Windows ใหม่ → ไม่เปลี่ยน
// ฟอร์แมตไดรฟ์ → ไม่เปลี่ยน
// เปลี่ยน CPU/RAM/GPU/SSD → ไม่เปลี่ยน
// เปลี่ยนเมนบอร์ดเท่านั้น → เปลี่ยน
// =============================================================


std::string GetLocalHWID_V2()
{
    if (!g_CachedHWID_V2.empty())
        return g_CachedHWID_V2;

    // ใช้ UUID จาก Win32_ComputerSystemProduct (แน่นที่สุด)
    std::string uuid = GetWMISerial(L"SELECT UUID FROM Win32_ComputerSystemProduct", L"UUID");

    // ป้องกันค่า default หรือว่าง
    if (uuid.empty() ||
        uuid == std::string(AY_OBFUSCATE("Default")) ||
        uuid.find(AY_OBFUSCATE("00000000-0000-0000-0000-000000000000")) != std::string::npos)
    {
        g_CachedHWID_V2 = AY_OBFUSCATE("00000000-0000-0000-0000-000000000000");
        return g_CachedHWID_V2;
    }

    // ใช้ UUID เป็น input แล้ว hash เหมือนเดิม (MD5 + GUID format)
    // เพื่อให้ format ตรงกับโค้ดเดิม ไม่ต้องแก้ส่วนอื่น
    std::string input = uuid;

    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    std::string finalHWID = AY_OBFUSCATE("00000000-0000-0000-0000-000000000000");

    if (CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
    {
        if (CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))
        {
            CryptHashData(hHash, (const BYTE*)input.c_str(), (DWORD)input.length(), 0);

            BYTE hash[16];
            DWORD hashLen = 16;
            if (CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0))
            {
                char hex[33] = { 0 };
                for (int i = 0; i < 16; ++i)
                    sprintf(hex + (i * 2), AY_OBFUSCATE("%02X"), hash[i]);

                char formatted[37] = { 0 };
                sprintf(formatted,
                    AY_OBFUSCATE("%8.8s-%4.4s-%4.4s-%4.4s-%12.12s"),
                    hex, hex + 8, hex + 12, hex + 16, hex + 20);

                finalHWID = formatted;
            }
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }

    g_CachedHWID_V2 = finalHWID;
    return g_CachedHWID_V2;
}


DWORD WINAPI IdentityWorker(LPVOID) {
    try {
        stp::Identity identity;
        identity.type = kClientType;
        identity.hwid = GetLocalHWID();
        identity.hwid2 = GetLocalHWID_V2();
        g_Session.SetIdentity(identity);
    } catch (...) { OutputDebugStringA("STP Vanguard: identity initialization failed\n"); }
    return 0;
}

