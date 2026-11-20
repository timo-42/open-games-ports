/*
 * Chunk 044 (cod2_lnxded 1.3, 0x080ed5c0..0x080f14aa): per-player weapon and
 * offhand state machine, pmove/pml-style input and view-sway helpers.
 *
 * Layouts below are recovered from consistent offsets in this chunk only. Fields
 * not read here are padded. Names of fields are usage-derived; "fieldNNN" /
 * "fFieldNNN" / "stateTimeNNN" / "stateCountNNN" mark offsets whose meaning is not
 * established by this chunk. Struct sizes are lower bounds where noted.
 *
 * Out-of-chunk callees are declared with their fn_08xxxxxx placeholders (or the
 * merged name from symbols.json when present). Merge lane rewrites placeholders.
 */
#ifndef COD2_CHUNK_044_TYPES_H
#define COD2_CHUNK_044_TYPES_H

/* ---- playerState_s: server playerState (0x26a8 bytes); partial view, lower bound 0x5d8 ---- */
/* playerState_s: canonical, see cod2_canon.h */

/* Weapon state numbers (playerState_s.weaponState). READY and FIRING are named from
 * their transitions; the rest keep numeric labels because this chunk does not establish
 * their behavior beyond the transitions and timers it implements. */
enum weapon_state_e {
    WEAPSTATE_READY = 0,
    WEAPSTATE_1 = 1,
    WEAPSTATE_2 = 2,
    WEAPSTATE_FIRING = 3,
    WEAPSTATE_4 = 4,
    WEAPSTATE_5 = 5,
    WEAPSTATE_6 = 6,
    WEAPSTATE_7 = 7,
    WEAPSTATE_8 = 8,
    WEAPSTATE_9 = 9,
    WEAPSTATE_10 = 10,
    WEAPSTATE_11 = 11,
    WEAPSTATE_12 = 12,
    WEAPSTATE_13 = 13,
    WEAPSTATE_14 = 14,
    WEAPSTATE_15 = 15,
    WEAPSTATE_16 = 16,
    WEAPSTATE_17 = 17,
    WEAPSTATE_18 = 18,
    WEAPSTATE_19 = 19,
    WEAPSTATE_20 = 20,
    WEAPSTATE_21 = 21,
    WEAPSTATE_22 = 22
};

/* pmFlags bits touched by this chunk (set/clear sites in the functions noted). */
#define PMF_OFFHAND_ACTIVE      0x00000010u  /* set on offhand state entry, cleared on exit */
#define PMF_SWITCH_LOCKED       0x00000020u  /* blocks weapon selection (PM_Weapon_ChangeWeapon) */
#define PMF_FLAG_0x2000         0x00002000u  /* set/cleared in PM_Weapon_TryStartMelee */
#define PMF_FLAG_0x20000        0x00020000u  /* set/cleared by PM_Weapon_SetBinocularsActive/ClearFlag20000 */
#define PMF_FLAG_0x4000000      0x04000000u  /* read-only gate in weapon change paths */
#define PMF_FLAG_0x1000         0x00001000u  /* when set the whole weapon frame is skipped */

/* usercmd button bits used by this chunk (usercmd.buttons, pmove cmd at +0x08). */
#define CMD_BTN_ATTACK          0x00000001u
#define CMD_BTN_ALT             0x00000004u
#define CMD_BTN_OFFHAND_A       0x00010000u
#define CMD_BTN_OFFHAND_B       0x00020000u

/* ---- pmove_s: input wrapper. [0] = playerState, cmd at +4 (usercmd layout), oldcmd at +0x20. ---- */
/* pmove_s: canonical, see cod2_canon.h */

/* ---- pml_s: per-frame timing and axes. ---- */
/* pml_s: canonical, see cod2_canon.h */

/* ---- view_motion_s: view-sway state passed to the PM_Weapon_* view helpers. ---- */
struct view_motion_s {
    playerState_s *ps;          /* 0x00 */
    /* gameplay-fix(wpn_melee_ads_lean): these slots are floats in the original (fld/fstp in 0x080f0d74, 0x080f11b0); the client copies (gclient 0x2870 float[3], 0x287c) are raw dwords. They were int32 with numeric casts. */
    float elapsedMs;            /* 0x04: speed */
    float deltaMs;              /* 0x08: frame seconds */
    float offset[3];            /* 0x0c */
    float blend;                /* 0x18 */
    byte _pad_1c[0x34];
    int32 *phase;               /* 0x50: accumulator pointer */
};
COD2_ASSERT_OFFSET(view_motion_s, elapsedMs, 0x04);
COD2_ASSERT_OFFSET(view_motion_s, deltaMs, 0x08);
COD2_ASSERT_OFFSET(view_motion_s, offset, 0x0c);
COD2_ASSERT_OFFSET(view_motion_s, blend, 0x18);
COD2_ASSERT_OFFSET(view_motion_s, phase, 0x50);

/* ---- weaponDef_s: per-weapon definition record (partial; only fields used by this chunk) ---- */
/* weaponDef_s: canonical, see cod2_canon.h */

/* ---- dvar_s: console/dvar view; the value word is at +0x08 ---- */
/* dvar_s: canonical, see cod2_canon.h */

/* Weapon-kind scale cvars (float at +8) and globals used by this chunk. */

/* ---- out-of-chunk callees (placeholders; merge lane rewrites) ---- */
extern "C" {
}


#endif
