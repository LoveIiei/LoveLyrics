// SPDX-License-Identifier: AGPL-3.0-or-later
#include "SingleInstance.h"

#include "AppInfo.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcInstance, "lovelyrics.instance")

SingleInstance::SingleInstance(QObject *parent)
    : QObject(parent)
{
}

bool SingleInstance::registerPrimary()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return true;
    const QString name = QString::fromLatin1(AppInfo::DBusName);
    const auto reply = bus.interface()->registerService(name, QDBusConnectionInterface::DontQueueService,
                                                        QDBusConnectionInterface::DontAllowReplacement);
    if (!reply.isValid() || reply.value() != QDBusConnectionInterface::ServiceRegistered) {
        qCInfo(lcInstance) << "Another instance owns" << name;
        return false;
    }
    bus.registerObject(QString::fromLatin1(ObjectPath), this, QDBusConnection::ExportScriptableSlots);
    return true;
}

bool SingleInstance::callRunning(const QString &method, const QVariantList &args)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return false;
    QDBusMessage msg = QDBusMessage::createMethodCall(QString::fromLatin1(AppInfo::DBusName),
                                                      QString::fromLatin1(ObjectPath),
                                                      QStringLiteral("org.lovelyrics.App"), method);
    msg.setArguments(args);
    const QDBusMessage reply = bus.call(msg, QDBus::Block, 3000);
    return reply.type() == QDBusMessage::ReplyMessage;
}

void SingleInstance::Activate(const QString &token)
{
    qCInfo(lcInstance) << "Activate requested";
    emit activateRequested(token);
}

void SingleInstance::Quit()
{
    qCInfo(lcInstance) << "Quit requested";
    emit quitRequested();
}
