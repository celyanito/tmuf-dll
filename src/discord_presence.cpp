#include "../include/discord_presence.h"
#include "../../external/discord_game_sdk/discord_game_sdk.h"

#include <windows.h>
#include <string>
#include <iostream>
#include <cstring>   // strncpy
#include <cctype>    // isxdigit

static const DiscordClientId DISCORD_APP_ID = 1478464595642417163ULL;

// IMPORTANT: c'est le "Name" dans Developer Portal > Rich Presence > Art Assets
static const char* ASSET_KEY = "is_race_icon";

static HMODULE g_sdk = nullptr;
static IDiscordCore* g_core = nullptr;
static IDiscordActivityManager* g_act = nullptr;
static std::string g_lastSentMap;

// On ne récupère que DiscordCreate en export.
// DiscordCreateParamsSetDefault est souvent inline dans le header.
static decltype(&DiscordCreate) pDiscordCreate = nullptr;

static std::string StripTmFormatting(const std::string& s)
{
    std::string out;
    out.reserve(s.size());

    for (size_t i = 0; i < s.size(); ++i)
    {
        if (s[i] == '$')
        {
            // $$ -> afficher un $
            if (i + 1 < s.size() && s[i + 1] == '$') {
                out.push_back('$');
                ++i;
                continue;
            }

            // $abc -> couleur hex (3 chars)
            if (i + 3 < s.size() &&
                std::isxdigit((unsigned char)s[i + 1]) &&
                std::isxdigit((unsigned char)s[i + 2]) &&
                std::isxdigit((unsigned char)s[i + 3]))
            {
                i += 3;
                continue;
            }

            // $w $o $i $t $s $g $z etc -> style (1 char)
            if (i + 1 < s.size()) {
                ++i;
                continue;
            }
        }

        out.push_back(s[i]);
    }

    return out;
}

static void CopyTo(char* dst, size_t dstSize, const char* src)
{
    if (!dst || dstSize == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    std::strncpy(dst, src, dstSize - 1);
    dst[dstSize - 1] = '\0';
}

// callback signature du SDK (convention correcte via macro)
static void DISCORD_CALLBACK OnActivityUpdated(void* /*callback_data*/, EDiscordResult /*result*/)
{
    // no-op
}

namespace discord_presence
{
    bool Init()
    {
        if (g_core) return true;

        g_sdk = LoadLibraryA("discord_game_sdk.dll");
        if (!g_sdk) {
            std::cout << "[discord] discord_game_sdk.dll not found (RPC disabled)\n";
            return false;
        }

        pDiscordCreate = (decltype(&DiscordCreate))GetProcAddress(g_sdk, "DiscordCreate");
        if (!pDiscordCreate) {
            std::cout << "[discord] missing export: DiscordCreate (wrong dll?)\n";
            FreeLibrary(g_sdk);
            g_sdk = nullptr;
            return false;
        }

        DiscordCreateParams params{};
        DiscordCreateParamsSetDefault(&params); // via header (souvent inline)

        params.client_id = DISCORD_APP_ID;
        params.flags = DiscordCreateFlags_Default;
        params.events = nullptr;
        params.event_data = nullptr;

        EDiscordResult res = pDiscordCreate(DISCORD_VERSION, &params, &g_core);
        if (res != DiscordResult_Ok || !g_core) {
            std::cout << "[discord] DiscordCreate failed res=" << (int)res << "\n";
            FreeLibrary(g_sdk);
            g_sdk = nullptr;
            g_core = nullptr;
            return false;
        }

        g_act = g_core->get_activity_manager(g_core);
        if (!g_act) {
            std::cout << "[discord] get_activity_manager failed\n";
            Shutdown();
            return false;
        }

        std::cout << "[discord] Rich Presence initialized\n";
        return true;
    }

    void Shutdown()
    {
        if (g_core && g_core->destroy) {
            g_core->destroy(g_core);
        }

        g_act = nullptr;
        g_core = nullptr;
        g_lastSentMap.clear();

        if (g_sdk) {
            FreeLibrary(g_sdk);
            g_sdk = nullptr;
        }

        std::cout << "[discord] shutdown\n";
    }

    void Tick()
    {
        if (!g_core) return;
        g_core->run_callbacks(g_core);
    }

    void SetMap(const std::string& mapName)
    {
        if (!g_act || mapName.empty()) return;

        // compare sur le nom "clean" pour éviter spam si brut change (codes $) mais pas le texte visible
        std::string clean = StripTmFormatting(mapName);
        if (clean.empty()) return;

        if (clean == g_lastSentMap) return;
        g_lastSentMap = clean;

        DiscordActivity act{};
        act.type = DiscordActivityType_Playing;

        std::string details = "Playing " + clean;
        CopyTo(act.details, sizeof(act.details), details.c_str());
        CopyTo(act.state, sizeof(act.state), "");

        CopyTo(act.assets.large_image, sizeof(act.assets.large_image), ASSET_KEY);
        CopyTo(act.assets.large_text, sizeof(act.assets.large_text), "TrackMania Forever");
        CopyTo(act.assets.small_image, sizeof(act.assets.small_image), "");
        CopyTo(act.assets.small_text, sizeof(act.assets.small_text), "");

        g_act->update_activity(g_act, &act, nullptr, &OnActivityUpdated);
    }

    void Clear()
    {
        if (!g_act) return;

        g_lastSentMap.clear();

        DiscordActivity act{};
        act.type = DiscordActivityType_Playing;

        CopyTo(act.details, sizeof(act.details), "");
        CopyTo(act.state, sizeof(act.state), "");
        CopyTo(act.assets.large_image, sizeof(act.assets.large_image), "");
        CopyTo(act.assets.large_text, sizeof(act.assets.large_text), "");
        CopyTo(act.assets.small_image, sizeof(act.assets.small_image), "");
        CopyTo(act.assets.small_text, sizeof(act.assets.small_text), "");

        g_act->update_activity(g_act, &act, nullptr, &OnActivityUpdated);
    }

    void SetMenu()
    {
        if (!g_act) return;

        // Evite de spam si on était déjà en menu
        if (g_lastSentMap == "__MENU__") return;
        g_lastSentMap = "__MENU__";

        DiscordActivity act{};
        act.type = DiscordActivityType_Playing;

        CopyTo(act.details, sizeof(act.details), "In Menu");
        CopyTo(act.state, sizeof(act.state), "");

        CopyTo(act.assets.large_image, sizeof(act.assets.large_image), ASSET_KEY);
        CopyTo(act.assets.large_text, sizeof(act.assets.large_text), "TrackMania Forever");
        CopyTo(act.assets.small_image, sizeof(act.assets.small_image), "");
        CopyTo(act.assets.small_text, sizeof(act.assets.small_text), "");

        g_act->update_activity(g_act, &act, nullptr, &OnActivityUpdated);
    }
}
