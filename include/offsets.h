#pragma once
#include <cstdint>

// ===== CTrackMania* candidates =====
// A: Twinkie / ton code (TMUF)
constexpr uintptr_t O_CTRACKMANIA_A = 0x00972EB8;

// B: BulbToys (global alternatif observé dans leur code)
// (Peut dépendre de la build, mais utile en fallback)
constexpr uintptr_t O_CTRACKMANIA_B = 0x0096A2A4;

// ===== Other offsets (tes valeurs actuelles) =====
constexpr uintptr_t PROFILE_OFFSET = 0x168;
constexpr uintptr_t LOGIN_OFFSET = 0x34;

constexpr uintptr_t MENUMGR_OFFSET = 0x194;
constexpr uintptr_t GAMEMENU_OFFSET = 0x788;
constexpr uintptr_t FRAMES_OFFSET = 0x68;

// ===== Functions =====
// CMwId_GetName (BulbToys RVA)
constexpr uintptr_t RVA_CMwId_GetName = 0x00535660;
