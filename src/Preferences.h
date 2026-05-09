#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

// Thin QObject wrapper over QSettings keys that the QML SettingsPage
// needs to read and mutate. Mainly the hwdec selector for now; future
// preferences (theme, default audio language, ...) can grow alongside.
//
// Note: hwdec changes here only affect the *next* MpvObject instance —
// MpvObject reads QSettings("hwdec") in its constructor. Changing the
// preference while a player is open won't reconfigure that player.
class Preferences : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString hwdec READ hwdec WRITE setHwdec NOTIFY hwdecChanged)

public:
    explicit Preferences(QObject* parent = nullptr);

    QString hwdec() const;
    void setHwdec(const QString& choice);

    // Static-cached list of hwdec backends mpv was built against. Probed
    // once via `mpv --hwdec=help` (the same approach MpvWidget already
    // used) and reused for the lifetime of the process.
    Q_INVOKABLE QStringList availableHwdecChoices() const;

signals:
    void hwdecChanged();
};
