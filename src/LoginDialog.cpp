#include "LoginDialog.h"
#include "JellyfinClient.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QUrl>
#include <QVBoxLayout>

LoginDialog::LoginDialog(JellyfinClient* client, QWidget* parent)
    : QDialog(parent), m_client(client) {
    setWindowTitle(tr("Conectar a Jellyfin"));

    QSettings s;
    m_server = new QLineEdit(s.value("server").toString(), this);
    m_server->setPlaceholderText("https://jellyfin.example.com");
    m_user = new QLineEdit(s.value("username").toString(), this);
    m_pass = new QLineEdit(this);
    m_pass->setEchoMode(QLineEdit::Password);

    auto* form = new QFormLayout;
    form->addRow(tr("Servidor"), m_server);
    form->addRow(tr("Usuario"), m_user);
    form->addRow(tr("Contraseña"), m_pass);

    m_status = new QLabel(this);
    m_status->setStyleSheet("color: #c33;");

    m_loginBtn = new QPushButton(tr("Conectar"), this);
    auto* cancelBtn = new QPushButton(tr("Cancelar"), this);
    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(m_loginBtn);

    auto* root = new QVBoxLayout(this);
    root->addLayout(form);
    root->addWidget(m_status);
    root->addLayout(btnRow);

    connect(m_loginBtn, &QPushButton::clicked, this, &LoginDialog::onAccept);
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_client, &JellyfinClient::authenticated, this, &LoginDialog::onAuthenticated);
    connect(m_client, &JellyfinClient::authenticationFailed, this, &LoginDialog::onAuthFailed);

    if (m_user->text().isEmpty()) m_user->setFocus();
    else m_pass->setFocus();
}

void LoginDialog::onAccept() {
    QString srv = m_server->text().trimmed();
    if (!srv.startsWith("http://") && !srv.startsWith("https://")) srv = "http://" + srv;
    while (srv.endsWith('/')) srv.chop(1);

    QUrl url(srv);
    if (!url.isValid() || url.host().isEmpty()) {
        m_status->setText(tr("URL del servidor no válida"));
        return;
    }

    m_loginBtn->setEnabled(false);
    m_status->setText(tr("Autenticando…"));
    m_client->setServer(url);
    m_client->authenticate(m_user->text(), m_pass->text());
}

void LoginDialog::onAuthenticated() {
    QSettings s;
    s.setValue("server", m_client->server().toString());
    s.setValue("username", m_user->text());
    s.setValue("userId", m_client->userId());
    s.setValue("token", m_client->accessToken());
    accept();
}

void LoginDialog::onAuthFailed(const QString& message) {
    m_loginBtn->setEnabled(true);
    m_status->setText(tr("Falló la autenticación: %1").arg(message));
}
