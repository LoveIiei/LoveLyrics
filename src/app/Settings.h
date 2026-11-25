// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <QObject>
#include <QRect>
#include <QSettings>
#include <QStringList>

// ~/.config/lovelyrics/lovelyrics.conf. Every setter writes through immediately.
class Settings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString playerName READ playerName WRITE setPlayerName NOTIFY playerNameChanged)
    Q_PROPERTY(QStringList lyricSourceOrder READ lyricSourceOrder WRITE setLyricSourceOrder NOTIFY lyricSourceOrderChanged)
    Q_PROPERTY(int globalOffsetMs READ globalOffsetMs WRITE setGlobalOffsetMs NOTIFY globalOffsetMsChanged)
    Q_PROPERTY(QString style READ style WRITE setStyle NOTIFY styleChanged)
    Q_PROPERTY(bool showTranslation READ showTranslation WRITE setShowTranslation NOTIFY showTranslationChanged)
    Q_PROPERTY(bool showCoverInfo READ showCoverInfo WRITE setShowCoverInfo NOTIFY showCoverInfoChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY fontFamilyChanged)
    Q_PROPERTY(int fontWeight READ fontWeight WRITE setFontWeight NOTIFY fontWeightChanged)
    Q_PROPERTY(double fontScale READ fontScale WRITE setFontScale NOTIFY fontScaleChanged)
    Q_PROPERTY(QString backgroundMode READ backgroundMode WRITE setBackgroundMode NOTIFY backgroundModeChanged)
    Q_PROPERTY(int fpsCap READ fpsCap WRITE setFpsCap NOTIFY fpsCapChanged)
    Q_PROPERTY(bool karaokeFill READ karaokeFill WRITE setKaraokeFill NOTIFY karaokeFillChanged)
    Q_PROPERTY(QRect windowGeometry READ windowGeometry WRITE setWindowGeometry NOTIFY windowGeometryChanged)
    Q_PROPERTY(QString windowScreen READ windowScreen WRITE setWindowScreen NOTIFY windowScreenChanged)
    Q_PROPERTY(bool fullScreen READ fullScreen WRITE setFullScreen NOTIFY fullScreenChanged)
    Q_PROPERTY(QString autostartMode READ autostartMode WRITE setAutostartMode NOTIFY autostartModeChanged)
    Q_PROPERTY(QString autostartScreen READ autostartScreen WRITE setAutostartScreen NOTIFY autostartScreenChanged)
    Q_PROPERTY(bool hideWhenPlayerQuits READ hideWhenPlayerQuits WRITE setHideWhenPlayerQuits NOTIFY hideWhenPlayerQuitsChanged)

public:
    explicit Settings(QObject *parent = nullptr);
    // For tests: a settings file at a given path.
    explicit Settings(const QString &iniPath, QObject *parent = nullptr);

    QString fileName() const { return m_s.fileName(); }

    QString playerName() const;
    void setPlayerName(const QString &v);
    QStringList lyricSourceOrder() const;
    void setLyricSourceOrder(const QStringList &v);
    int globalOffsetMs() const;
    void setGlobalOffsetMs(int v);
    QString style() const;
    void setStyle(const QString &v);
    bool showTranslation() const;
    void setShowTranslation(bool v);
    bool showCoverInfo() const;
    void setShowCoverInfo(bool v);
    QString fontFamily() const;
    void setFontFamily(const QString &v);
    int fontWeight() const;
    void setFontWeight(int v);
    double fontScale() const;
    void setFontScale(double v);
    QString backgroundMode() const;
    void setBackgroundMode(const QString &v);
    int fpsCap() const;
    void setFpsCap(int v);
    bool karaokeFill() const;
    void setKaraokeFill(bool v);
    QRect windowGeometry() const;
    void setWindowGeometry(const QRect &v);
    QString windowScreen() const;
    void setWindowScreen(const QString &v);
    bool fullScreen() const;
    void setFullScreen(bool v);
    QString autostartMode() const; // "off", "window", "fullscreen"
    void setAutostartMode(const QString &v);
    QString autostartScreen() const;
    void setAutostartScreen(const QString &v);
    bool hideWhenPlayerQuits() const;
    void setHideWhenPlayerQuits(bool v);

    // Per-track lyric offsets, keyed by file path (or url / "artist - title").
    Q_INVOKABLE int trackOffsetMs(const QString &trackKey) const;
    Q_INVOKABLE void setTrackOffsetMs(const QString &trackKey, int ms);

    // Per-style tunables: styleValue("Flow", "blur", 1.0).
    Q_INVOKABLE QVariant styleValue(const QString &style, const QString &key, const QVariant &fallback) const;
    Q_INVOKABLE void setStyleValue(const QString &style, const QString &key, const QVariant &value);

    Q_INVOKABLE void sync() { m_s.sync(); }

signals:
    void playerNameChanged();
    void lyricSourceOrderChanged();
    void globalOffsetMsChanged();
    void styleChanged();
    void showTranslationChanged();
    void showCoverInfoChanged();
    void fontFamilyChanged();
    void fontWeightChanged();
    void fontScaleChanged();
    void backgroundModeChanged();
    void fpsCapChanged();
    void karaokeFillChanged();
    void windowGeometryChanged();
    void windowScreenChanged();
    void fullScreenChanged();
    void autostartModeChanged();
    void autostartScreenChanged();
    void hideWhenPlayerQuitsChanged();
    void styleValueChanged(const QString &style, const QString &key);

private:
    template<typename T>
    T get(const char *key, const T &fallback) const
    {
        return m_s.value(QLatin1String(key), QVariant::fromValue(fallback)).template value<T>();
    }
    template<typename T, typename Signal>
    void set(const char *key, const T &value, Signal signal)
    {
        if (get<T>(key, T{}) == value && m_s.contains(QLatin1String(key)))
            return;
        m_s.setValue(QLatin1String(key), QVariant::fromValue(value));
        emit(this->*signal)();
    }

    static QString trackKeyHash(const QString &key);

    mutable QSettings m_s;
};
