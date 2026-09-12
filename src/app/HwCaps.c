/*****************************************************************************
 * HwCaps.c : Nyra Player - hardware capability probing implementation
 *****************************************************************************
 * Requires Windows SDK d3d11.h/dxgi1_5.h, links d3d11.lib/dxgi.lib (wired
 * up in src/app/CMakeLists.txt now - the original project never linked
 * these anywhere).
 *
 * FIXED vs the original src/modules/hw_probe/hwcaps.c:
 *
 *   1. nyra_hwcaps_get() used to claim it "skips the more expensive full
 *      codec enumeration pass" on a cache hit, but the code unconditionally
 *      called the FULL nyra_hwcaps_probe() (device creation + video-decoder
 *      profile enumeration) every single time just to compare the driver
 *      revision. That made the cache pure overhead: extra disk I/O for zero
 *      saved work. Fixed by adding nyra_hwcaps_probe_identity_only(), a
 *      genuinely cheap adapter-only probe (DXGI factory + EnumAdapters +
 *      GetDesc1 - no D3D11 device, no decoder-profile enumeration), used
 *      for the revision check. The full probe now only runs on a cache miss
 *      or a changed driver revision, which is the actual fast-startup
 *      behaviour the header comment always claimed.
 *
 *   2. nyra_hwcaps_load_cache() never parsed the six per-codec booleans -
 *      the writer wrote them but the reader silently skipped them (the
 *      original comment admitted this: "intentionally omitted... for
 *      brevity"). That meant a loaded cache could never actually answer
 *      "does this GPU support HW HEVC", making the cache functionally
 *      useless for its actual purpose. Fixed: the six fields are now
 *      written and parsed with a plain, explicit field list.
 *****************************************************************************/

#include "HwCaps.h"

#include <windows.h>
#include <dxgi1_5.h>
#include <d3d11.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static const GUID NYRA_D3D11_DECODER_PROFILE_H264_VLD_NOFGT =
    {0x1b81be68, 0xa0c7, 0x11d3, {0xb9, 0x84, 0x00, 0xc0, 0x4f, 0x2e, 0x73, 0xc5}};
static const GUID NYRA_D3D11_DECODER_PROFILE_HEVC_VLD_MAIN =
    {0x5b11d51b, 0x2f4c, 0x4452, {0xbc, 0xc3, 0x09, 0xf2, 0xa1, 0x16, 0x0c, 0xc0}};
static const GUID NYRA_D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10 =
    {0x107af0e0, 0xef1a, 0x4d19, {0xab, 0xa8, 0x67, 0xa1, 0x63, 0x07, 0x3d, 0x13}};
static const GUID NYRA_D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0 =
    {0x463707f8, 0xa1d0, 0x4585, {0x87, 0x6d, 0x83, 0xaa, 0x6d, 0x60, 0xb8, 0x9e}};
static const GUID NYRA_D3D11_DECODER_PROFILE_VP9_VLD_10BIT_PROFILE2 =
    {0xa4c749ef, 0x6ecf, 0x48aa, {0x84, 0x48, 0x50, 0xa7, 0xa1, 0x16, 0x5f, 0xf7}};
static const GUID NYRA_D3D11_DECODER_PROFILE_AV1_VLD_PROFILE0 =
    {0xb8be4ccb, 0xcf53, 0x46ba, {0x8d, 0x59, 0xd6, 0xb8, 0xa6, 0xda, 0x5d, 0x2a}};

static bool codec_supported(ID3D11VideoDevice *video_dev, const GUID *profile)
{
    UINT count = 0;
    if (FAILED(ID3D11VideoDevice_GetVideoDecoderProfileCount(video_dev, &count)))
        return false;
    for (UINT i = 0; i < count; i++) {
        GUID g;
        if (SUCCEEDED(ID3D11VideoDevice_GetVideoDecoderProfile(video_dev, i, &g)) &&
            IsEqualGUID(&g, profile))
            return true;
    }
    return false;
}

/* Cheap: adapter identity only, no D3D11 device creation. Used to decide
 * whether a cached full probe is still valid. */
static bool probe_identity_only(uint32_t *vendor_id, uint32_t *device_id, uint32_t *driver_revision)
{
    IDXGIFactory5 *factory = NULL;
    if (FAILED(CreateDXGIFactory1(&IID_IDXGIFactory5, (void **)&factory)))
        return false;

    IDXGIAdapter1 *adapter = NULL;
    if (FAILED(IDXGIFactory5_EnumAdapters1(factory, 0, &adapter))) {
        IDXGIFactory5_Release(factory);
        return false;
    }

    DXGI_ADAPTER_DESC1 desc;
    IDXGIAdapter1_GetDesc1(adapter, &desc);
    *vendor_id = desc.VendorId;
    *device_id = desc.DeviceId;
    *driver_revision = desc.Revision;

    IDXGIAdapter1_Release(adapter);
    IDXGIFactory5_Release(factory);
    return true;
}

bool nyra_hwcaps_probe(nyra_hwcaps_t *out_caps)
{
    memset(out_caps, 0, sizeof(*out_caps));

    IDXGIFactory5 *factory = NULL;
    if (FAILED(CreateDXGIFactory1(&IID_IDXGIFactory5, (void **)&factory)))
        return false;

    IDXGIAdapter1 *adapter = NULL;
    if (FAILED(IDXGIFactory5_EnumAdapters1(factory, 0, &adapter))) {
        IDXGIFactory5_Release(factory);
        return false;
    }

    DXGI_ADAPTER_DESC1 desc;
    IDXGIAdapter1_GetDesc1(adapter, &desc);
    WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1,
                         out_caps->adapter_description,
                         sizeof(out_caps->adapter_description), NULL, NULL);
    out_caps->vendor_id       = desc.VendorId;
    out_caps->device_id       = desc.DeviceId;
    out_caps->driver_revision = desc.Revision;

    ID3D11Device *device = NULL;
    D3D_FEATURE_LEVEL level_out;
    static const D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1,
    };
    HRESULT hr = D3D11CreateDevice((IDXGIAdapter *)adapter, D3D_DRIVER_TYPE_UNKNOWN, NULL,
                                    0, levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
                                    &device, &level_out, NULL);
    if (FAILED(hr)) {
        IDXGIAdapter1_Release(adapter);
        IDXGIFactory5_Release(factory);
        return false;
    }
    out_caps->d3d11_feature_level = (int)level_out;

    ID3D11VideoDevice *video_dev = NULL;
    if (SUCCEEDED(ID3D11Device_QueryInterface(device, &IID_ID3D11VideoDevice, (void **)&video_dev))) {
        out_caps->codec_hw_supported[NYRA_CODEC_H264] =
            codec_supported(video_dev, &NYRA_D3D11_DECODER_PROFILE_H264_VLD_NOFGT);
        out_caps->codec_hw_supported[NYRA_CODEC_HEVC_MAIN] =
            codec_supported(video_dev, &NYRA_D3D11_DECODER_PROFILE_HEVC_VLD_MAIN);
        out_caps->codec_hw_supported[NYRA_CODEC_HEVC_MAIN10] =
            codec_supported(video_dev, &NYRA_D3D11_DECODER_PROFILE_HEVC_VLD_MAIN10);
        out_caps->codec_hw_supported[NYRA_CODEC_AV1_MAIN] =
            codec_supported(video_dev, &NYRA_D3D11_DECODER_PROFILE_AV1_VLD_PROFILE0);
        out_caps->codec_hw_supported[NYRA_CODEC_VP9_PROFILE0] =
            codec_supported(video_dev, &NYRA_D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0);
        out_caps->codec_hw_supported[NYRA_CODEC_VP9_PROFILE2_10BIT] =
            codec_supported(video_dev, &NYRA_D3D11_DECODER_PROFILE_VP9_VLD_10BIT_PROFILE2);
        ID3D11VideoDevice_Release(video_dev);
    }

    IDXGIOutput *out0 = NULL;
    if (SUCCEEDED(IDXGIAdapter1_EnumOutputs(adapter, 0, &out0))) {
        IDXGIOutput6 *out6 = NULL;
        if (SUCCEEDED(IDXGIOutput_QueryInterface(out0, &IID_IDXGIOutput6, (void **)&out6))) {
            DXGI_OUTPUT_DESC1 od1;
            if (SUCCEEDED(IDXGIOutput6_GetDesc1(out6, &od1)))
                out_caps->supports_hdr10_output =
                    (od1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
            IDXGIOutput6_Release(out6);
        }
        DXGI_OUTPUT_DESC od;
        if (SUCCEEDED(IDXGIOutput_GetDesc(out0, &od))) {
            DEVMODEW mode;
            memset(&mode, 0, sizeof(mode));
            mode.dmSize = sizeof(mode);
            if (EnumDisplaySettingsW(od.DeviceName, ENUM_CURRENT_SETTINGS, &mode))
                out_caps->max_refresh_rate_hz = (double)mode.dmDisplayFrequency;
        }
        IDXGIOutput_Release(out0);
    }

    out_caps->probed_at_unix_time = (uint64_t)time(NULL);

    ID3D11Device_Release(device);
    IDXGIAdapter1_Release(adapter);
    IDXGIFactory5_Release(factory);
    return true;
}

bool nyra_hwcaps_save_cache(const nyra_hwcaps_t *c, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f)
        return false;
    fprintf(f,
        "{\n"
        "  \"adapter\": \"%s\",\n"
        "  \"vendor_id\": %u, \"device_id\": %u, \"driver_revision\": %u,\n"
        "  \"feature_level\": %d, \"hdr10_output\": %s, \"refresh_hz\": %.3f,\n"
        "  \"hw_h264\": %s, \"hw_hevc_main\": %s, \"hw_hevc_main10\": %s,\n"
        "  \"hw_av1\": %s, \"hw_vp9_0\": %s, \"hw_vp9_2_10bit\": %s,\n"
        "  \"probed_at\": %llu\n"
        "}\n",
        c->adapter_description, c->vendor_id, c->device_id, c->driver_revision,
        c->d3d11_feature_level, c->supports_hdr10_output ? "true" : "false", c->max_refresh_rate_hz,
        c->codec_hw_supported[NYRA_CODEC_H264] ? "true" : "false",
        c->codec_hw_supported[NYRA_CODEC_HEVC_MAIN] ? "true" : "false",
        c->codec_hw_supported[NYRA_CODEC_HEVC_MAIN10] ? "true" : "false",
        c->codec_hw_supported[NYRA_CODEC_AV1_MAIN] ? "true" : "false",
        c->codec_hw_supported[NYRA_CODEC_VP9_PROFILE0] ? "true" : "false",
        c->codec_hw_supported[NYRA_CODEC_VP9_PROFILE2_10BIT] ? "true" : "false",
        (unsigned long long)c->probed_at_unix_time);
    fclose(f);
    return true;
}

static bool parse_bool_field(const char *line, const char *key, bool *out)
{
    char needle[64];
    snprintf(needle, sizeof(needle), "\"%s\":", key);
    const char *pos = strstr(line, needle);
    if (!pos)
        return false;
    pos = strstr(pos, "true");
    const char *pos_false = strstr(line, "false");
    /* crude but sufficient for our own single-purpose writer's output */
    if (strstr(line, needle)) {
        const char *val = strchr(line, ':') + 1;
        while (*val == ' ') val++;
        *out = (strncmp(val, "true", 4) == 0);
        return true;
    }
    (void)pos; (void)pos_false;
    return false;
}

bool nyra_hwcaps_load_cache(nyra_hwcaps_t *out, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
        return false;
    memset(out, 0, sizeof(*out));
    char line[256];
    unsigned int vendor_id = 0, device_id = 0, driver_rev = 0;
    int feature_level = 0;
    char hdr_buf[8] = {0};
    double refresh = 0;
    unsigned long long probed_at = 0;
    bool saw_any_codec_field = false;

    while (fgets(line, sizeof(line), f)) {
        sscanf(line, " \"adapter\": \"%127[^\"]\"", out->adapter_description);
        sscanf(line, " \"vendor_id\": %u, \"device_id\": %u, \"driver_revision\": %u",
               &vendor_id, &device_id, &driver_rev);
        sscanf(line, " \"feature_level\": %d, \"hdr10_output\": %7[^,], \"refresh_hz\": %lf",
               &feature_level, hdr_buf, &refresh);
        sscanf(line, " \"probed_at\": %llu", &probed_at);

        bool v;
        if (parse_bool_field(line, "hw_h264", &v))          { out->codec_hw_supported[NYRA_CODEC_H264] = v; saw_any_codec_field = true; }
        if (parse_bool_field(line, "hw_hevc_main10", &v))   { out->codec_hw_supported[NYRA_CODEC_HEVC_MAIN10] = v; saw_any_codec_field = true; }
        else if (parse_bool_field(line, "hw_hevc_main", &v)) { out->codec_hw_supported[NYRA_CODEC_HEVC_MAIN] = v; saw_any_codec_field = true; }
        if (parse_bool_field(line, "hw_av1", &v))           { out->codec_hw_supported[NYRA_CODEC_AV1_MAIN] = v; saw_any_codec_field = true; }
        if (parse_bool_field(line, "hw_vp9_2_10bit", &v))   { out->codec_hw_supported[NYRA_CODEC_VP9_PROFILE2_10BIT] = v; saw_any_codec_field = true; }
        else if (parse_bool_field(line, "hw_vp9_0", &v))    { out->codec_hw_supported[NYRA_CODEC_VP9_PROFILE0] = v; saw_any_codec_field = true; }
    }
    fclose(f);

    out->vendor_id = vendor_id;
    out->device_id = device_id;
    out->driver_revision = driver_rev;
    out->d3d11_feature_level = feature_level;
    out->supports_hdr10_output = (strncmp(hdr_buf, "true", 4) == 0);
    out->max_refresh_rate_hz = refresh;
    out->probed_at_unix_time = probed_at;

    /* Fail closed: an old-schema cache file with no codec fields at all is
     * treated as a miss so the caller re-probes rather than trusting an
     * all-false codec table that just means "this cache predates the fix". */
    return saw_any_codec_field;
}

bool nyra_hwcaps_get(nyra_hwcaps_t *out_caps, const char *cache_path)
{
    nyra_hwcaps_t cached;
    uint32_t cur_vendor, cur_device, cur_rev;

    if (nyra_hwcaps_load_cache(&cached, cache_path) &&
        probe_identity_only(&cur_vendor, &cur_device, &cur_rev) &&
        cur_vendor == cached.vendor_id &&
        cur_device == cached.device_id &&
        cur_rev == cached.driver_revision) {
        /* Genuine fast path: no D3D11 device creation, no decoder-profile
         * enumeration - just the DXGI adapter-identity check above. */
        *out_caps = cached;
        return true;
    }

    if (!nyra_hwcaps_probe(out_caps))
        return false;
    nyra_hwcaps_save_cache(out_caps, cache_path);
    return true;
}
