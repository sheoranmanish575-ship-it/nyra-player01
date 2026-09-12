/*****************************************************************************
 * PowerManager.h : Nyra Player - idle/UI-suppression power management
 *****************************************************************************
 * REPLACES src/modules/power/idle_manager.c.
 *
 * The original idle_manager.c was written as a VLC-core "interface" plugin
 * using vlc_player_t / vlc_player_AddListener and a made-up
 * "vlc_player_GetHWND()" accessor (its own comment called this name
 * "illustrative", i.e. not a real function - it would not have compiled).
 * More fundamentally, an interface plugin loaded into VLC's own internal
 * playlist/player object graph has no connection to a libvlc_media_player_t
 * that a separate host app created via the public libvlc API - see
 * AUDIT.md Finding #1.
 *
 * This class implements the same two real, event-driven power behaviours
 * the original comment described, but from inside the Qt process, where
 * the window-state and playback-state events it needs are actually
 * available:
 *   1. Ask Windows for high-resolution multimedia timers only while media
 *      is actually playing (timeBeginPeriod(1) / timeEndPeriod(1)) -
 *      exactly the original's intent, called directly instead of through
 *      a disconnected VLC plugin.
 *   2. Slow the ~1 Hz UI stats-overlay refresh down further while the
 *      window is minimized or playback is paused/stopped, since redrawing
 *      a hidden or static window on a timer is pure waste.
 *
 * No polling loops are introduced: both behaviours are driven by Qt event
 * callbacks (changeEvent / libvlc player-state) that MainWindow already
 * receives, not by a new timer that checks state repeatedly.
 *****************************************************************************/

#pragma once

#include <QObject>

class PowerManager : public QObject {
    Q_OBJECT
public:
    explicit PowerManager(QObject *parent = nullptr);
    ~PowerManager() override;

    // Call when libvlc reports Playing / Paused / Stopped / EndReached.
    void onPlaybackStateChanged(bool isPlaying);

    // Call from MainWindow::changeEvent() on QEvent::WindowStateChange.
    void onWindowMinimizedChanged(bool minimized);

    // Suggested interval (ms) for the UI stats-overlay timer given current
    // state - coarser while hidden/idle, unchanged while actively playing
    // and visible. MainWindow re-arms its QTimer with this value instead of
    // a second polling loop checking the same thing.
    int suggestedStatsIntervalMs() const;

private:
    void applyHighResTimer(bool enable);

    bool m_playing = false;
    bool m_minimized = false;
    bool m_highResTimerActive = false;
};
