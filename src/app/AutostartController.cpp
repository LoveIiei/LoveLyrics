// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AutostartController.h"

#include "AppInfo.h"
#include "Settings.h"
#include "WindowManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSaveFile>
#include <QStandardPaths>

Q_LOGGING_CATEGORY(lcAutostart, "lovelyrics.autostart")

AutostartController::AutostartController(MprisClient *player, WindowManager *windows, Settings *settings,
                                         bool resident, QObject *parent)
    : QObject(parent)
    , m_player(player)
    , m_windows(windows)
    , m_settings(settings)
    , m_resident(resident)
{
    connect(m_player, &MprisClient::statusChanged, this, &AutostartController::onStatusChanged);
    connect(m_player, &MprisClient::connectedChanged, this, &AutostartController::onConnectedChanged);
    connect(m_windows, &WindowManager::userHid, this, [this] {
        if (m_player->status() != MprisClient::Stopped)
            m_dismissed = true;
    });
}

void AutostartController::onStatusChanged(MprisClient::PlaybackStatus status)
{
    if (status == MprisClient::Stopped) {
        m_dismissed = false;
        return;
    }
    if (status != MprisClient::Playing || !m_resident)
        return;
    const QString mode = m_settings->autostartMode();
    if (mode == u"off" || m_windows->isVisible() || m_dismissed)
        return;
    qCInfo(lcAutostart) << "Playback started: showing the window" << mode;
    m_windows->show(mode == u"fullscreen");
}

void AutostartController::onConnectedChanged(bool connected)
{
    if (connected)
        return;
    m_dismissed = false;
    if (m_resident && m_settings->hideWhenPlayerQuits() && m_windows->isVisible()) {
        qCInfo(lcAutostart) << "Player quit: hiding the window";
        m_windows->hide();
    }
}

QString AutostartController::desktopFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/autostart/")
           + QString::fromLatin1(AppInfo::Id) + QStringLiteral(".desktop");
}

QString AutostartController::execCommand()
{
    const QString self = QCoreApplication::applicationFilePath();
    const QString installed = QStandardPaths::findExecutable(QString::fromLatin1(AppInfo::Id));
    if (!installed.isEmpty() && QFileInfo(installed).canonicalFilePath() == QFileInfo(self).canonicalFilePath())
        return QString::fromLatin1(AppInfo::Id);
    // A development build: use its full path, quoted for the desktop-entry Exec key.
    QString quoted = self;
    quoted.replace(QLatin1Char('\\'), QLatin1String("\\\\")).replace(QLatin1Char('"'), QLatin1String("\\\""));
    return self.contains(QLatin1Char(' ')) ? QLatin1Char('"') + quoted + QLatin1Char('"') : self;
}

QString AutostartController::desktopFileContents()
{
    return QStringLiteral("[Desktop Entry]\n"
                          "Type=Application\n"
                          "Name=%1 (background)\n"
                          "Comment=Shows synced lyrics when the music player starts playing\n"
                          "Exec=%2 --background\n"
                          "Icon=%3\n"
                          "Terminal=false\n"
                          "NoDisplay=true\n"
                          "X-KDE-autostart-after=panel\n"
                          "X-GNOME-Autostart-enabled=true\n")
        .arg(QString::fromLatin1(AppInfo::Name), execCommand(), QString::fromLatin1(AppInfo::Id));
}

bool AutostartController::startAtLogin() const
{
    return QFile::exists(desktopFilePath());
}

void AutostartController::setStartAtLogin(bool enabled)
{
    if (enabled == startAtLogin())
        return;
    const QString path = desktopFilePath();
    if (enabled) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QSaveFile f(path);
        if (!f.open(QIODevice::WriteOnly) || f.write(desktopFileContents().toUtf8()) < 0 || !f.commit()) {
            qCWarning(lcAutostart) << "Could not write" << path << f.errorString();
            return;
        }
        qCInfo(lcAutostart) << "Start at login enabled:" << path;
    } else {
        QFile::remove(path);
        qCInfo(lcAutostart) << "Start at login disabled";
    }
    emit startAtLoginChanged();
}
