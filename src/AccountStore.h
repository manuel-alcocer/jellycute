#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

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
    Q_INVOKABLE QString addServer(const QString& name, const QUrl& url);
    void updateServer(const ServerEntry& s);
    Q_INVOKABLE void removeServer(const QString& id);   // also removes accounts on it.

    QString addAccount(const AccountEntry& a);
    // QML-friendly variant of addAccount that takes the individual fields
    // instead of the AccountEntry struct.
    Q_INVOKABLE QString addAccountWith(const QString& serverId,
                                       const QString& username,
                                       const QString& userId,
                                       const QString& token);
    void updateAccount(const AccountEntry& a);
    Q_INVOKABLE void removeAccount(const QString& id);

    Q_INVOKABLE QString currentAccountId() const;
    Q_INVOKABLE void setCurrentAccountId(const QString& id);   // empty clears.
    AccountEntry currentAccount() const;

    // Search the registered servers for one whose URL matches `url`.
    // Returns the empty string when no match exists. Used by LoginPage to
    // avoid registering the same server twice when the user re-logs in.
    Q_INVOKABLE QString findServerIdByUrl(const QUrl& url) const;

    // QVariantMap views over the structs so QML can iterate them via
    // Repeater / ListView without registering Q_GADGETs. Keys mirror the
    // ServerEntry / AccountEntry field names.
    Q_INVOKABLE QVariantList serverList() const;
    Q_INVOKABLE QVariantList accountList() const;
    Q_INVOKABLE QVariantMap serverAsMap(const QString& id) const;
    Q_INVOKABLE QVariantMap accountAsMap(const QString& id) const;

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
