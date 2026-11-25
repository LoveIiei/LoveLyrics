// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <QObject>
#include <QVariantList>

// One running copy per session, via the D-Bus name org.lovelyrics.App. A second launch
// asks the first to show its window (or to quit, with --quit) and exits.
class SingleInstance : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.lovelyrics.App")

public:
    static constexpr const char *ObjectPath = "/org/lovelyrics/App";

    explicit SingleInstance(QObject *parent = nullptr);

    // Claims the bus name. False if another instance already owns it. Without a
    // session bus there is nothing to coordinate, so this returns true.
    bool registerPrimary();

    // Calls a method on the running instance. False if none answers.
    static bool callRunning(const QString &method, const QVariantList &args = {});

public slots:
    // token: XDG activation token of the launch, so the compositor lets us take focus.
    Q_SCRIPTABLE void Activate(const QString &token);
    Q_SCRIPTABLE void Quit();

signals:
    void activateRequested(const QString &token);
    void quitRequested();
};
