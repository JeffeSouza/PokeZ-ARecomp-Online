// Exact validated module profiles. Unknown identities fail closed.
#pragma once
#include "rt.h"
#include <stdlib.h>
#define LUMIOSE_NATIVE_PROFILE_ENABLED 1
typedef struct {
    uint8_t build_id[32];
    uint32_t camera_query_returns[3];
} LumioseNativeProfile;
static const LumioseNativeProfile lumiose_native_profiles[] = {
    {{0x84,0x68,0x44,0x91,0x19,0x9a,0x6b,0x91,0x80,0x72,0x58,0x02,0x55,0x8b,0x75,0xd9,0xc0,0xa0,0xd0,0xc4,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x152b0,0x15320,0x15400}},
    {{0xd2,0xe9,0x4b,0x5e,0x81,0xe6,0xda,0x07,0x52,0x56,0xb5,0x15,0x95,0x2e,0x75,0xed,0x21,0x03,0xe3,0x2a,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x152b0,0x15320,0x15400}},
    {{0xfe,0xd1,0xe6,0x4f,0xbb,0x84,0x79,0xba,0xa7,0x8e,0x4f,0x57,0xaf,0xda,0x7f,0x46,0x8c,0x7d,0x4a,0xd2,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x152b0,0x15320,0x15400}},
    {{0x0f,0x59,0x5e,0xfe,0xd7,0xe8,0x26,0x86,0xfd,0x28,0x8a,0xd6,0x1b,0xec,0x0c,0x0f,0x68,0xf1,0xa4,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x1a300,0x1a370,0x1a450}},
};
// Written by the local preparer only after the downloaded NSO and its
// compatibility metadata pass hash, build-ID, import and instruction checks.
// The runtime binds the small prepared record to the actual loaded module.
static LumioseNativeProfile lumiose_native_prepared_profile;
static bool lumiose_native_prepared_valid;
static pthread_once_t lumiose_native_prepared_once = PTHREAD_ONCE_INIT;
static void lumiose_native_read_prepared_profile(void) {
    const char *path = getenv("LUMIOSE_NATIVE_PROFILE_PATH");
    if (!path || !*path) return;
    FILE *file = fopen(path, "rb");
    if (!file) return;
    uint8_t raw[45];
    size_t size = fread(raw, 1, sizeof raw, file);
    bool failed = ferror(file) != 0;
    fclose(file);
    if (failed || size != 44) return;
    uint8_t any = 0;
    for (unsigned i=0; i<32; ++i) any |= raw[i];
    if (!any) return;
    memcpy(lumiose_native_prepared_profile.build_id, raw, 32);
    for (unsigned i=0; i<3; ++i) {
        const uint8_t *p = raw + 32 + 4*i;
        uint32_t value = (uint32_t)p[0] | (uint32_t)p[1]<<8 |
                         (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
        if (!value || (value & 3)) return;
        lumiose_native_prepared_profile.camera_query_returns[i] = value;
    }
    lumiose_native_prepared_valid = true;
}
static inline const LumioseNativeProfile *lumiose_native_prepared_current(void) {
    pthread_once(&lumiose_native_prepared_once, lumiose_native_read_prepared_profile);
    if (!lumiose_native_prepared_valid ||
        memcmp(g_modules[2].build_id, lumiose_native_prepared_profile.build_id, 32)) return NULL;
    uint64_t start = g_modules[2].text_off;
    uint64_t end = start + g_modules[2].text_size;
    if (end <= start) return NULL;
    for (unsigned i=0; i<3; ++i) {
        uint64_t offset = lumiose_native_prepared_profile.camera_query_returns[i];
        if (offset < start || offset >= end) return NULL;
    }
    return &lumiose_native_prepared_profile;
}
static inline const LumioseNativeProfile *lumiose_native_profile_current(void) {
    if (!LUMIOSE_NATIVE_PROFILE_ENABLED || g_nmodules <= 2) return NULL;
    for (unsigned i=0; i<sizeof lumiose_native_profiles/sizeof lumiose_native_profiles[0]; ++i)
        if (!memcmp(g_modules[2].build_id, lumiose_native_profiles[i].build_id, 32))
            return &lumiose_native_profiles[i];
    return lumiose_native_prepared_current();
}
static inline bool lumiose_native_module(void) { return lumiose_native_profile_current() != NULL; }
static inline bool lumiose_native_caller(uint64_t lr) {
    return lumiose_native_module() && lr >= g_modules[2].base + g_modules[2].text_off &&
        lr < g_modules[2].base + g_modules[2].text_off + g_modules[2].text_size;
}
static inline bool lumiose_native_camera_query(uint64_t lr) {
    const LumioseNativeProfile *profile = lumiose_native_profile_current();
    if (!profile) return false;
    uint64_t offset = lr - g_modules[2].base;
    return offset == profile->camera_query_returns[0] || offset == profile->camera_query_returns[1] ||
           offset == profile->camera_query_returns[2];
}
