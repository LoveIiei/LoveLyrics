// SPDX-License-Identifier: AGPL-3.0-or-later
#include "WindowManager.h"

#include "AppController.h"
#include "FrameLimiter.h"
#include "Settings.h"

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QQmlApplicationEngine>
#include <QQuickWindow>

Q_LOGGING_CATEGORY(lcWindow, "lovelyrics.window")

WindowManager::WindowManager(AppController *controller, Settings *settings, Options options, QObject *parent)
    : QObject(parent)
    , m_controller(controller)
    , m_settings(settings)
    , m_options(options)
{
}

WindowManager::~WindowManager()
{
    // The engine (and window) go before the backend objects QML refers to.
    m_engine.reset();
}

bool WindowManager::isVisible() const
{
    return m_window && m_window->isVisible();
}

bool WindowManager::create()
{
    if (m_window)
        return true;
    qCInfo(lcWindow) << "Creating window";
    m_engine = std::make_unique<QQmlApplicationEngine>();
    m_controller->registerWithQml(m_engine.get());
    m_engine->setInitialProperties({{QStringLiteral("startOnDebugPage"), m_options.debugPage},
                                    {QStringLiteral("settingsOpen"), m_options.settingsOpen}});
    QObject::connect(m_engine.get(), &QQmlApplicationEngine::objectCreationFailed, qApp,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    m_engine->loadFromModule("LoveLyrics", "Main");

    const auto roots = m_engine->rootObjects();
    m_window = roots.isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(roots.first());
    if (!m_window) {
        qCCritical(lcWindow) << "Main.qml did not create a window";
        return false;
    }

    m_limiter = new FrameLimiter(m_window, this);
    m_limiter->setCap(m_settings->fpsCap());
    connect(m_settings, &Settings::fpsCapChanged, m_limiter, [this] { m_limiter->setCap(m_settings->fpsCap()); });

    connect(m_window, &QWindow::visibleChanged, this, [this](bool visible) {
        emit visibleChanged(visible);
        if (!visible && !m_programmaticHide) {
            qCInfo(lcWindow) << "Window hidden by the user";
            emit userHid();
        }
    });
    emit windowCreated(m_window);
    return true;
}

void WindowManager::show(bool fullScreen, const QString &activationToken)
{
    if (!create())
        return;
    // Qt's Wayland backend reads this when asking the compositor for focus.
    if (!activationToken.isEmpty())
        qputenv("XDG_ACTIVATION_TOKEN", activationToken.toUtf8());
    qCInfo(lcWindow) << "Showing window" << (fullScreen ? "full screen" : "");
    QMetaObject::invokeMethod(m_window, "present", Q_ARG(QVariant, fullScreen));
}

void WindowManager::hide()
{
    if (!m_window || !m_window->isVisible())
        return;
    qCInfo(lcWindow) << "Hiding window";
    saveState();
    m_programmaticHide = true;
    m_window->hide();
    m_programmaticHide = false;
}

void WindowManager::saveState()
{
    if (m_window)
        QMetaObject::invokeMethod(m_window, "saveGeometry");
}
