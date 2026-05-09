#include "AccountStore.h"
#include "BrowseModel.h"
#include "ItemDetailModel.h"
#include "JellyfinClient.h"
#include "MpvObject.h"
#include "PlaybackSession.h"
#include "Preferences.h"

#include <QApplication>
#include <QIcon>
#include <QPalette>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QStyleFactory>
#include <QSurfaceFormat>
#include <QUrl>
#include <qqml.h>
#include <locale>

int main(int argc, char** argv) {
    // Ask for a GL 3.3 (compatibility) context: libmpv's render API needs
    // ≥ 3.x and Qt's default 2.0 context produces "OpenGL error
    // INVALID_ENUM after creating texture" noise from libmpv's probe.
    {
        QSurfaceFormat fmt;
        fmt.setVersion(3, 3);
        fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
        fmt.setRenderableType(QSurfaceFormat::OpenGL);
        QSurfaceFormat::setDefaultFormat(fmt);
    }

    // Qt 6 defaults Qt Quick to RHI (Vulkan/Metal/D3D depending on platform).
    // MpvObject is a QQuickFramebufferObject which only works with the
    // OpenGL backend — pin it explicitly. AA_ShareOpenGLContexts is needed
    // because we're back to a QApplication (Widgets) and the QStyle
    // backend may create its own GL contexts for theme rendering paths.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    // Route the Qt Quick Controls 2 controls through qqc2-desktop-style
    // so QStyle (Oxygen / Breeze / Fusion) draws them. Must happen before
    // any QQmlApplicationEngine instantiation; setting it after is silently
    // ignored. The application style itself is picked once QApplication
    // is constructed below.
    QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));

    QApplication app(argc, argv);

    // libmpv requires the C numeric locale. QApplication's constructor
    // resets it to the user's locale, so this must run *after* it.
    std::setlocale(LC_NUMERIC, "C");

    QCoreApplication::setOrganizationName("jellycute");
    QCoreApplication::setOrganizationDomain("jellycute.local");
    QCoreApplication::setApplicationName("jellycute");
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icon.png")));

    // Pick Oxygen if available, falling back to Breeze and finally Fusion.
    // Oxygen is the visual the user explicitly wants for buttons / sliders /
    // combo boxes; on Plasma 6 it lives in the `oxygen` package via the
    // oxygen6.so QStyle plugin.
    {
        const QStringList prefer{
            QStringLiteral("Oxygen"),
            QStringLiteral("Breeze"),
            QStringLiteral("Fusion"),
        };
        const QStringList keys = QStyleFactory::keys();
        for (const QString& s : prefer) {
            if (keys.contains(s, Qt::CaseInsensitive)) {
                if (auto* style = QStyleFactory::create(s)) {
                    QApplication::setStyle(style);
                    break;
                }
            }
        }
    }

    // Carbon dark palette so Oxygen's gradients render in dark greys. The
    // hex values mirror the jelly tokens in qml/Main.qml — Kirigami.Theme
    // overrides drive the QML side, this drives whatever QStyle paints.
    {
        const QColor base("#0a0d14");
        const QColor altBase("#11151c");
        const QColor elevated("#161a23");
        const QColor text("#e6ecf3");
        const QColor textDim("#8a98ad");
        const QColor accent("#3daee9");
        const QColor accentText("#0a0d14");

        QPalette p;
        p.setColor(QPalette::Window,           base);
        p.setColor(QPalette::WindowText,       text);
        p.setColor(QPalette::Base,             altBase);
        p.setColor(QPalette::AlternateBase,    elevated);
        p.setColor(QPalette::Text,             text);
        p.setColor(QPalette::Button,           elevated);
        p.setColor(QPalette::ButtonText,       text);
        p.setColor(QPalette::BrightText,       QColor("#ffffff"));
        p.setColor(QPalette::Highlight,        accent);
        p.setColor(QPalette::HighlightedText,  accentText);
        p.setColor(QPalette::Link,             accent);
        p.setColor(QPalette::LinkVisited,      QColor("#7c5fde"));
        p.setColor(QPalette::ToolTipBase,      altBase);
        p.setColor(QPalette::ToolTipText,      text);
        p.setColor(QPalette::PlaceholderText,  textDim);
        p.setColor(QPalette::Disabled, QPalette::Text,       textDim);
        p.setColor(QPalette::Disabled, QPalette::WindowText, textDim);
        p.setColor(QPalette::Disabled, QPalette::ButtonText, textDim);
        QApplication::setPalette(p);
    }

    // Hydrate JellyfinClient from the persisted active account so the QML
    // shell can talk to the server immediately. If no account exists,
    // Main.qml's Component.onCompleted pushes LoginPage on top of HomePage.
    JellyfinClient client;
    auto& store = AccountStore::instance();
    AccountEntry current = store.currentAccount();
    if (current.id.isEmpty() && !store.accounts().isEmpty()) {
        // Active account got cleared (e.g. its server was removed) but other
        // accounts remain — fall back to the first one so the app still has
        // something to log in with.
        current = store.accounts().first();
        store.setCurrentAccountId(current.id);
    }
    if (!current.id.isEmpty()) {
        const ServerEntry srv = store.server(current.serverId);
        client.setServer(srv.url);
        client.setCredentials(current.userId, current.token);
    }

    BrowseModel viewsModel(&client);
    BrowseModel resumeModel(&client);
    PlaybackSession playback;
    playback.setClient(&client);
    Preferences preferences;

    qmlRegisterType<MpvObject>("Jellycute", 1, 0, "MpvObject");
    qmlRegisterType<BrowseModel>("Jellycute", 1, 0, "BrowseModel");
    qmlRegisterType<ItemDetailModel>("Jellycute", 1, 0, "ItemDetailModel");
    qmlRegisterUncreatableType<JellyfinClient>(
        "Jellycute", 1, 0, "JellyfinClient",
        QStringLiteral("Provided by the application as the 'jellyfin' "
                       "context property"));
    qmlRegisterUncreatableType<PlaybackSession>(
        "Jellycute", 1, 0, "PlaybackSession",
        QStringLiteral("Provided by the application as the 'playback' "
                       "context property"));
    qmlRegisterUncreatableType<AccountStore>(
        "Jellycute", 1, 0, "AccountStore",
        QStringLiteral("Provided by the application as the 'accountStore' "
                       "context property"));
    qmlRegisterUncreatableType<Preferences>(
        "Jellycute", 1, 0, "Preferences",
        QStringLiteral("Provided by the application as the 'preferences' "
                       "context property"));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("jellyfin", &client);
    engine.rootContext()->setContextProperty("viewsModel", &viewsModel);
    engine.rootContext()->setContextProperty("resumeModel", &resumeModel);
    engine.rootContext()->setContextProperty("playback", &playback);
    engine.rootContext()->setContextProperty("accountStore", &store);
    engine.rootContext()->setContextProperty("preferences", &preferences);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) return -1;
    return app.exec();
}
