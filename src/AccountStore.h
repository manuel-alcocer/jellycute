#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>

struct ServerEntry {
    QString id;
    QString name;
    QUrl url;
};

struct AccountEntry {
    QString id;
    QString serverId;
    QString username;
    QString userId;
    QString token;
};

// Persists the user's known Jellyfin servers, the per-server accounts (one
// access token per (server, user) pair), and which account is currently
// active. Storage is layered on top of QSettings using two QSettings arrays
// ("servers", "accounts") plus a scalar key ("currentAccountId").
//
// On first run after the upgrade, migrateLegacy() promotes the previously
// flat ("server"/"userId"/"token"/"username") credentials into a default
// server + account so the user doesn't have to re-login.
class AccountStore : public QObject {
    Q_OBJECT
public:
    static AccountStore& instance();

    QList<ServerEntry> servers() const;
    QList<AccountEntry> accounts() const;
    QList<AccountEntry> accountsForServer(const QString& serverId) const;

    ServerEntry server(const QString& id) const;
    AccountEntry account(const QString& id) const;

    // Mutations write through to QSettings and emit changed().
    QString addServer(const QString& name, const QUrl& url);
    void updateServer(const ServerEntry& s);
    void removeServer(const QString& id);   // also removes accounts on it.

    QString addAccount(const AccountEntry& a);
    void updateAccount(const AccountEntry& a);
    void removeAccount(const QString& id);

    QString currentAccountId() const;
    void setCurrentAccountId(const QString& id);   // empty clears.
    AccountEntry currentAccount() const;

    void migrateLegacy();

signals:
    void changed();

private:
    AccountStore();
    void loadAll();
    void saveServers();
    void saveAccounts();

    QList<ServerEntry> m_servers;
    QList<AccountEntry> m_accounts;
};
