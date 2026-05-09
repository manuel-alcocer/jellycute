#include "AccountStore.h"
#include "BrowseModel.h"
#include "ItemDetailModel.h"
#include "JellyfinClient.h"
#include "MpvObject.h"
#include "PlaybackSession.h"
#include "Preferences.h"

#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSGRendererInterface>
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
    // OpenGL backend — pin it explicitly. AA_ShareOpenGLContexts isn't
    // strictly required without a Widgets GL context in the mix, but it's
    // free insurance against future hybrid setups.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QGuiApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QGuiApplication app(argc, argv);

    // libmpv requires the C numeric locale. QGuiApplication's constructor
    // resets it to the user's locale, so this must run *after* it.
    std::setlocale(LC_NUMERIC, "C");

    QCoreApplication::setOrganizationName("jellycute");
    QCoreApplication::setOrganizationDomain("jellycute.local");
    QCoreApplication::setApplicationName("jellycute");
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/icon.png")));

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
