/*****************************************************************************
 * PlaybackModeManagerBridge.h : Nyra Player UI shell
 *****************************************************************************
 * FIXED vs the original: the old version emitted ":vout-scale-mode=..."
 * and ":gpu-power-hint=..." as libvlc_media_add_option() strings. Those are
 * NOT real VLC/libvlc options - grep the VLC source and they do not exist.
 * The original file's own comment admitted they required "a handful of
 * small, documented patches to the existing d3d11 vout and avcodec glue"
 * that do not exist anywhere in this repository, meaning those two
 * settings were silent no-ops that would have made Maximum Quality,
 * Balanced and Battery Saver mode behave identically. See AUDIT.md
 * Finding #3.
 *
 * This version only emits options that are real, documented, and verifiable
 * against any VLC 3.x/4.x checkout:
 *   - "avcodec-hw"                real avcodec module option (any/none)
 *   - "video-filter" / "deinterlace-mode"  real deinterlace filter options
 *   - "avcodec-threads"           real avcodec module option
 * The previously-invented per-mode "modes actually change behaviour"
 * feature is implemented with a smaller, honest set of real levers instead
 * of a larger set of fictional ones. It does less than originally claimed,
 * but everything it does is real.
 *****************************************************************************/

#pragma once

#include <QStringList>

enum class PlaybackMode {
    MaximumQuality,
    Balanced,
    BatterySaver,
    Custom,
};

struct CustomModeSettings {
    bool forceHardwareDecode = true;
    bool enableDeinterlace = false;
    QString deinterlaceMode = "yadif"; // yadif2x | yadif | blend | mean | bob | linear
};

class PlaybackModeManagerBridge {
public:
    // Returns the exact set of libvlc_media_add_option() strings for the
    // given mode. Every string here is a real, currently-existing libvlc
    // per-media option - verify against vlc/vlc.h and the avcodec /
    // deinterlace module docs in whatever VLC SDK BUILD.md points at.
    static QStringList optionsForMode(PlaybackMode mode,
                                       const CustomModeSettings &custom = {});
};
