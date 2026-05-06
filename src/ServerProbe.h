#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QUrl>

// Pings <url>/System/Info/Public and verifies the JSON identifies the host
// as a Jellyfin server (ProductName contains "Jellyfin"). Always emits
// finished() exactly once with online=true|false, regardless of the
// outcome — callers can render the LED in either state without bookkeeping
// timeouts themselves.
class ServerProbe : public QObject {
    Q_OBJECT
public:
    explicit ServerProbe(QObject* parent = nullptr);

    void probe(const QUrl& url);

signals:
    void finished(const QUrl& url, bool online, const QString& info);

private:
    QNetworkAccessManager m_nam;
};
