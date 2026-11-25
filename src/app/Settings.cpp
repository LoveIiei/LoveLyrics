// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Settings.h"

#include "lyrics/LyricsLoader.h"

#include <QCryptographicHash>
#include <QStandardPaths>

Settings::Settings(QObject *parent)
    : QObject(parent)
    , m_s(defaultPath(), QSettings::IniFormat)
{
}

Settings::Settings(const QString &iniPath, QObject *parent)
    : QObject(parent)
    , m_s(iniPath, QSettings::IniFormat)
{
}

QString Settings::defaultPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
           + QStringLiteral("/lovelyrics/lovelyrics.conf");
}

QString Settings::playerName() const { return get<QString>("player/name", QStringLiteral("strawberry")); }
void Settings::setPlayerName(const QString &v) { set("player/name", v, &Settings::playerNameChanged); }

QStringList Settings::lyricSourceOrder() const
{
    return get<QStringList>("lyrics/sourceOrder", LyricsLoader::DefaultSourceOrder);
}
void Settings::setLyricSourceOrder(const QStringList &v) { set("lyrics/sourceOrder", v, &Settings::lyricSourceOrderChanged); }

int Settings::globalOffsetMs() const { return get<int>("lyrics/globalOffsetMs", 0); }
void Settings::setGlobalOffsetMs(int v) { set("lyrics/globalOffsetMs", v, &Settings::globalOffsetMsChanged); }

QString Settings::style() const { return get<QString>("view/style", QStringLiteral("Flow")); }
void Settings::setStyle(const QString &v) { set("view/style", v, &Settings::styleChanged); }

bool Settings::showTranslation() const { return get<bool>("view/showTranslation", true); }
void Settings::setShowTranslation(bool v) { set("view/showTranslation", v, &Settings::showTranslationChanged); }

bool Settings::showCoverInfo() const { return get<bool>("view/showCoverInfo", true); }
void Settings::setShowCoverInfo(bool v) { set("view/showCoverInfo", v, &Settings::showCoverInfoChanged); }

QString Settings::fontFamily() const { return get<QString>("font/family", QStringLiteral("Noto Sans")); }
void Settings::setFontFamily(const QString &v) { set("font/family", v, &Settings::fontFamilyChanged); }

int Settings::fontWeight() const { return get<int>("font/weight", 700); }
void Settings::setFontWeight(int v) { set("font/weight", v, &Settings::fontWeightChanged); }

double Settings::fontScale() const { return get<double>("font/scale", 1.0); }
void Settings::setFontScale(double v) { set("font/scale", v, &Settings::fontScaleChanged); }

QString Settings::backgroundMode() const { return get<QString>("view/background", QStringLiteral("cover")); }
void Settings::setBackgroundMode(const QString &v) { set("view/background", v, &Settings::backgroundModeChanged); }

int Settings::fpsCap() const { return get<int>("view/fpsCap", 0); }
void Settings::setFpsCap(int v) { set("view/fpsCap", v, &Settings::fpsCapChanged); }

bool Settings::karaokeFill() const { return get<bool>("view/karaokeFill", true); }
void Settings::setKaraokeFill(bool v) { set("view/karaokeFill", v, &Settings::karaokeFillChanged); }

QRect Settings::windowGeometry() const { return get<QRect>("window/geometry", QRect()); }
void Settings::setWindowGeometry(const QRect &v) { set("window/geometry", v, &Settings::windowGeometryChanged); }

QString Settings::windowScreen() const { return get<QString>("window/screen", QString()); }
void Settings::setWindowScreen(const QString &v) { set("window/screen", v, &Settings::windowScreenChanged); }

bool Settings::fullScreen() const { return get<bool>("window/fullScreen", false); }
void Settings::setFullScreen(bool v) { set("window/fullScreen", v, &Settings::fullScreenChanged); }

QString Settings::autostartMode() const { return get<QString>("autostart/mode", QStringLiteral("window")); }
void Settings::setAutostartMode(const QString &v) { set("autostart/mode", v, &Settings::autostartModeChanged); }

QString Settings::autostartScreen() const { return get<QString>("autostart/screen", QString()); }
void Settings::setAutostartScreen(const QString &v) { set("autostart/screen", v, &Settings::autostartScreenChanged); }

bool Settings::hideWhenPlayerQuits() const { return get<bool>("autostart/hideWhenPlayerQuits", false); }
void Settings::setHideWhenPlayerQuits(bool v)
{
    set("autostart/hideWhenPlayerQuits", v, &Settings::hideWhenPlayerQuitsChanged);
}

QString Settings::trackKeyHash(const QString &key)
{
    // INI keys can't hold arbitrary paths; a short hash keeps them flat and safe.
    return QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex().left(20));
}

int Settings::trackOffsetMs(const QString &trackKey) const
{
    if (trackKey.isEmpty())
        return 0;
    return m_s.value(QStringLiteral("trackOffsets/") + trackKeyHash(trackKey), 0).toInt();
}

void Settings::setTrackOffsetMs(const QString &trackKey, int ms)
{
    if (trackKey.isEmpty())
        return;
    const QString key = QStringLiteral("trackOffsets/") + trackKeyHash(trackKey);
    if (ms == 0)
        m_s.remove(key);
    else
        m_s.setValue(key, ms);
}

QVariant Settings::styleValue(const QString &style, const QString &key, const QVariant &fallback) const
{
    return m_s.value(QStringLiteral("style.%1/%2").arg(style, key), fallback);
}

void Settings::setStyleValue(const QString &style, const QString &key, const QVariant &value)
{
    m_s.setValue(QStringLiteral("style.%1/%2").arg(style, key), value);
    emit styleValueChanged(style, key);
}
