#include "../include/tm_mapname.h"
#include "../include/offsets.h"
#include <windows.h>
#include <mutex>
#include <cctype>
#include <string>
#include <cstdint>

namespace {
    std::mutex g_mtx;
    std::string g_last;

    template <typename T>
    bool SafeReadT(std::uintptr_t addr, T& out)
    {
        __try { out = *reinterpret_cast<T*>(addr); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    struct CFastStringInt {
        int Size;
        wchar_t* Cstr;
    };

    static std::string WStringToUTF8(const std::wstring& w)
    {
        if (w.empty()) return {};
        int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
        std::string out(n, 0);
        WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &out[0], n, nullptr, nullptr);
        return out;
    }

    static std::string ReadMapNameOnce()
    {
        std::uintptr_t base = (std::uintptr_t)GetModuleHandleW(nullptr);

        std::uintptr_t tmPtr = 0;
        if (!SafeReadT(base + offsets::O_CTRACKMANIA_A, tmPtr) || !tmPtr) {
            // fallback B
            SafeReadT(base + offsets::O_CTRACKMANIA_B, tmPtr);
        }
        if (!tmPtr) return {};

        std::uintptr_t chalPtr = 0;
        if (!SafeReadT(tmPtr + offsets::TM_Challenge, chalPtr) || !chalPtr) return {};

        CFastStringInt fs{};
        if (!SafeReadT(chalPtr + offsets::Challenge_Name, fs)) return {};
        if (!fs.Cstr || fs.Size <= 0 || fs.Size > 4096) return {};

        std::wstring w(fs.Cstr, fs.Cstr + fs.Size);
        return WStringToUTF8(w);
    }
} // anonymous namespace

namespace tmu
{
    std::string StripTmFormatting(const std::string& s)
    {
        std::string out;
        out.reserve(s.size());

        for (size_t i = 0; i < s.size(); ++i)
        {
            if (s[i] == '$')
            {
                // $$ => affiche un $
                if (i + 1 < s.size() && s[i + 1] == '$') {
                    out.push_back('$');
                    ++i;
                    continue;
                }

                // $rgb => couleur hex 3 chars
                if (i + 3 < s.size() &&
                    std::isxdigit((unsigned char)s[i + 1]) &&
                    std::isxdigit((unsigned char)s[i + 2]) &&
                    std::isxdigit((unsigned char)s[i + 3]))
                {
                    i += 3;
                    continue;
                }

                // $w $n $o $i $t $s $g $z etc => saute 1 char
                if (i + 1 < s.size()) {
                    ++i;
                    continue;
                }
            }

            out.push_back(s[i]);
        }

        return out;
    }

    bool TickMapName()
    {
        std::string cur = ReadMapNameOnce();
        if (cur.empty()) return false;

        std::lock_guard<std::mutex> _{ g_mtx };
        if (cur == g_last) return false;
        g_last = std::move(cur);
        return true;
    }

    std::string GetLastMapName()
    {
        std::lock_guard<std::mutex> _{ g_mtx };
        return g_last;
    }
}
