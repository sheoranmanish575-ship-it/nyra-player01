#include "PowerManager.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

PowerManager::PowerManager(QObject *parent) : QObject(parent) {}

PowerManager::~PowerManager()
{
    applyHighResTimer(false); // never leave the process holding a high-res timer on exit
}

void PowerManager::onPlaybackStateChanged(bool isPlaying)
{
    m_playing = isPlaying;
    // Smooth frame pacing benefits from the high-res timer while actually
    // playing; there is no benefit to holding it while paused/stopped, and
    // holding a 1ms system timer needlessly is a real, documented drain on
    // laptop idle power (it prevents deeper CPU C-states system-wide, not
    // just for this process).
    applyHighResTimer(isPlaying);
}

void PowerManager::onWindowMinimizedChanged(bool minimized)
{
    m_minimized = minimized;
}

int PowerManager::suggestedStatsIntervalMs() const
{
    if (m_minimized)
        return 5000;      // window not visible: no point refreshing an overlay no one sees
    if (!m_playing)
        return 2000;       // paused/stopped: numbers are static, no need for 1 Hz
    return 1000;            // actively playing and visible: original 1 Hz cadence
}

void PowerManager::applyHighResTimer(bool enable)
{
#ifdef Q_OS_WIN
    if (enable == m_highResTimerActive)
        return; // avoid redundant timeBeginPeriod/timeEndPeriod churn
    if (enable)
        timeBeginPeriod(1);
    else
        timeEndPeriod(1);
    m_highResTimerActive = enable;
#else
    m_highResTimerActive = enable;
#endif
}
