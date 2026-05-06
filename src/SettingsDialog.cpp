#include "SettingsDialog.h"
#include "AccountStore.h"
#include "BrowserWidget.h"
#include "JellyfinClient.h"
#include "MpvWidget.h"
#include "ServerProbe.h"
#include "Theme.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QStackedWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace {

// Filled coloured circle used as an "online/offline/unknown" status LED next
// to each server entry.
class StatusDot : public QWidget {
public:
    enum State { Unknown, Online, Offline };
    explicit StatusDot(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedSize(14, 14);
    }
    void setState(State s) { m_state = s; update(); }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QColor fill;
        switch (m_state) {
            case Online:  fill = QColor("#2ecc71"); break;
            case Offline: fill = QColor("#e74c3c"); break;
            default:      fill = QColor("#7f8a93"); break;
        }
        p.setBrush(fill);
        p.setPen(QPen(fill.darker(140), 1));
        const QRectF r(2, 2, 10, 10);
        p.drawEllipse(r);
        // Tiny highlight on top-left so the LED reads as glassy rather than
        // flat at small sizes.
        QColor hi = fill.lighter(160);
        hi.setAlpha(140);
        p.setPen(Qt::NoPen);
        p.setBrush(hi);
        p.drawEllipse(QRectF(3.5, 3, 4, 3));
    }
private:
    State m_state = Unknown;
};

class AddServerDialog : public QDialog {
public:
    AddServerDialog(QWidget* parent = nullptr,
                    const ServerEntry& initial = {})
        : QDialog(parent), m_editing(!initial.id.isEmpty()) {
        setWindowTitle(m_editing ? QObject::tr("Editar servidor")
                                  : QObject::tr("Añadir servidor"));
        auto* form = new QFormLayout;
        m_name = new QLineEdit(initial.name, this);
        m_name->setPlaceholderText(QObject::tr("Casa, Trabajo…"));
        m_url = new QLineEdit(initial.url.toString(), this);
        m_url->setPlaceholderText("https://jellyfin.example.com");
        form->addRow(QObject::tr("Nombre"), m_name);
        form->addRow(QObject::tr("URL"), m_url);

        auto* probeRow = new QHBoxLayout;
        m_probeBtn = new QPushButton(QObject::tr("Probar"), this);
        m_status = new QLabel(this);
        probeRow->addWidget(m_probeBtn);
        probeRow->addWidget(m_status, 1);

        auto* bb = new QDialogButtonBox(
            QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
        connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
        connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto* root = new QVBoxLayout(this);
        root->addLayout(form);
        root->addLayout(probeRow);
        root->addWidget(bb);

        m_probe = new ServerProbe(this);
        connect(m_probe, &ServerProbe::finished, this,
                [this](const QUrl&, bool online, const QString& info) {
                    m_probeBtn->setEnabled(true);
                    m_status->setText(
                        online ? QObject::tr("OK — %1").arg(info)
                               : QObject::tr("Sin respuesta — %1").arg(info));
                });
        connect(m_probeBtn, &QPushButton::clicked, this, [this]() {
            const QUrl u = normalisedUrl();
            if (!u.isValid() || u.host().isEmpty()) {
                m_status->setText(QObject::tr("URL no válida"));
                return;
            }
            m_status->setText(QObject::tr("Comprobando…"));
            m_probeBtn->setEnabled(false);
            m_probe->probe(u);
        });
    }
    QUrl normalisedUrl() const {
        QString s = m_url->text().trimmed();
        if (s.isEmpty()) return {};
        if (!s.startsWith("http://") && !s.startsWith("https://"))
            s = "http://" + s;
        while (s.endsWith('/')) s.chop(1);
        return QUrl(s);
    }
    QString name() const { return m_name->text().trimmed(); }
    bool editing() const { return m_editing; }
private:
    bool m_editing;
    QLineEdit* m_name;
    QLineEdit* m_url;
    QPushButton* m_probeBtn;
    QLabel* m_status;
    ServerProbe* m_probe;
};

class AddAccountDialog : public QDialog {
public:
    // accountIdToEdit empty => add mode; non-empty => edit-and-reauth mode.
    AddAccountDialog(QWidget* parent, JellyfinClient* probeClient,
                     const QString& accountIdToEdit = QString())
        : QDialog(parent), m_client(probeClient),
          m_editingId(accountIdToEdit) {
        const bool editing = !m_editingId.isEmpty();
        setWindowTitle(editing ? QObject::tr("Editar cuenta")
                                : QObject::tr("Añadir cuenta"));
        auto* form = new QFormLayout;
        m_servers = new QComboBox(this);
        for (const auto& s : AccountStore::instance().servers()) {
            const QString label = s.name.isEmpty()
                                      ? s.url.toString()
                                      : QStringLiteral("%1 (%2)")
                                            .arg(s.name, s.url.toString());
            m_servers->addItem(label, s.id);
        }
        m_user = new QLineEdit(this);
        m_pass = new QLineEdit(this);
        m_pass->setEchoMode(QLineEdit::Password);

        if (editing) {
            const AccountEntry a = AccountStore::instance().account(m_editingId);
            const int idx = m_servers->findData(a.serverId);
            if (idx >= 0) m_servers->setCurrentIndex(idx);
            m_user->setText(a.username);
            m_pass->setPlaceholderText(
                QObject::tr("Vuelve a introducir la contraseña"));
        }

        form->addRow(QObject::tr("Servidor"), m_servers);
        form->addRow(QObject::tr("Usuario"), m_user);
        form->addRow(QObject::tr("Contraseña"), m_pass);

        m_status = new QLabel(this);
        m_status->setStyleSheet("color: #c33;");

        m_loginBtn = new QPushButton(
            editing ? QObject::tr("Guardar") : QObject::tr("Conectar"), this);
        m_loginBtn->setDefault(true);
        auto* cancel = new QPushButton(QObject::tr("Cancelar"), this);
        auto* btnRow = new QHBoxLayout;
        btnRow->addStretch();
        btnRow->addWidget(cancel);
        btnRow->addWidget(m_loginBtn);

        auto* root = new QVBoxLayout(this);
        root->addLayout(form);
        root->addWidget(m_status);
        root->addLayout(btnRow);

        connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
        connect(m_loginBtn, &QPushButton::clicked, this,
                &AddAccountDialog::tryLogin);
        connect(m_client, &JellyfinClient::authenticated, this,
                &AddAccountDialog::onAuthenticated);
        connect(m_client, &JellyfinClient::authenticationFailed, this,
                &AddAccountDialog::onAuthFailed);
    }
    bool isValid() const { return m_servers->count() > 0; }
private:
    void tryLogin() {
        const QString srvId = m_servers->currentData().toString();
        if (srvId.isEmpty()) {
            m_status->setText(QObject::tr("Selecciona un servidor"));
            return;
        }
        const ServerEntry srv = AccountStore::instance().server(srvId);
        m_loginBtn->setEnabled(false);
        m_status->setText(QObject::tr("Autenticando…"));
        m_client->setServer(srv.url);
        m_client->authenticate(m_user->text(), m_pass->text());
    }
    void onAuthenticated() {
        const QString srvId = m_servers->currentData().toString();
        if (m_editingId.isEmpty()) {
            AccountEntry a;
            a.serverId = srvId;
            a.username = m_user->text();
            a.userId = m_client->userId();
            a.token = m_client->accessToken();
            AccountStore::instance().addAccount(a);
        } else {
            // Keep the same id so any pointer (e.g. currentAccountId) into
            // it stays valid; the rest of the fields are refreshed from the
            // successful re-auth.
            AccountEntry a = AccountStore::instance().account(m_editingId);
            a.serverId = srvId;
            a.username = m_user->text();
            a.userId = m_client->userId();
            a.token = m_client->accessToken();
            AccountStore::instance().updateAccount(a);
        }
        accept();
    }
    void onAuthFailed(const QString& msg) {
        m_loginBtn->setEnabled(true);
        m_status->setText(QObject::tr("Falló: %1").arg(msg));
    }

    JellyfinClient* m_client;
    QString m_editingId;
    QComboBox* m_servers;
    QLineEdit* m_user;
    QLineEdit* m_pass;
    QLabel* m_status;
    QPushButton* m_loginBtn;
};

}  // anonymous

SettingsDialog::SettingsDialog(BrowserWidget* browser, MpvWidget* player,
                               QWidget* parent)
    : QDialog(parent), m_browser(browser), m_player(player) {
    setWindowTitle(tr("Ajustes"));
    setObjectName(QStringLiteral("settingsDialog"));
    resize(720, 480);
    m_probe = new ServerProbe(this);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    auto* split = new QHBoxLayout;
    split->setSpacing(12);

    m_categories = new QListWidget(this);
    m_categories->setObjectName(QStringLiteral("settingsCategories"));
    m_categories->setFixedWidth(180);
    m_categories->setFrameShape(QFrame::NoFrame);
    new QListWidgetItem(tr("Conexiones"),   m_categories);
    new QListWidgetItem(tr("Bibliotecas"),  m_categories);
    new QListWidgetItem(tr("Reproducción"), m_categories);
    new QListWidgetItem(tr("Apariencia"),   m_categories);
    m_categories->setCurrentRow(0);
    split->addWidget(m_categories);

    m_pages = new QStackedWidget(this);
    m_pages->addWidget(buildConnectionsPage());
    m_pages->addWidget(buildLibrariesPage());
    m_pages->addWidget(buildPlaybackPage());
    m_pages->addWidget(buildAppearancePage());
    split->addWidget(m_pages, 1);

    connect(m_probe, &ServerProbe::finished, this,
            [this](const QUrl& url, bool online, const QString&) {
                // Map URL → server id and update the LED on its row.
                for (const auto& s : AccountStore::instance().servers()) {
                    if (s.url == url) {
                        m_serverStatus[s.id] = online ? 1 : 2;
                        refreshServers();
                        break;
                    }
                }
            });
    connect(&AccountStore::instance(), &AccountStore::changed, this,
            [this]() { refreshServers(); refreshAccounts(); });

    connect(m_categories, &QListWidget::currentRowChanged,
            m_pages, &QStackedWidget::setCurrentIndex);

    root->addLayout(split, 1);

    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    auto* discardBtn = new QPushButton(tr("Descartar cambios"), this);
    auto* saveBtn = new QPushButton(tr("Guardar"), this);
    saveBtn->setDefault(true);
    btnRow->addWidget(discardBtn);
    btnRow->addWidget(saveBtn);
    root->addLayout(btnRow);

    connect(saveBtn, &QPushButton::clicked, this, &SettingsDialog::onSave);
    connect(discardBtn, &QPushButton::clicked, this, &QDialog::reject);
}

QWidget* SettingsDialog::buildLibrariesPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* title = new QLabel(tr("Mostrar bibliotecas"), page);
    QFont tf = title->font();
    tf.setBold(true);
    tf.setPointSize(tf.pointSize() + 2);
    title->setFont(tf);
    layout->addWidget(title);

    auto* desc = new QLabel(
        tr("Marca las bibliotecas que quieres ver en la barra lateral y "
           "en la página de Inicio."), page);
    desc->setForegroundRole(QPalette::PlaceholderText);
    desc->setWordWrap(true);
    layout->addWidget(desc);

    m_libsList = new QListWidget(page);
    m_libsList->setSelectionMode(QAbstractItemView::NoSelection);
    m_libsList->setFrameShape(QFrame::NoFrame);
    const auto libs = m_browser->allLibraries();
    if (libs.isEmpty()) {
        auto* empty = new QListWidgetItem(tr("(sin bibliotecas)"), m_libsList);
        empty->setFlags(Qt::NoItemFlags);
    } else {
        for (const auto& lib : libs) {
            auto* item = new QListWidgetItem(lib.name, m_libsList);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
            item->setCheckState(m_browser->isLibraryHidden(lib.id)
                                    ? Qt::Unchecked : Qt::Checked);
            item->setData(Qt::UserRole, lib.id);
        }
    }
    layout->addWidget(m_libsList, 1);

    return page;
}

QWidget* SettingsDialog::buildPlaybackPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* title = new QLabel(tr("Decodificación de hardware"), page);
    QFont tf = title->font();
    tf.setBold(true);
    tf.setPointSize(tf.pointSize() + 2);
    title->setFont(tf);
    layout->addWidget(title);

    auto* desc = new QLabel(
        tr("Elige el motor de decodificación. «Automático» deja que mpv "
           "elija el mejor disponible."), page);
    desc->setForegroundRole(QPalette::PlaceholderText);
    desc->setWordWrap(true);
    layout->addWidget(desc);

    auto* row = new QHBoxLayout;
    row->setSpacing(8);
    auto* lbl = new QLabel(tr("Motor:"), page);
    m_hwCombo = new QComboBox(page);
    m_hwCombo->addItem(
        tr("Automático (detectado: %1)").arg(m_player->detectedDefaultHwdec()),
        QString());
    for (const QString& c : m_player->availableHwdecChoices())
        m_hwCombo->addItem(c, c);
    const QString current = m_player->hwdecSetting();
    int idx = m_hwCombo->findData(current);
    m_hwCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    row->addWidget(lbl);
    row->addWidget(m_hwCombo, 1);
    layout->addLayout(row);

    layout->addStretch();
    return page;
}

QWidget* SettingsDialog::buildConnectionsPage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto boldTitleFont = [](QFont f) {
        f.setBold(true);
        f.setPointSize(f.pointSize() + 2);
        return f;
    };

    // ---- Servers section ----
    auto* sTitle = new QLabel(tr("Servidores"), page);
    sTitle->setFont(boldTitleFont(sTitle->font()));
    layout->addWidget(sTitle);

    auto* sDesc = new QLabel(
        tr("El LED indica si el servidor responde. Puedes guardarlo aunque "
           "esté offline."), page);
    sDesc->setForegroundRole(QPalette::PlaceholderText);
    sDesc->setWordWrap(true);
    layout->addWidget(sDesc);

    m_serversList = new QListWidget(page);
    m_serversList->setFrameShape(QFrame::NoFrame);
    m_serversList->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_serversList, 1);

    auto* sRow = new QHBoxLayout;
    auto* addServerBtn = new QPushButton(tr("Añadir"), page);
    auto* editServerBtn = new QPushButton(tr("Editar"), page);
    auto* removeServerBtn = new QPushButton(tr("Eliminar"), page);
    sRow->addWidget(addServerBtn);
    sRow->addWidget(editServerBtn);
    sRow->addWidget(removeServerBtn);
    sRow->addStretch();
    layout->addLayout(sRow);

    connect(addServerBtn, &QPushButton::clicked, this,
            &SettingsDialog::onAddServer);
    connect(editServerBtn, &QPushButton::clicked, this,
            &SettingsDialog::onEditServer);
    connect(removeServerBtn, &QPushButton::clicked, this,
            &SettingsDialog::onRemoveServer);
    connect(m_serversList, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem*) { onEditServer(); });

    // Spacer between sections.
    layout->addSpacing(6);

    // ---- Accounts section ----
    auto* aTitle = new QLabel(tr("Cuentas"), page);
    aTitle->setFont(boldTitleFont(aTitle->font()));
    layout->addWidget(aTitle);

    auto* aDesc = new QLabel(
        tr("Cada cuenta vincula un usuario a uno de los servidores. Después "
           "puedes alternar entre ellas desde el icono de usuario."), page);
    aDesc->setForegroundRole(QPalette::PlaceholderText);
    aDesc->setWordWrap(true);
    layout->addWidget(aDesc);

    m_accountsList = new QListWidget(page);
    m_accountsList->setFrameShape(QFrame::NoFrame);
    layout->addWidget(m_accountsList, 1);

    auto* aRow = new QHBoxLayout;
    auto* addAccountBtn = new QPushButton(tr("Añadir"), page);
    auto* editAccountBtn = new QPushButton(tr("Editar"), page);
    auto* removeAccountBtn = new QPushButton(tr("Eliminar"), page);
    aRow->addWidget(addAccountBtn);
    aRow->addWidget(editAccountBtn);
    aRow->addWidget(removeAccountBtn);
    aRow->addStretch();
    layout->addLayout(aRow);

    connect(addAccountBtn, &QPushButton::clicked, this,
            &SettingsDialog::onAddAccount);
    connect(editAccountBtn, &QPushButton::clicked, this,
            &SettingsDialog::onEditAccount);
    connect(removeAccountBtn, &QPushButton::clicked, this,
            &SettingsDialog::onRemoveAccount);
    connect(m_accountsList, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem*) { onEditAccount(); });

    refreshServers();
    refreshAccounts();
    // Probe everything we know about so the user gets up-to-date LEDs as
    // soon as the page opens.
    for (const auto& s : AccountStore::instance().servers())
        probeServer(s.id);

    return page;
}

void SettingsDialog::refreshServers() {
    if (!m_serversList) return;
    const QString prevId = m_serversList->currentItem()
        ? m_serversList->currentItem()->data(Qt::UserRole).toString()
        : QString();
    m_serversList->clear();
    for (const auto& s : AccountStore::instance().servers()) {
        auto* row = new QWidget;
        auto* lay = new QHBoxLayout(row);
        lay->setContentsMargins(8, 4, 8, 4);
        lay->setSpacing(10);
        auto* dot = new StatusDot(row);
        const int st = m_serverStatus.value(s.id, 0);
        dot->setState(st == 1 ? StatusDot::Online
                              : st == 2 ? StatusDot::Offline
                                        : StatusDot::Unknown);
        auto* labels = new QVBoxLayout;
        labels->setContentsMargins(0, 0, 0, 0);
        labels->setSpacing(0);
        auto* name = new QLabel(s.name.isEmpty() ? s.url.host() : s.name, row);
        QFont nf = name->font();
        nf.setBold(true);
        name->setFont(nf);
        auto* url = new QLabel(s.url.toString(), row);
        url->setForegroundRole(QPalette::PlaceholderText);
        labels->addWidget(name);
        labels->addWidget(url);
        lay->addWidget(dot);
        lay->addLayout(labels, 1);

        auto* item = new QListWidgetItem(m_serversList);
        item->setSizeHint(row->sizeHint());
        item->setData(Qt::UserRole, s.id);
        m_serversList->setItemWidget(item, row);
        if (s.id == prevId) m_serversList->setCurrentItem(item);
    }
}

void SettingsDialog::refreshAccounts() {
    if (!m_accountsList) return;
    const QString prevId = m_accountsList->currentItem()
        ? m_accountsList->currentItem()->data(Qt::UserRole).toString()
        : QString();
    m_accountsList->clear();
    auto& store = AccountStore::instance();
    for (const auto& a : store.accounts()) {
        const ServerEntry srv = store.server(a.serverId);
        const QString srvLabel = srv.name.isEmpty() ? srv.url.host() : srv.name;
        const QString text = srvLabel.isEmpty()
                                 ? a.username
                                 : QStringLiteral("%1  ·  %2")
                                       .arg(a.username, srvLabel);
        auto* item = new QListWidgetItem(text, m_accountsList);
        item->setData(Qt::UserRole, a.id);
        if (a.id == prevId) m_accountsList->setCurrentItem(item);
    }
}

void SettingsDialog::onAddServer() {
    AddServerDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    const QUrl u = dlg.normalisedUrl();
    if (!u.isValid() || u.host().isEmpty()) {
        QMessageBox::warning(this, tr("URL no válida"),
                             tr("La URL del servidor no es válida."));
        return;
    }
    const QString id = AccountStore::instance().addServer(dlg.name(), u);
    probeServer(id);
}

void SettingsDialog::onEditServer() {
    if (!m_serversList || !m_serversList->currentItem()) return;
    const QString id = m_serversList->currentItem()->data(Qt::UserRole)
                          .toString();
    if (id.isEmpty()) return;
    auto& store = AccountStore::instance();
    const ServerEntry current = store.server(id);
    if (current.id.isEmpty()) return;
    AddServerDialog dlg(this, current);
    if (dlg.exec() != QDialog::Accepted) return;
    const QUrl u = dlg.normalisedUrl();
    if (!u.isValid() || u.host().isEmpty()) {
        QMessageBox::warning(this, tr("URL no válida"),
                             tr("La URL del servidor no es válida."));
        return;
    }
    ServerEntry updated = current;
    updated.name = dlg.name().isEmpty() ? u.host() : dlg.name();
    updated.url = u;
    store.updateServer(updated);
    // The URL may have changed — re-probe to refresh the LED.
    probeServer(id);
}

void SettingsDialog::onRemoveServer() {
    if (!m_serversList || !m_serversList->currentItem()) return;
    const QString id = m_serversList->currentItem()->data(Qt::UserRole)
                          .toString();
    if (id.isEmpty()) return;
    if (QMessageBox::question(this, tr("Eliminar servidor"),
            tr("¿Eliminar el servidor y todas sus cuentas?"))
        != QMessageBox::Yes) return;
    AccountStore::instance().removeServer(id);
    m_serverStatus.remove(id);
}

void SettingsDialog::onAddAccount() {
    if (AccountStore::instance().servers().isEmpty()) {
        QMessageBox::information(this, tr("Sin servidores"),
            tr("Antes de añadir cuentas tienes que crear al menos un servidor."));
        m_categories->setCurrentRow(0);
        return;
    }
    // We need a JellyfinClient to perform the auth call. Borrow the player's
    // client field is not exposed; create a transient one. The deviceId is
    // pulled from QSettings inside its constructor, matching what the rest
    // of the app uses.
    JellyfinClient probeClient;
    AddAccountDialog dlg(this, &probeClient);
    if (!dlg.isValid()) return;
    dlg.exec();
}

void SettingsDialog::onEditAccount() {
    if (!m_accountsList || !m_accountsList->currentItem()) return;
    const QString id = m_accountsList->currentItem()->data(Qt::UserRole)
                          .toString();
    if (id.isEmpty()) return;
    if (AccountStore::instance().servers().isEmpty()) {
        QMessageBox::information(this, tr("Sin servidores"),
            tr("Antes de editar cuentas tienes que crear al menos un servidor."));
        return;
    }
    JellyfinClient probeClient;
    AddAccountDialog dlg(this, &probeClient, id);
    if (!dlg.isValid()) return;
    dlg.exec();
}

void SettingsDialog::onRemoveAccount() {
    if (!m_accountsList || !m_accountsList->currentItem()) return;
    const QString id = m_accountsList->currentItem()->data(Qt::UserRole)
                          .toString();
    if (id.isEmpty()) return;
    if (QMessageBox::question(this, tr("Eliminar cuenta"),
            tr("¿Eliminar esta cuenta? Tendrás que volver a iniciar sesión "
               "para usarla."))
        != QMessageBox::Yes) return;
    AccountStore::instance().removeAccount(id);
}

void SettingsDialog::probeServer(const QString& serverId) {
    const ServerEntry s = AccountStore::instance().server(serverId);
    if (s.id.isEmpty()) return;
    m_serverStatus[serverId] = 0;
    refreshServers();
    m_probe->probe(s.url);
}

QWidget* SettingsDialog::buildAppearancePage() {
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* title = new QLabel(tr("Tema"), page);
    QFont tf = title->font();
    tf.setBold(true);
    tf.setPointSize(tf.pointSize() + 2);
    title->setFont(tf);
    layout->addWidget(title);

    auto* desc = new QLabel(
        tr("Elige la paleta de colores de la interfaz. El tema «Carbono» "
           "usa una superficie oscura con texto e iconos en blanco roto."),
        page);
    desc->setForegroundRole(QPalette::PlaceholderText);
    desc->setWordWrap(true);
    layout->addWidget(desc);

    m_themeGroup = new QButtonGroup(this);
    auto* lightBtn = new QRadioButton(tr("Claro (Breeze)"), page);
    lightBtn->setProperty("themeKey", QStringLiteral("light"));
    auto* carbonBtn = new QRadioButton(tr("Carbono (oscuro)"), page);
    carbonBtn->setProperty("themeKey", QStringLiteral("carbon"));
    m_themeGroup->addButton(lightBtn);
    m_themeGroup->addButton(carbonBtn);
    layout->addWidget(lightBtn);
    layout->addWidget(carbonBtn);

    const QString currentKey = Theme::currentKey();
    (currentKey == "carbon" ? carbonBtn : lightBtn)->setChecked(true);

    layout->addStretch();
    return page;
}

void SettingsDialog::onSave() {
    if (m_libsList) {
        for (int i = 0; i < m_libsList->count(); ++i) {
            auto* item = m_libsList->item(i);
            const QString id = item->data(Qt::UserRole).toString();
            if (id.isEmpty()) continue;
            const bool visible = (item->checkState() == Qt::Checked);
            m_browser->setLibraryHidden(id, !visible);
        }
    }
    if (m_hwCombo) {
        m_player->setHwdecSetting(m_hwCombo->currentData().toString());
    }
    if (m_themeGroup) {
        if (auto* btn = m_themeGroup->checkedButton()) {
            const QString key = btn->property("themeKey").toString();
            Theme::setCurrent(key == "carbon" ? Theme::Carbon : Theme::Light);
        }
    }
    accept();
}
