#include "AccountStore.h"

#include <QSettings>
#include <QUuid>

namespace {
QString makeId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
}

AccountStore& AccountStore::instance() {
    static AccountStore s;
    return s;
}

AccountStore::AccountStore() {
    loadAll();
    if (m_accounts.isEmpty()) migrateLegacy();
}

void AccountStore::loadAll() {
    m_servers.clear();
    m_accounts.clear();

    QSettings s;
    const int sn = s.beginReadArray(QStringLiteral("servers"));
    for (int i = 0; i < sn; ++i) {
        s.setArrayIndex(i);
        ServerEntry e;
        e.id = s.value(QStringLiteral("id")).toString();
        e.name = s.value(QStringLiteral("name")).toString();
        e.url = s.value(QStringLiteral("url")).toUrl();
        if (!e.id.isEmpty()) m_servers.append(e);
    }
    s.endArray();

    const int an = s.beginReadArray(QStringLiteral("accounts"));
    for (int i = 0; i < an; ++i) {
        s.setArrayIndex(i);
        AccountEntry a;
        a.id = s.value(QStringLiteral("id")).toString();
        a.serverId = s.value(QStringLiteral("serverId")).toString();
        a.username = s.value(QStringLiteral("username")).toString();
        a.userId = s.value(QStringLiteral("userId")).toString();
        a.token = s.value(QStringLiteral("token")).toString();
        if (!a.id.isEmpty()) m_accounts.append(a);
    }
    s.endArray();
}

void AccountStore::saveServers() {
    QSettings s;
    s.beginWriteArray(QStringLiteral("servers"));
    for (int i = 0; i < m_servers.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue(QStringLiteral("id"), m_servers[i].id);
        s.setValue(QStringLiteral("name"), m_servers[i].name);
        s.setValue(QStringLiteral("url"), m_servers[i].url);
    }
    s.endArray();
}

void AccountStore::saveAccounts() {
    QSettings s;
    s.beginWriteArray(QStringLiteral("accounts"));
    for (int i = 0; i < m_accounts.size(); ++i) {
        s.setArrayIndex(i);
        s.setValue(QStringLiteral("id"), m_accounts[i].id);
        s.setValue(QStringLiteral("serverId"), m_accounts[i].serverId);
        s.setValue(QStringLiteral("username"), m_accounts[i].username);
        s.setValue(QStringLiteral("userId"), m_accounts[i].userId);
        s.setValue(QStringLiteral("token"), m_accounts[i].token);
    }
    s.endArray();
}

QList<ServerEntry> AccountStore::servers() const { return m_servers; }
QList<AccountEntry> AccountStore::accounts() const { return m_accounts; }

QList<AccountEntry> AccountStore::accountsForServer(const QString& serverId) const {
    QList<AccountEntry> out;
    for (const auto& a : m_accounts)
        if (a.serverId == serverId) out.append(a);
    return out;
}

ServerEntry AccountStore::server(const QString& id) const {
    for (const auto& s : m_servers) if (s.id == id) return s;
    return {};
}

AccountEntry AccountStore::account(const QString& id) const {
    for (const auto& a : m_accounts) if (a.id == id) return a;
    return {};
}

QString AccountStore::addServer(const QString& name, const QUrl& url) {
    ServerEntry e;
    e.id = makeId();
    e.name = name.isEmpty() ? url.host() : name;
    e.url = url;
    m_servers.append(e);
    saveServers();
    emit changed();
    return e.id;
}

void AccountStore::updateServer(const ServerEntry& s) {
    for (auto& e : m_servers) {
        if (e.id == s.id) {
            e = s;
            saveServers();
            emit changed();
            return;
        }
    }
}

void AccountStore::removeServer(const QString& id) {
    bool changedAny = false;
    for (int i = m_servers.size() - 1; i >= 0; --i) {
        if (m_servers[i].id == id) {
            m_servers.removeAt(i);
            changedAny = true;
        }
    }
    // Cascade-remove the accounts that lived on the dropped server so the
    // accounts list never contains dangling serverId references.
    for (int i = m_accounts.size() - 1; i >= 0; --i) {
        if (m_accounts[i].serverId == id) {
            m_accounts.removeAt(i);
            changedAny = true;
        }
    }
    if (changedAny) {
        saveServers();
        saveAccounts();
        // If the active account belonged to the removed server, clear it.
        const QString cur = currentAccountId();
        if (!cur.isEmpty() && account(cur).id.isEmpty())
            setCurrentAccountId(QString());
        emit changed();
    }
}

QString AccountStore::addAccount(const AccountEntry& a) {
    AccountEntry e = a;
    if (e.id.isEmpty()) e.id = makeId();
    m_accounts.append(e);
    saveAccounts();
    emit changed();
    return e.id;
}

void AccountStore::updateAccount(const AccountEntry& a) {
    for (auto& e : m_accounts) {
        if (e.id == a.id) {
            e = a;
            saveAccounts();
            emit changed();
            return;
        }
    }
}

void AccountStore::removeAccount(const QString& id) {
    for (int i = m_accounts.size() - 1; i >= 0; --i) {
        if (m_accounts[i].id == id) {
            m_accounts.removeAt(i);
            saveAccounts();
            if (currentAccountId() == id) setCurrentAccountId(QString());
            emit changed();
            return;
        }
    }
}

QString AccountStore::currentAccountId() const {
    return QSettings().value(QStringLiteral("currentAccountId")).toString();
}

void AccountStore::setCurrentAccountId(const QString& id) {
    QSettings s;
    if (id.isEmpty()) s.remove(QStringLiteral("currentAccountId"));
    else s.setValue(QStringLiteral("currentAccountId"), id);
    emit changed();
}

AccountEntry AccountStore::currentAccount() const {
    const QString id = currentAccountId();
    if (id.isEmpty()) return {};
    return account(id);
}

void AccountStore::migrateLegacy() {
    QSettings s;
    const QString srv = s.value(QStringLiteral("server")).toString();
    const QString uid = s.value(QStringLiteral("userId")).toString();
    const QString tok = s.value(QStringLiteral("token")).toString();
    const QString user = s.value(QStringLiteral("username")).toString();
    if (srv.isEmpty() || uid.isEmpty() || tok.isEmpty()) return;

    const QUrl url(srv);
    const QString srvId = addServer(url.host(), url);
    AccountEntry a;
    a.serverId = srvId;
    a.username = user;
    a.userId = uid;
    a.token = tok;
    const QString accId = addAccount(a);
    setCurrentAccountId(accId);
}
