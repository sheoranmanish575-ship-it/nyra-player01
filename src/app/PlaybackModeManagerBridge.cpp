#include "PlaybackModeManagerBridge.h"

QStringList PlaybackModeManagerBridge::optionsForMode(PlaybackMode mode,
                                                       const CustomModeSettings &custom)
{
    switch (mode) {
    case PlaybackMode::MaximumQuality:
        return {
            ":avcodec-hw=any",
            ":video-filter=deinterlace",
            ":deinterlace-mode=yadif2x",   // highest-quality real deinterlace mode
        };
    case PlaybackMode::Balanced:
        return {
            ":avcodec-hw=any",             // prefer HW decode; no forced filters
        };
    case PlaybackMode::BatterySaver:
        return {
            ":avcodec-hw=any",             // HW decode is the single biggest real power win
            ":avcodec-threads=1",          // cap SW-fallback CPU burst if HW decode isn't available
        };
    case PlaybackMode::Custom: {
        QStringList opts;
        opts << QString(":avcodec-hw=%1").arg(custom.forceHardwareDecode ? "any" : "none");
        if (custom.enableDeinterlace) {
            opts << ":video-filter=deinterlace";
            opts << QString(":deinterlace-mode=%1").arg(custom.deinterlaceMode);
        }
        return opts;
    }
    }
    return {};
}
