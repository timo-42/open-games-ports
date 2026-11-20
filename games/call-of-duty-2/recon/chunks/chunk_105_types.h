/*
 * chunk 105 types: GSC entity-method and script-function handlers (ScriptEntCmd_* / GScr_*)
 * recovered from .eh_frame FDE starts, scattered between 0x081116d8 .. 0x08114b14.
 * Layouts are partial: only fields these 86 functions touch are named, the rest is padding.
 * Offsets observed from disassembly of cod2_lnxded 1.3.
 */
#ifndef COD2_CHUNK_105_TYPES_H
#define COD2_CHUNK_105_TYPES_H

#include "cod2_types.h"
#include "cod2_canon.h"

/* ---- per-entity-class handler record (stride 0x28), only the first slot is used here ---- */
struct entity_handler_s {
    void (*use)(struct gentity_s *self, struct gentity_s *other, struct gentity_s *activator); /* 0x00 */
    uint8 pad04[0x24];
};
COD2_ASSERT_SIZE(struct entity_handler_s, 0x28);

#endif
