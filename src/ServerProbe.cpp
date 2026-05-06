#include "ServerProbe.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

ServerProbe::ServerProbe(QObject* parent) : QObject(parent) {}

void ServerProbe::probe(const QUrl& url) {
    QUrl probeUrl = url;
    QString path = probeUrl.path();
    if (path.endsWith('/')) path.chop(1);
    probeUrl.setPath(path + QStringLiteral("/System/Info/Public"));

    QNetworkRequest req(probeUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_nam.get(req);

    // Hard timeout — Qt's default is generous; we want a snappy LED update
    // when the host is unreachable.
    auto* timeout = new QTimer(reply);
    timeout->setSingleShot(true);
    timeout->setInterval(4000);
    connect(timeout, &QTimer::timeout, reply, [reply]() {
        if (reply->isRunning()) reply->abort();
    });
    timeout->start();

    connect(reply, &QNetworkReply::finished, this, [this, reply, url]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit finished(url, false, reply->errorString());
            return;
        }
        const QByteArray body = reply->readAll();
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        if (!doc.isObject()) {
            emit finished(url, false, tr("Respuesta no JSON"));
            return;
        }
        const QJsonObject obj = doc.object();
        const QString product = obj.value(QStringLiteral("ProductName")).toString();
        const QString version = obj.value(QStringLiteral("Version")).toString();
        const QString srvName = obj.value(QStringLiteral("ServerName")).toString();
        if (!product.contains(QStringLiteral("Jellyfin"), Qt::CaseInsensitive)) {
            emit finished(url, false, tr("No es un servidor Jellyfin"));
            return;
        }
        const QString info = srvName.isEmpty()
                                 ? QStringLiteral("Jellyfin %1").arg(version)
                                 : QStringLiteral("%1 — Jellyfin %2")
                                       .arg(srvName, version);
        emit finished(url, true, info);
    });
}
