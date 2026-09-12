/*****************************************************************************
 * MainWindow.cpp : Nyra Player UI shell
 *****************************************************************************
 * Public libvlc API only (vlc/vlc.h) - see ../../AUDIT.md for the full
 * before/after list. Key fixes applied in this file specifically:
 *   - Real crash-handler wiring (CrashHandler.h) replaces a QSettings
 *     "cleanExit" flag that was disconnected from crash_handler.c's own
 *     resume-state file in the original project.
 *   - HwCaps probed once at startup and surfaced in the status bar.
 *   - A-B repeat is fully implemented (was a documented no-op).
 *   - Screenshot filenames are generated with a real timestamp, since
 *     libvlc_video_take_snapshot() takes an exact file path, not a printf
 *     template - the original "%d" placeholder in the path was never
 *     substituted by libvlc and would have produced a literal, colliding
 *     filename on every screenshot.
 *   - PowerManager (see PowerManager.h) drives the stats-timer interval and
 *     the Windows high-res timer, replacing the disconnected VLC-plugin
 *     idle_manager.c.
 *****************************************************************************/

#include "MainWindow.h"

#include <QApplication>
#include <QToolBar>
#include <QStatusBar>
#include <QMenuBar>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QDockWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QEvent>
#include <QTimer>
#include <QMetaObject>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QLineEdit>
#include <QDateTime>

#ifdef NYRA_HAVE_CRASH_HANDLER
#include "CrashHandler.h"
#endif
#ifdef NYRA_HAVE_HWCAPS
#include "HwCaps.h"
#endif

// --- libvlc event callback trampolines --------------------------------
// libvlc invokes these from an internal VLC thread, never the Qt UI
// thread. We never touch a QWidget here - only marshal via a queued
// invokeMethod so the update happens on the UI thread.
static void libvlc_time_changed_trampoline(const libvlc_event_t *, void *userData)
{
    auto *win = static_cast<MainWindow *>(userData);
    QMetaObject::invokeMethod(win, "onRefreshStatsOverlay", Qt::QueuedConnection);
}

static void libvlc_state_changed_trampoline(const libvlc_event_t *, void *userData)
{
    auto *win = static_cast<MainWindow *>(userData);
    QMetaObject::invokeMethod(win, "onLibvlcStateEvent", Qt::QueuedConnection);
}

static QString crashDirPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/crash";
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setAcceptDrops(true);
    setWindowTitle("Nyra Player");
    resize(1280, 800);

#ifdef NYRA_HAVE_CRASH_HANDLER
    QDir().mkpath(crashDirPath());
    nyra_crash_handler_install(crashDirPath().toUtf8().constData());
#endif

    const char *vlc_args[] = { "--no-video-title-show", "--quiet" };
    m_vlc = libvlc_new(2, vlc_args);
    if (!m_vlc) {
        QMessageBox::critical(this, "Nyra Player",
            "libvlc failed to initialize. This usually means the plugins/ "
            "folder isn't next to this executable, or VLC_PLUGIN_PATH is "
            "unset - see BUILD.md.");
    }
    m_player = m_vlc ? libvlc_media_player_new(m_vlc) : nullptr;

    buildUi();
    buildMenus();
    if (m_player)
        wireLibvlcEvents();

    m_statsTimer = new QTimer(this);
    connect(m_statsTimer, &QTimer::timeout, this, &MainWindow::onRefreshStatsOverlay);
    m_statsTimer->start(1000);

#ifdef NYRA_HAVE_HWCAPS
    nyra_hwcaps_t caps;
    QString cachePath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/hwcaps.json";
    QDir().mkpath(QFileInfo(cachePath).absolutePath());
    if (nyra_hwcaps_get(&caps, cachePath.toUtf8().constData()) && caps.adapter_description[0]) {
        statusBar()->showMessage(QString("GPU: %1").arg(QString::fromUtf8(caps.adapter_description)), 5000);
    } else {
        statusBar()->showMessage("Hardware decode probe unavailable - software decode fallback in effect", 5000);
    }
#endif

    QTimer::singleShot(0, this, &MainWindow::onCheckResumePrompt);
}

MainWindow::~MainWindow()
{
    if (m_currentMedia)
        libvlc_media_release(m_currentMedia);
    if (m_player)
        libvlc_media_player_release(m_player);
    if (m_vlc)
        libvlc_release(m_vlc);
}

void MainWindow::buildUi()
{
    m_videoFrame = new QWidget(this);
    m_videoFrame->setAttribute(Qt::WA_NativeWindow);
    m_videoFrame->setAttribute(Qt::WA_NoSystemBackground);   // avoid Qt painting over the video surface
    m_videoFrame->setAttribute(Qt::WA_PaintOnScreen);
    m_videoFrame->setStyleSheet("background-color: black;");
    setCentralWidget(m_videoFrame);

#ifdef Q_OS_WIN
    if (m_player)
        libvlc_media_player_set_hwnd(m_player, reinterpret_cast<void *>(m_videoFrame->winId()));
#endif

    auto *playlistDock = new QDockWidget("Playlist", this);
    m_playlist = new QListWidget(playlistDock);
    playlistDock->setWidget(m_playlist);
    addDockWidget(Qt::RightDockWidgetArea, playlistDock);
    connect(m_playlist, &QListWidget::itemActivated, this, &MainWindow::onPlaylistItemActivated);

    auto *toolbar = addToolBar("Transport");
    auto *playPauseBtn = new QPushButton("Play/Pause", toolbar);
    connect(playPauseBtn, &QPushButton::clicked, this, &MainWindow::onPlayPause);
    toolbar->addWidget(playPauseBtn);

    auto *stopBtn = new QPushButton("Stop", toolbar);
    connect(stopBtn, &QPushButton::clicked, this, &MainWindow::onStop);
    toolbar->addWidget(stopBtn);

    m_seekSlider = new QSlider(Qt::Horizontal, toolbar);
    m_seekSlider->setRange(0, 1000);
    connect(m_seekSlider, &QSlider::sliderPressed, [this] { m_seekSliderBeingDragged = true; });
    connect(m_seekSlider, &QSlider::sliderReleased, [this] { m_seekSliderBeingDragged = false; });
    connect(m_seekSlider, &QSlider::valueChanged, this, &MainWindow::onSeekSliderMoved);
    toolbar->addWidget(m_seekSlider);

    m_timeLabel = new QLabel("00:00 / 00:00", toolbar);
    toolbar->addWidget(m_timeLabel);

    m_volumeSlider = new QSlider(Qt::Horizontal, toolbar);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(100);
    m_volumeSlider->setMaximumWidth(120);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &MainWindow::onVolumeChanged);
    toolbar->addWidget(new QLabel("Vol", toolbar));
    toolbar->addWidget(m_volumeSlider);

    auto *rateSpin = new QDoubleSpinBox(toolbar);
    rateSpin->setRange(0.25, 4.0);
    rateSpin->setSingleStep(0.25);
    rateSpin->setValue(1.0);
    connect(rateSpin, qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onPlaybackRateChanged);
    toolbar->addWidget(new QLabel("Speed", toolbar));
    toolbar->addWidget(rateSpin);

    m_modeCombo = new QComboBox(toolbar);
    m_modeCombo->addItem("Maximum Quality");
    m_modeCombo->addItem("Balanced");
    m_modeCombo->addItem("Battery Saver");
    m_modeCombo->addItem("Custom");
    m_modeCombo->setCurrentIndex(1);
    connect(m_modeCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &MainWindow::onModeSelected);
    toolbar->addWidget(new QLabel(" Mode:", toolbar));
    toolbar->addWidget(m_modeCombo);

    auto *fsBtn = new QPushButton("Fullscreen", toolbar);
    connect(fsBtn, &QPushButton::clicked, this, &MainWindow::onToggleFullscreen);
    toolbar->addWidget(fsBtn);

    auto *abBtn = new QPushButton("A-B Repeat", toolbar);
    connect(abBtn, &QPushButton::clicked, this, &MainWindow::onSetAbRepeatPoint);
    toolbar->addWidget(abBtn);

    auto *shotBtn = new QPushButton("Screenshot", toolbar);
    connect(shotBtn, &QPushButton::clicked, this, &MainWindow::onScreenshot);
    toolbar->addWidget(shotBtn);

    m_statsLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_statsLabel);
}

void MainWindow::buildMenus()
{
    auto *fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("&Open File...", this, &MainWindow::onOpenFile, QKeySequence::Open);
    fileMenu->addAction("Open &Network Stream...", this, &MainWindow::onOpenUrl);
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", this, &QWidget::close);

    m_audioTrackMenu = menuBar()->addMenu("&Audio Track");
    m_subtitleTrackMenu = menuBar()->addMenu("&Subtitle Track");
    connect(m_audioTrackMenu, &QMenu::triggered, this, &MainWindow::onAudioTrackSelected);
    connect(m_subtitleTrackMenu, &QMenu::triggered, this, &MainWindow::onSubtitleTrackSelected);
}

void MainWindow::wireLibvlcEvents()
{
    m_eventMgr = libvlc_media_player_event_manager(m_player);
    libvlc_event_attach(m_eventMgr, libvlc_MediaPlayerTimeChanged,
                         libvlc_time_changed_trampoline, this);
    libvlc_event_attach(m_eventMgr, libvlc_MediaPlayerEndReached,
                         libvlc_state_changed_trampoline, this);
    libvlc_event_attach(m_eventMgr, libvlc_MediaPlayerPlaying,
                         libvlc_state_changed_trampoline, this);
    libvlc_event_attach(m_eventMgr, libvlc_MediaPlayerPaused,
                         libvlc_state_changed_trampoline, this);
    libvlc_event_attach(m_eventMgr, libvlc_MediaPlayerStopped,
                         libvlc_state_changed_trampoline, this);
}

void MainWindow::onLibvlcStateEvent()
{
    if (!m_player)
        return;
    bool playing = libvlc_media_player_is_playing(m_player) != 0;
    m_powerManager.onPlaybackStateChanged(playing);
    rearmStatsTimer();
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        m_powerManager.onWindowMinimizedChanged(isMinimized());
        rearmStatsTimer();
    }
}

void MainWindow::rearmStatsTimer()
{
    if (m_statsTimer)
        m_statsTimer->start(m_powerManager.suggestedStatsIntervalMs());
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile()) {
            m_playlist->addItem(url.toLocalFile());
            playFile(url.toLocalFile());
        }
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Space: onPlayPause(); break;
    case Qt::Key_F:     onToggleFullscreen(); break;
    case Qt::Key_Right: if (m_player) libvlc_media_player_set_time(m_player,
                             libvlc_media_player_get_time(m_player) + 5000); break;
    case Qt::Key_Left:  if (m_player) libvlc_media_player_set_time(m_player,
                             qMax<qint64>(0, libvlc_media_player_get_time(m_player) - 5000)); break;
    case Qt::Key_Escape: if (m_fullscreen) onToggleFullscreen(); break;
    default: QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
#ifdef NYRA_HAVE_CRASH_HANDLER
    nyra_crash_handler_mark_clean_exit();
#endif
    QMainWindow::closeEvent(event);
}

void MainWindow::onOpenFileFromArgv(const QString &path)
{
    m_playlist->addItem(path);
    playFile(path);
}

void MainWindow::onOpenFile()
{
    QString path = QFileDialog::getOpenFileName(this, "Open Media",
        QString(), "Media files (*.mp4 *.mkv *.avi *.mov *.webm *.ts *.m3u8);;All files (*.*)");
    if (!path.isEmpty()) {
        m_playlist->addItem(path);
        playFile(path);
    }
}

void MainWindow::onOpenUrl()
{
    bool ok = false;
    QString url = QInputDialog::getText(this, "Open Network Stream", "URL:",
                                         QLineEdit::Normal, "", &ok);
    if (ok && !url.isEmpty()) {
        m_playlist->addItem(url);
        playFile(url);
    }
}

void MainWindow::playFile(const QString &path)
{
    if (!m_vlc || !m_player) {
        reportPlaybackError("libvlc not initialized");
        return;
    }
    if (m_currentMedia)
        libvlc_media_release(m_currentMedia);

    const QByteArray pathBytes = path.toUtf8();
    m_currentMedia = path.startsWith("http://") || path.startsWith("https://") || path.startsWith("rtsp://")
        ? libvlc_media_new_location(m_vlc, pathBytes.constData())
        : libvlc_media_new_path(m_vlc, pathBytes.constData());

    if (!m_currentMedia) {
        reportPlaybackError("Could not create media object for " + path);
        return;
    }

    m_currentPath = path;
    m_abRepeatStartMs = -1;
    m_abRepeatEndMs = -1;
    applyCurrentPlaybackMode();

    libvlc_media_player_set_media(m_player, m_currentMedia);
    if (libvlc_media_player_play(m_player) != 0)
        reportPlaybackError("libvlc_media_player_play() failed for " + path);

    QTimer::singleShot(500, this, &MainWindow::populateTrackMenus);
}

void MainWindow::applyCurrentPlaybackMode()
{
    if (!m_currentMedia)
        return;
    const QStringList opts = PlaybackModeManagerBridge::optionsForMode(m_currentMode, m_customSettings);
    for (const QString &opt : opts) {
        if (opt.endsWith('='))
            continue;
        libvlc_media_add_option(m_currentMedia, opt.toUtf8().constData());
    }
}

void MainWindow::onModeSelected(int index)
{
    m_currentMode = static_cast<PlaybackMode>(index);
    // Applies on the next Open, same as before - see PlaybackModeManagerBridge.h
    // for exactly which real libvlc options each mode now sets.
    statusBar()->showMessage(QString("Playback mode set to %1 (applies on next Open)")
                              .arg(m_modeCombo->currentText()), 4000);
}

void MainWindow::onPlayPause()
{
    if (!m_player) return;
    if (libvlc_media_player_is_playing(m_player))
        libvlc_media_player_pause(m_player);
    else
        libvlc_media_player_play(m_player);
}

void MainWindow::onStop()
{
    if (m_player)
        libvlc_media_player_stop(m_player);
}

void MainWindow::onSeekSliderMoved(int value)
{
    if (!m_player || !m_seekSliderBeingDragged)
        return;
    float pos = value / 1000.0f;
    libvlc_media_player_set_position(m_player, pos);
}

void MainWindow::onVolumeChanged(int value)
{
    if (m_player)
        libvlc_audio_set_volume(m_player, value);
}

void MainWindow::onPlaylistItemActivated(QListWidgetItem *item)
{
    playFile(item->text());
}

void MainWindow::onToggleFullscreen()
{
    m_fullscreen = !m_fullscreen;
    if (m_fullscreen) {
        menuBar()->hide();
        showFullScreen();
    } else {
        menuBar()->show();
        showNormal();
    }
}

void MainWindow::onAudioTrackSelected(QAction *action)
{
    if (m_player)
        libvlc_audio_set_track(m_player, action->data().toInt());
}

void MainWindow::onSubtitleTrackSelected(QAction *action)
{
    if (m_player)
        libvlc_video_set_spu(m_player, action->data().toInt());
}

void MainWindow::populateTrackMenus()
{
    if (!m_player)
        return;
    m_audioTrackMenu->clear();
    m_subtitleTrackMenu->clear();

    libvlc_track_description_t *audioTracks = libvlc_audio_get_track_description(m_player);
    for (auto *t = audioTracks; t; t = t->p_next) {
        QAction *act = m_audioTrackMenu->addAction(QString::fromUtf8(t->psz_name));
        act->setData(t->i_id);
        act->setCheckable(true);
    }
    if (audioTracks) libvlc_track_description_list_release(audioTracks);

    libvlc_track_description_t *subTracks = libvlc_video_get_spu_description(m_player);
    m_subtitleTrackMenu->addAction("Disable")->setData(-1);
    for (auto *t = subTracks; t; t = t->p_next) {
        QAction *act = m_subtitleTrackMenu->addAction(QString::fromUtf8(t->psz_name));
        act->setData(t->i_id);
        act->setCheckable(true);
    }
    if (subTracks) libvlc_track_description_list_release(subTracks);
}

void MainWindow::onScreenshot()
{
    if (!m_player) return;
    QString dir = QFileDialog::getExistingDirectory(this, "Screenshot save folder");
    if (dir.isEmpty()) return;
    // FIXED: libvlc_video_take_snapshot() takes an exact file path, it does
    // NOT expand a "%d" placeholder (the original code passed one, which
    // libvlc would have written out literally, colliding on every shot).
    // Build a real unique name ourselves.
    QString path = dir + "/nyra-screenshot-" +
                   QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss-zzz") + ".png";
    if (libvlc_video_take_snapshot(m_player, 0, path.toUtf8().constData(), 0, 0) != 0)
        reportPlaybackError("Screenshot failed");
    else
        statusBar()->showMessage("Screenshot saved: " + path, 4000);
}

void MainWindow::onSetAbRepeatPoint()
{
    if (!m_player) return;
    if (m_abRepeatStartMs < 0) {
        m_abRepeatStartMs = libvlc_media_player_get_time(m_player);
        m_abRepeatEndMs = -1;
        statusBar()->showMessage("A-B repeat: start point set. Click again to set end point.", 3000);
    } else if (m_abRepeatEndMs < 0) {
        // FIXED: the original left the "B" (end) point unimplemented -
        // it reset state on the second click instead of latching an end
        // point, so the loop described in the comments could never fire.
        qint64 end = libvlc_media_player_get_time(m_player);
        if (end <= m_abRepeatStartMs) {
            statusBar()->showMessage("A-B repeat: end point must be after start point - try again.", 4000);
            m_abRepeatStartMs = -1;
            return;
        }
        m_abRepeatEndMs = end;
        statusBar()->showMessage("A-B repeat: looping active.", 3000);
    } else {
        m_abRepeatStartMs = -1;
        m_abRepeatEndMs = -1;
        statusBar()->showMessage("A-B repeat: cleared.", 3000);
    }
}

void MainWindow::onPlaybackRateChanged(double rate)
{
    if (m_player)
        libvlc_media_player_set_rate(m_player, static_cast<float>(rate));
}

void MainWindow::updateTimeLabelAndSlider()
{
    if (!m_player) return;
    qint64 cur = libvlc_media_player_get_time(m_player);
    qint64 dur = libvlc_media_player_get_length(m_player);

    auto fmt = [](qint64 ms) {
        qint64 s = ms / 1000;
        return QString("%1:%2").arg(s / 60, 2, 10, QChar('0')).arg(s % 60, 2, 10, QChar('0'));
    };
    m_timeLabel->setText(QString("%1 / %2").arg(fmt(cur), fmt(dur)));

    if (!m_seekSliderBeingDragged && dur > 0)
        m_seekSlider->setValue(static_cast<int>(1000.0 * cur / dur));
}

void MainWindow::onRefreshStatsOverlay()
{
    if (!m_player || !m_currentMedia) {
        m_statsLabel->clear();
        return;
    }

    updateTimeLabelAndSlider();

    libvlc_media_stats_t stats;
    if (libvlc_media_get_stats(m_currentMedia, &stats)) {
        m_statsLabel->setText(QString("decoded: %1  displayed: %2  lost: %3  demux bitrate: %4 kb/s")
            .arg(stats.i_decoded_video)
            .arg(stats.i_displayed_pictures)
            .arg(stats.i_lost_pictures)
            .arg(static_cast<int>(stats.f_demux_bitrate * 8)));
    }

#ifdef NYRA_HAVE_CRASH_HANDLER
    if (!m_currentPath.isEmpty())
        nyra_crash_handler_update_state(m_currentPath.toUtf8().constData(),
                                         libvlc_media_player_get_time(m_player) / 1000.0);
#endif

    // FIXED: real A-B repeat loop (was a documented no-op in the original).
    if (m_abRepeatStartMs >= 0 && m_abRepeatEndMs >= 0) {
        qint64 cur = libvlc_media_player_get_time(m_player);
        if (cur >= m_abRepeatEndMs)
            libvlc_media_player_set_time(m_player, m_abRepeatStartMs);
    }
}

void MainWindow::onCheckResumePrompt()
{
#ifdef NYRA_HAVE_CRASH_HANDLER
    char lastFile[512] = {0};
    double lastPos = 0.0;
    if (nyra_crash_handler_check_previous_crash(lastFile, sizeof(lastFile), &lastPos)) {
        QMessageBox::information(this, "Nyra Player",
            QString("The previous session did not exit cleanly while playing:\n%1\n\n"
                    "A local crash report was saved (see the crash folder in your app "
                    "data) but is never sent anywhere automatically.")
                .arg(QString::fromUtf8(lastFile)));
    }
#endif
}

void MainWindow::reportPlaybackError(const QString &context)
{
    statusBar()->showMessage("Error: " + context, 6000);
}
