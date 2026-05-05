#pragma once

#include <QDialog>

class QLineEdit;
class QPushButton;
class QLabel;
class JellyfinClient;

class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(JellyfinClient* client, QWidget* parent = nullptr);

private slots:
    void onAccept();
    void onAuthenticated();
    void onAuthFailed(const QString& message);

private:
    JellyfinClient* m_client;
    QLineEdit* m_server;
    QLineEdit* m_user;
    QLineEdit* m_pass;
    QPushButton* m_loginBtn;
    QLabel* m_status;
};
