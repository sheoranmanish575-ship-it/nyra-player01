/*****************************************************************************
 * HwCaps.h : Nyra Player - hardware capability probing
 *****************************************************************************
 * Moved from the original src/modules/hw_probe/hwcaps.{c,h}. Logic is the
 * same DXGI/D3D11 probe; what changed is WHERE it runs: it is now compiled
 * straight into nyra.exe and called from the Qt app (see MainWindow.cpp),
 * instead of being a standalone file with no build target that linked it
 * to anything. See AUDIT.md Finding #1 for why the original "inject into
 * VLC's own build" plan was dropped.
 *****************************************************************************/

#ifndef NYRA_HWCAPS_H
#define NYRA_HWCAPS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NYRA_CODEC_H264 = 0,
    NYRA_CODEC_HEVC_MAIN,
    NYRA_CODEC_HEVC_MAIN10,
    NYRA_CODEC_AV1_MAIN,
    NYRA_CODEC_VP9_PROFILE0,
    NYRA_CODEC_VP9_PROFILE2_10BIT,
    NYRA_CODEC_COUNT
} nyra_codec_t;

typedef struct {
    char     adapter_description[128];
    uint32_t vendor_id;
    uint32_t device_id;
    uint32_t driver_revision;
    int      d3d11_feature_level;
    bool     supports_hdr10_output;
    double   max_refresh_rate_hz;
    bool     codec_hw_supported[NYRA_CODEC_COUNT];
    uint64_t probed_at_unix_time;
} nyra_hwcaps_t;

/* Runs the full DXGI/D3D11 probe. Returns false (with caps zeroed) if D3D11
 * is entirely unavailable - callers must treat that as "software decode
 * only", never as a fatal error. */
bool nyra_hwcaps_probe(nyra_hwcaps_t *out_caps);

bool nyra_hwcaps_load_cache(nyra_hwcaps_t *out_caps, const char *cache_path);
bool nyra_hwcaps_save_cache(const nyra_hwcaps_t *caps, const char *cache_path);

/* Cheap identity-only re-check + full probe on mismatch/first-run. See
 * HwCaps.c for exactly what "cheap" means now (FIXED - the original
 * implementation claimed a cheap path but always ran the full probe;
 * see AUDIT.md Finding #4). */
bool nyra_hwcaps_get(nyra_hwcaps_t *out_caps, const char *cache_path);

#ifdef __cplusplus
}
#endif

#endif /* NYRA_HWCAPS_H */
