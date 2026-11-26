// SPDX-License-Identifier: AGPL-3.0-or-later
#include "MprisClient.h"

#include <QDBusArgument>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcMpris, "lovelyrics.mpris")

namespace {

const QString MprisPath = QStringLiteral("/org/mpris/MediaPlayer2");
const QString MprisPrefix = QStringLiteral("org.mpris.MediaPlayer2.");
const QString RootIface = QStringLiteral("org.mpris.MediaPlayer2");
const QString PlayerIface = QStringLiteral("org.mpris.MediaPlayer2.Player");
const QString PropsIface = QStringLiteral("org.freedesktop.DBus.Properties");

QVariant unwrap(const QVariant &v)
{
    if (v.metaType() == QMetaType::fromType<QDBusVariant>())
        return v.value<QDBusVariant>().variant();
    return v;
}

QVariantMap toMap(const QVariant &value)
{
    const QVariant v = unwrap(value);
    if (v.metaType() == QMetaType::fromType<QDBusArgument>())
        return qdbus_cast<QVariantMap>(v.value<QDBusArgument>());
    return v.toMap();
}

QStringList toStringList(const QVariant &value)
{
    const QVariant v = unwrap(value);
    if (v.metaType() == QMetaType::fromType<QDBusArgument>())
        return qdbus_cast<QStringList>(v.value<QDBusArgument>());
    if (v.metaType() == QMetaType::fromType<QString>())
        return v.toString().isEmpty() ? QStringList{} : QStringList{v.toString()};
    return v.toStringList();
}

QString toPathString(const QVariant &value)
{
    const QVariant v = unwrap(value);
    if (v.metaType() == QMetaType::fromType<QDBusObjectPath>())
        return v.value<QDBusObjectPath>().path();
    return v.toString();
}

QString serviceFor(const QString &name)
{
    if (name.startsWith(MprisPrefix))
        return name;
    return MprisPrefix + name;
}

} // namespace

MprisClient::MprisClient(QObject *parent)
    : MprisClient(QDBusConnection::sessionBus(), parent)
{
}

MprisClient::MprisClient(const QDBusConnection &bus, QObject *parent)
    : QObject(parent)
    , m_bus(bus)
{
    init();
}

MprisClient::~MprisClient()
{
    unsubscribe();
}

void MprisClient::init()
{
    qRegisterMetaType<TrackInfo>();
    m_clock.start();
    if (!m_bus.isConnected())
        qCWarning(lcMpris) << "No D-Bus session bus:" << m_bus.lastError().message();

    m_watcher = new QDBusServiceWatcher(this);
    m_watcher->setConnection(m_bus);
    m_watcher->setWatchMode(QDBusServiceWatcher::WatchForOwnerChange);
    connect(m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString &, const QString &, const QString &newOwner) {
                if (newOwner.isEmpty())
                    detach();
                else
                    attach();
            });

    m_pollTimer.setInterval(PollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout, this, [this] { requestPosition(false); });

    setPlayerName(QStringLiteral("strawberry"));
}

void MprisClient::setPlayerName(const QString &name)
{
    const QString trimmed = name.trimmed().isEmpty() ? QStringLiteral("strawberry") : name.trimmed();
    const QString service = serviceFor(trimmed);
    if (service == m_service)
        return;

    unsubscribe();
    detach();
    m_playerName = trimmed;
    m_service = service;
    m_watcher->setWatchedServices({m_service});
    subscribe();
    emit playerNameChanged();

    if (m_bus.isConnected() && m_bus.interface()->isServiceRegistered(m_service))
        attach();
}

void MprisClient::subscribe()
{
    if (m_subscribed || !m_bus.isConnected())
        return;
    // Connecting with the well-known name: QtDBus follows its owner, so this survives
    // the player restarting.
    m_bus.connect(m_service, MprisPath, PropsIface, QStringLiteral("PropertiesChanged"), this,
                  SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
    m_bus.connect(m_service, MprisPath, PlayerIface, QStringLiteral("Seeked"), this, SLOT(onSeeked(qlonglong)));
    m_subscribed = true;
}

void MprisClient::unsubscribe()
{
    if (!m_subscribed)
        return;
    m_bus.disconnect(m_service, MprisPath, PropsIface, QStringLiteral("PropertiesChanged"), this,
                     SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
    m_bus.disconnect(m_service, MprisPath, PlayerIface, QStringLiteral("Seeked"), this,
                     SLOT(onSeeked(qlonglong)));
    m_subscribed = false;
}

void MprisClient::attach()
{
    qCInfo(lcMpris) << "Player appeared:" << m_service;
    ++m_generation; // replies from a previous owner of the name are stale
    setConnected(true);
    fetchAll();
}

void MprisClient::detach()
{
    if (!m_connected)
        return;
    qCInfo(lcMpris) << "Player went away:" << m_service;
    ++m_generation;
    setConnected(false);
    m_identity.clear();
    if (m_status != Stopped) {
        m_status = Stopped;
        emit statusChanged(m_status);
    }
    if (!m_track.isEmpty()) {
        m_track = {};
        emit trackChanged(m_track);
        emit metadataChanged();
        emit artUrlChanged({});
    }
    m_canControl = m_canSeek = m_canGoNext = m_canGoPrevious = false;
    emit capabilitiesChanged();
    updatePolling();
}

void MprisClient::setConnected(bool connected)
{
    if (m_connected == connected)
        return;
    m_connected = connected;
    emit connectedChanged(connected);
}

void MprisClient::fetchAll()
{
    auto getAll = [this](const QString &iface, auto handler) {
        QDBusMessage msg = QDBusMessage::createMethodCall(m_service, MprisPath, PropsIface, QStringLiteral("GetAll"));
        msg << iface;
        auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(msg), this);
        const QString service = m_service;
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, handler, service](QDBusPendingCallWatcher *w) {
            QDBusPendingReply<QVariantMap> reply = *w;
            w->deleteLater();
            if (service != m_service || !m_connected)
                return; // the player changed (or went away) meanwhile
            if (reply.isError()) {
                qCWarning(lcMpris) << "GetAll failed:" << reply.error().message();
                return;
            }
            handler(reply.value());
        });
    };

    getAll(PlayerIface, [this](const QVariantMap &props) {
        applyPlayerProperties(props);
        requestPosition(true);
    });
    getAll(RootIface, [this](const QVariantMap &props) {
        m_identity = props.value(QStringLiteral("Identity")).toString();
        emit connectedChanged(m_connected);
    });
}

TrackInfo MprisClient::parseMetadata(const QVariantMap &md)
{
    TrackInfo t;
    t.trackId = toPathString(md.value(QStringLiteral("mpris:trackid")));
    t.url = unwrap(md.value(QStringLiteral("xesam:url"))).toString();
    t.title = unwrap(md.value(QStringLiteral("xesam:title"))).toString();
    t.artists = toStringList(md.value(QStringLiteral("xesam:artist")));
    t.album = unwrap(md.value(QStringLiteral("xesam:album"))).toString();
    t.artUrl = unwrap(md.value(QStringLiteral("mpris:artUrl"))).toString();
    t.lyricsText = unwrap(md.value(QStringLiteral("xesam:asText"))).toString();
    const QVariant length = unwrap(md.value(QStringLiteral("mpris:length")));
    t.lengthMs = length.isValid() ? length.toLongLong() / 1000 : -1;
    return t;
}

MprisClient::PlaybackStatus MprisClient::parseStatus(const QString &status)
{
    if (status == u"Playing")
        return Playing;
    if (status == u"Paused")
        return Paused;
    return Stopped;
}

void MprisClient::applyPlayerProperties(const QVariantMap &props)
{
    bool wantPosition = false;

    if (props.contains(QStringLiteral("Metadata"))) {
        const TrackInfo t = parseMetadata(toMap(props.value(QStringLiteral("Metadata"))));
        if (!t.sameTrack(m_track)) {
            ++m_generation;
            m_track = t;
            qCInfo(lcMpris) << "Track:" << t.artist() << "-" << t.title << t.url;
            // trackChanged covers the art too: artUrlChanged is only for art that arrives
            // later for the same track (otherwise every new track loaded its cover twice).
            emit trackChanged(m_track);
            emit metadataChanged();
            wantPosition = true;
        } else if (!(t == m_track)) {
            const bool artChanged = t.artUrl != m_track.artUrl;
            m_track = t;
            emit metadataChanged();
            if (artChanged)
                emit artUrlChanged(m_track.artUrl);
        }
    }

    if (props.contains(QStringLiteral("PlaybackStatus"))) {
        const auto status = parseStatus(unwrap(props.value(QStringLiteral("PlaybackStatus"))).toString());
        if (status != m_status) {
            m_status = status;
            emit statusChanged(m_status);
            wantPosition = true;
            updatePolling();
        }
    }

    if (props.contains(QStringLiteral("Rate"))) {
        const double rate = unwrap(props.value(QStringLiteral("Rate"))).toDouble();
        if (rate > 0 && !qFuzzyCompare(rate, m_rate)) {
            m_rate = rate;
            emit rateChanged(m_rate);
            wantPosition = true;
        }
    }

    bool capsChanged = false;
    auto updateCap = [&](const char *key, bool &field) {
        const QString k = QString::fromLatin1(key);
        if (!props.contains(k))
            return;
        const bool v = unwrap(props.value(k)).toBool();
        if (v != field) {
            field = v;
            capsChanged = true;
        }
    };
    updateCap("CanControl", m_canControl);
    updateCap("CanSeek", m_canSeek);
    updateCap("CanGoNext", m_canGoNext);
    updateCap("CanGoPrevious", m_canGoPrevious);
    if (capsChanged)
        emit capabilitiesChanged();

    if (wantPosition)
        requestPosition(true);
}

void MprisClient::onPropertiesChanged(const QString &interface, const QVariantMap &changed,
                                      const QStringList &invalidated)
{
    if (interface == PlayerIface) {
        applyPlayerProperties(changed);
        if (!invalidated.isEmpty())
            fetchAll();
    } else if (interface == RootIface && changed.contains(QStringLiteral("Identity"))) {
        m_identity = unwrap(changed.value(QStringLiteral("Identity"))).toString();
        emit connectedChanged(m_connected);
    }
}

void MprisClient::onSeeked(qlonglong positionUs)
{
    ++m_generation; // in-flight position reads are now stale
    emit positionSynced(positionUs / 1000, true);
}

void MprisClient::setPositionTracking(bool enabled)
{
    if (m_positionTracking == enabled)
        return;
    m_positionTracking = enabled;
    updatePolling();
    if (enabled)
        requestPosition(true);
}

void MprisClient::requestPosition(bool snap)
{
    if (!m_connected || !m_positionTracking)
        return;
    QDBusMessage msg = QDBusMessage::createMethodCall(m_service, MprisPath, PropsIface, QStringLiteral("Get"));
    msg << PlayerIface << QStringLiteral("Position");
    const quint64 generation = m_generation;
    const qint64 sentNs = m_clock.nsecsElapsed();
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, generation, sentNs, snap](QDBusPendingCallWatcher *w) {
        QDBusPendingReply<QDBusVariant> reply = *w;
        w->deleteLater();
        if (reply.isError() || generation != m_generation)
            return;
        double ms = double(reply.value().variant().toLongLong()) / 1000.0;
        // The player answered somewhere in the round trip: assume the middle.
        if (m_status == Playing)
            ms += double(m_clock.nsecsElapsed() - sentNs) / 2e6 * m_rate;
        emit positionSynced(qint64(ms), snap);
    });
}

void MprisClient::updatePolling()
{
    if (m_connected && m_status == Playing && m_positionTracking)
        m_pollTimer.start();
    else
        m_pollTimer.stop();
}

void MprisClient::callPlayer(const QString &method, const QVariantList &args)
{
    if (!m_connected)
        return;
    QDBusMessage msg = QDBusMessage::createMethodCall(m_service, MprisPath, PlayerIface, method);
    msg.setArguments(args);
    m_bus.asyncCall(msg);
}

void MprisClient::playPause()
{
    callPlayer(QStringLiteral("PlayPause"));
}

void MprisClient::next()
{
    callPlayer(QStringLiteral("Next"));
}

void MprisClient::previous()
{
    callPlayer(QStringLiteral("Previous"));
}

void MprisClient::setPositionMs(qint64 ms)
{
    if (m_track.trackId.isEmpty())
        return;
    callPlayer(QStringLiteral("SetPosition"),
               {QVariant::fromValue(QDBusObjectPath(m_track.trackId)), QVariant::fromValue(qlonglong(ms * 1000))});
}

void MprisClient::seekByMs(qint64 deltaMs)
{
    callPlayer(QStringLiteral("Seek"), {QVariant::fromValue(qlonglong(deltaMs * 1000))});
}
