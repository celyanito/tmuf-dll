#pragma once
#include "pch.h"

// ================= SAFE MEMORY =================
static bool IsReadableAddress(uintptr_t addr, size_t size = 4)
{
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi)) == 0) return false;
    if (mbi.State != MEM_COMMIT) return false;

    const DWORD protect = mbi.Protect & 0xFF;
    const bool readable =
        protect == PAGE_READONLY ||
        protect == PAGE_READWRITE ||
        protect == PAGE_WRITECOPY ||
        protect == PAGE_EXECUTE_READ ||
        protect == PAGE_EXECUTE_READWRITE ||
        protect == PAGE_EXECUTE_WRITECOPY;

    if (!readable) return false;

    uintptr_t regionStart = (uintptr_t)mbi.BaseAddress;
    uintptr_t regionEnd = regionStart + (uintptr_t)mbi.RegionSize;

    return addr + size <= regionEnd;
}

template<typename T>
static bool SafeRead(uintptr_t addr, T& out)
{
    if (!IsReadableAddress(addr, sizeof(T))) return false;
    std::memcpy(&out, (const void*)addr, sizeof(T));
    return true;
}

static bool IsPlausiblePtr(uintptr_t p)
{
    // x86 userland typical range
    return p > 0x10000 && p < 0x7FFF0000;
}

static bool IsPrintableAscii(const char* s, int n)
{
    if (!s || n <= 0 || n > 128) return false;
    for (int i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c < 32 || c > 126) return false;
    }
    return true;
}