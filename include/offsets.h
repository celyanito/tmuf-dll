#pragma once
#include <cstdint>

namespace offsets
{
    // CTrackMania candidates
    constexpr std::uintptr_t O_CTRACKMANIA_A = 0x00972EB8;
    constexpr std::uintptr_t O_CTRACKMANIA_B = 0x0096A2A4;

    // Other offsets
    constexpr std::uintptr_t PROFILE_OFFSET = 0x168;
    constexpr std::uintptr_t LOGIN_OFFSET = 0x34;

    constexpr std::uintptr_t MENUMGR_OFFSET = 0x194;
    constexpr std::uintptr_t GAMEMENU_OFFSET = 0x788;
    constexpr std::uintptr_t FRAMES_OFFSET = 0x68;

    // Challenge / map
    constexpr std::uintptr_t TM_Challenge = 0x198;
    constexpr std::uintptr_t Challenge_Name = 0x108;

    // Functions
    constexpr std::uintptr_t RVA_CMwId_GetName = 0x00535660;
}
