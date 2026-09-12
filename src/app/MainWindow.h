/*****************************************************************************
 * MainWindow.h : Nyra Player UI shell (now the whole application)
 *****************************************************************************
 * FIXED vs the original (see AUDIT.md for the full list; summary here):
 *   - Added PowerManager integration (replaces the disconnected
 *     idle_manager.c VLC-plugin approach).
 *   - Added real crash-handler wiring (CrashHandler.h) instead of a
 *     second, disconnected QSettings "cleanExit" flag.
 *   - Added HwCaps probing at startup (was written but never called by
 *     anything in the original project).
 *   - A-B repeat is now a fully implemented feature, not a documented
 *     no-op stub.
 *   - changeEvent() added to detect minimize/restore for PowerManager.
 *****************************************************************************/
#pragma once

#include <QMainWindow>
#include <QListWidget>
#include <QSlider>
#include <QLabel>
#include <QComboBox>
#include <QMenu>
#include <vlc/vlc.h>

#include "PlaybackModeManagerBridge.h"
#include "PowerManager.h"

class QDragEnterEvent;
class QDropEvent;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;

public slots:
    void onOpenFileFromArgv(const QString &path);

private slots:
    void onOpenFile();
    void onOpenUrl();
    void onPlayPause();
    void onStop();
    void onSeekSliderMoved(int value);
    void onVolumeChanged(int value);
    void onPlaylistItemActivated(QListWidgetItem *item);
    void onToggleFullscreen();
    void onModeSelected(int index);
    void onAudioTrackSelected(QAction *action);
    void onSubtitleTrackSelected(QAction *action);
    void onScreenshot();
    void onSetAbRepeatPoint();
    void onPlaybackRateChanged(double rate);
    void onRefreshStatsOverlay();
    void onCheckResumePrompt();
    void onLibvlcStateEvent(); // fired via queued connection from the libvlc trampoline

private:
    void buildUi();
    void buildMenus();
    void wireLibvlcEvents();
    void playFile(const QString &path);
    void applyCurrentPlaybackMode();
    void populateTrackMenus();
    void updateTimeLabelAndSlider();
    void reportPlaybackError(const QString &context);
    void rearmStatsTimer();

    libvlc_instance_t       *m_vlc = nullptr;
    libvlc_media_player_t   *m_player = nullptr;
    libvlc_media_t          *m_currentMedia = nullptr;
    struct libvlc_event_manager_t *m_eventMgr = nullptr;

    QWidget      *m_videoFrame = nullptr;
    QListWidget  *m_playlist = nullptr;
    QSlider      *m_seekSlider = nullptr;
    QSlider      *m_volumeSlider = nullptr;
    QLabel       *m_timeLabel = nullptr;
    QLabel       *m_statsLabel = nullptr;
    QComboBox    *m_modeCombo = nullptr;
    QMenu        *m_audioTrackMenu = nullptr;
    QMenu        *m_subtitleTrackMenu = nullptr;
    QTimer       *m_statsTimer = nullptr;

    PlaybackMode  m_currentMode = PlaybackMode::Balanced;
    CustomModeSettings m_customSettings;
    PowerManager  m_powerManager;

    QString       m_currentPath;
    bool          m_seekSliderBeingDragged = false;
    qint64        m_abRepeatStartMs = -1;
    qint64        m_abRepeatEndMs = -1;
    bool          m_fullscreen = false;
};
