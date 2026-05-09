#include "AccountStore.h"
#include "JellyfinClient.h"
#include "LoginDialog.h"
#include "MainWindow.h"
#include "Theme.h"

#include <QApplication>
#include <QIcon>
#include <QStyleFactory>
#include <QSurfaceFormat>
#include <QUrl>
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

    QApplication app(argc, argv);

    // libmpv requires the C numeric locale. QApplication's constructor
    // resets it to the user's locale, so this must run *after* it.
    std::setlocale(LC_NUMERIC, "C");

    QCoreApplication::setOrganizationName("jellycute");
    QCoreApplication::setOrganizationDomain("jellycute.local");
    QCoreApplication::setApplicationName("jellycute");
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icon.png")));

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
