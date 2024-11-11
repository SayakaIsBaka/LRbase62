#include "framework.h"
#include "mem.h"

#include <iostream>
#include <filesystem>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>

#ifdef _DEBUG
#define DEBUG_CONSOLE_ENABLED
#endif

typedef int(__cdecl* LoadBms)(char* gp, char* filename, char* aud, char* g, char* meta, int bgaFlag, int scratchSide);
LoadBms oLoadBms;

typedef char* (__cdecl* StrUpr)(char* str);
StrUpr oStrUpr = (StrUpr)0x7175A8;

struct Patch {
    uintptr_t address;
    size_t len;
    const char* orig;
    const char* patch;
};

const std::vector<Patch> patches = {
    { 0x4B0828, 5, "\xB9\x10\x05\x00\x00", "\xB9\x04\x0F\x00\x00" }, // clear keysounds
    { 0x4B21F5, 6, "\x81\xFE\x0F\x05\x00\x00", "\x81\xFE\x03\x0F\x00\x00" }, // extend wav limit from 1295 to 3843 (1)
    { 0x4B2346, 6, "\x81\xFE\x0F\x05\x00\x00", "\x81\xFE\x03\x0F\x00\x00" }, // extend wav limit from 1295 to 3843 (2)
    //{ 0x4B0F28, 5, "\xE8\xA3\xA2\xF8\xFF", "\x90\x90\x90\x90\x90" }, // disable to upper
    { 0x4B248E, 6, "\x81\xFE\x0F\x05\x00\x00", "\x81\xFE\x03\x0F\x00\x00" }, // extend bmp limit from 1295 to 3843
};
const std::vector<uintptr_t> base36patches = {
    0x4B1891, // First call during channel parsing
    0x4B197C, // Second call during channel parsing
    0x4B1A7C, // Third call during channel parsing (unknown if actually needed)
    0x4B21E5, // First call during #WAV parsing
    0x4B233C, // Second call during #WAV parsing
    0x4B1DAC, // Call during #LNOBJ parsing
    0x4B2484, // Call during #BMP parsing
};

const DWORD Base36ToDec = 0x439dc0;
const DWORD ToUpper = 0x43B1D0;

int __cdecl Base62ToDec(char ch1, char ch2) {
    const std::string digits = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    int ret = 0;

    ret += digits.find(ch1) * 62;
    ret += digits.find(ch2);

    return ret;
}

// https://gist.github.com/takamin/2752a45c5cb4d0f9d1ff
std::string SJISToUTF8(const std::string& sjis) {
    std::string utf8_string;

    LPCCH pSJIS = (LPCCH)sjis.c_str();
    int utf16size = ::MultiByteToWideChar(932, MB_ERR_INVALID_CHARS, pSJIS, -1, 0, 0);
    if (utf16size != 0) {
        LPWSTR pUTF16 = new WCHAR[utf16size];
        if (::MultiByteToWideChar(932, 0, (LPCCH)pSJIS, -1, pUTF16, utf16size) != 0) {
            int utf8size = ::WideCharToMultiByte(CP_UTF8, 0, pUTF16, -1, 0, 0, 0, 0);
            if (utf8size != 0) {
                LPTSTR pUTF8 = new TCHAR[utf8size + 16];
                ZeroMemory(pUTF8, utf8size + 16);
                if (::WideCharToMultiByte(CP_UTF8, 0, pUTF16, -1, pUTF8, utf8size, 0, 0) != 0) {
                    utf8_string = std::string(pUTF8);
                }
                delete pUTF8;
            }
        }
        delete pUTF16;
    }
    return utf8_string;
}

bool isBase62BMS(const char* filename) {
    std::filesystem::path path = std::filesystem::u8path(SJISToUTF8(filename));
    std::ifstream infile(path);

    std::string line;
    while (std::getline(infile, line))
    {
        std::istringstream iss(line);
        std::string a, b;
        if (!(iss >> a >> b)) { continue; }
        if (a == "#BASE" && b == "62")
            return true;
    }

    return false;
}

bool isDigits(std::string str)
{
    return std::all_of(str.begin(), str.end(), ::isdigit);
}

char* __cdecl hkToUpper(char* str) {
    std::string s(str); // Copy string
    oStrUpr(&s[0]); // to upper to parse potentially weird commands
    if (s.rfind("#WAV", 0) == 0 || s.rfind("#BMP", 0) == 0) {
        memcpy(str, s.c_str(), 4); // Copy first 4 chars of line to parse properly afterwards
        return str;
    }
    else if (isDigits(s.substr(1, 5))) // If channel command
        return str;
    return oStrUpr(str);
}

// Basically a copy of 0x43B1D0 (except we call our own strupr implem)
__declspec(naked) char** trampToUpper() {
    __asm {
        push esi
        mov esi, ecx
        mov eax, [esi]
        test eax, eax
        jz end
        push eax
        call hkToUpper
        add esp, 4
     end:
        mov eax, esi
        pop esi
        retn
    }
}

void ApplyPatches(bool restore) {
    if (restore) {
        for (const Patch p : patches) {
            mem::WriteMemory((PVOID)p.address, (char*)p.orig, p.len);
        }
        for (const auto a : base36patches) {
            mem::HookFn((char*)a, (char*)Base36ToDec, 5);
        }
        mem::HookFn((char*)0x4B0F28, (char*)ToUpper, 5);
    }
    else {
        for (const Patch p : patches) {
            mem::WriteMemory((PVOID)p.address, (char*)p.patch, p.len);
        }
        for (const auto a : base36patches) {
            mem::HookFn((char*)a, (char*)Base62ToDec, 5);
        }
        mem::HookFn((char*)0x4B0F28, (char*)trampToUpper, 5);
    }
}

int __cdecl hkLoadBms(char* gp, char* filename, char* aud, char* g, char* meta, int bgaFlag, int scratchSide) {
    int numStages = gp[0x7C078];
    std::cout << "[+] Selected file: " << filename << std::endl;
    std::cout << "[+] Number of stages: " << std::to_string(numStages) << std::endl;
    if (numStages == 1 && isBase62BMS(filename)) {
        std::cout << "[+] File is base62!" << std::endl;
        ApplyPatches(false);
    }
    else {
        std::cout << "[+] File is not base62" << std::endl;
        ApplyPatches(true);
    }
    return oLoadBms(gp, filename, aud, g, meta, bgaFlag, scratchSide);
}

DWORD WINAPI Setup(HMODULE hModule)
{
#ifdef DEBUG_CONSOLE_ENABLED
	AllocConsole();
	FILE* f = nullptr;
	freopen_s(&f, "CONOUT$", "w", stdout);
#endif

    oLoadBms = (LoadBms)mem::TrampHook((char*)0x4B0690, (char*)hkLoadBms, 7);

#ifdef DEBUG_CONSOLE_ENABLED
	while (true)
	{
	}
	fclose(f);
	FreeConsole();
#endif

#pragma warning(push)
#pragma warning(disable : 6258)
	TerminateThread(GetCurrentThread(), 0);
#pragma warning(pop)
	return 0;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    HANDLE hThread;
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        hThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)Setup, hModule, 0, nullptr);
        if (hThread == nullptr)
        {
            std::cout << "[!] Couldn't create main thread" << std::endl;
            return FALSE;
        }

        CloseHandle(hThread);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
