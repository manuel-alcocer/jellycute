#include "AccountStore.h"
#include "BrowseModel.h"
#include "ItemDetailModel.h"
#include "JellyfinClient.h"
#include "LoginDialog.h"
#include "MainWindow.h"
#include "MpvObject.h"
#include "PlaybackSession.h"
#include "Theme.h"

#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QStyleFactory>
#include <QSurfaceFormat>
#include <QUrl>
#include <qqml.h>
#include <locale>

int main(int argc, char** argv) {
    // Ask for a GL 3.3 (compatibility) context for the QOpenGLWidget that hosts
    // mpv's render API. Qt defaults to 2.0, which lacks features mpv probes
    // and produces "OpenGL error INVALID_ENUM after creating texture" noise.
    {
        QSurfaceFormat fmt;
        fmt.setVersion(3, 3);
        fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
        fmt.setRenderableType(QSurfaceFormat::OpenGL);
        QSurfaceFormat::setDefaultFormat(fmt);
    }

    // Qt 6 defaults Qt Quick to RHI (Vulkan/Metal/D3D depending on platform).
    // MpvObject is a QQuickFramebufferObject which only works with the
    // OpenGL backend — pin it explicitly. AA_ShareOpenGLContexts keeps the
    // SG render-thread context interoperable with widget GL contexts when
    // both subsystems coexist.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QApplication app(argc, argv);

    // libmpv requires the C numeric locale. QApplication's constructor
    // resets it to the user's locale, so this must run *after* it.
    std::setlocale(LC_NUMERIC, "C");

    QCoreApplication::setOrganizationName("jellycute");
    QCoreApplication::setOrganizationDomain("jellycute.local");
    QCoreApplication::setApplicationName("jellycute");
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icon.png")));

    // Phase 0/1 scaffolding: with JC_QML=1 boot the Kirigami shell instead
    // of the Widgets MainWindow. The two paths are mutually exclusive while
    // the QML port is in progress; the QML side reuses the same JellyfinClient
    // bootstrap (active account from AccountStore -> setServer/setCredentials)
    // so models in QML can talk to the server immediately.
    if (qEnvironmentVariableIsSet("JC_QML")) {
        JellyfinClient qmlClient;
        auto& store = AccountStore::instance();
        AccountEntry current = store.currentAccount();
        if (current.id.isEmpty() && !store.accounts().isEmpty()) {
            current = store.accounts().first();
            store.setCurrentAccountId(current.id);
        }
        if (!current.id.isEmpty()) {
            const ServerEntry srv = store.server(current.serverId);
            qmlClient.setServer(srv.url);
            qmlClient.setCredentials(current.userId, current.token);
        }
        // Phase 4 will add the QML login flow; for now an unauthenticated
        // start is allowed so the shell still opens.

        BrowseModel viewsModel(&qmlClient);
        BrowseModel resumeModel(&qmlClient);
        PlaybackSession playback;
        playback.setClient(&qmlClient);

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

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("jellyfin", &qmlClient);
        engine.rootContext()->setContextProperty("viewsModel", &viewsModel);
        engine.rootContext()->setContextProperty("resumeModel", &resumeModel);
        engine.rootContext()->setContextProperty("playback", &playback);
        engine.rootContext()->setContextProperty("accountStore", &store);
        engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
        if (engine.rootObjects().isEmpty()) return -1;
        return app.exec();
    }

    // Pin a Qt style with Breeze/Fusion as fallbacks; the Theme module then
    // applies the active theme's stylesheet + palette on top.
    {
        const QStringList prefer{"Oxygen", "Breeze", "Fusion"};
        for (const QString& s : prefer) {
            if (QStyleFactory::keys().contains(s, Qt::CaseInsensitive)) {
                if (auto* style = QStyleFactory::create(s)) {
                    QApplication::setStyle(style);
                    break;
                }
            }
        }
        Theme::apply();
    }

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
    } else {
        LoginDialog dlg(&client);
        if (dlg.exec() != QDialog::Accepted) return 0;
    }

    MainWindow w(&client);
    w.show();
    return app.exec();
}
