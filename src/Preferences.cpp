#include "Preferences.h"

#include <QProcess>
#include <QSet>
#include <QSettings>

Preferences::Preferences(QObject* parent) : QObject(parent) {}

QString Preferences::hwdec() const {
    return QSettings().value(QStringLiteral("hwdec")).toString();
}

void Preferences::setHwdec(const QString& choice) {
    if (hwdec() == choice) return;
    QSettings().setValue(QStringLiteral("hwdec"), choice);
    emit hwdecChanged();
}

QStringList Preferences::availableHwdecChoices() const {
    static QStringList cached;
    if (!cached.isEmpty()) return cached;

    // libmpv doesn't expose the supported hwdec backends through the
    // property API. Shell out to mpv --hwdec=help (same trick MpvWidget
    // uses internally) and parse the indented list it prints.
    QProcess p;
    p.start(QStringLiteral("mpv"), QStringList{QStringLiteral("--hwdec=help")});
    if (p.waitForStarted(1500) && p.waitForFinished(2500)) {
        const QString output = QString::fromUtf8(
            p.readAllStandardOutput() + p.readAllStandardError());
        QSet<QString> seen;
        for (const QString& line : output.split(QChar('\n'))) {
            if (line.isEmpty() || !line.at(0).isSpace()) continue;
            const QString t = line.trimmed();
            if (t.isEmpty() || !t.at(0).isLower()) continue;
            const int sp = t.indexOf(QChar(' '));
            const QString name = sp < 0 ? t : t.left(sp);
            if (!name.isEmpty()) seen.insert(name);
        }
        cached = QStringList(seen.begin(), seen.end());
    }
    if (cached.isEmpty()) {
        cached = QStringList{
            "no", "auto", "auto-safe", "auto-copy",
            "vaapi", "vaapi-copy", "nvdec", "nvdec-copy",
            "vdpau", "vdpau-copy", "drmprime", "drmprime-copy",
            "vulkan", "vulkan-copy",
        };
    }
    cached.sort();
    return cached;
}
