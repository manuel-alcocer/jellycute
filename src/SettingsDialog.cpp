#include "SettingsDialog.h"
#include "BrowserWidget.h"
#include "MpvWidget.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QStackedWidget>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(BrowserWidget* browser, MpvWidget* player,
                               QWidget* parent)
    : QDialog(parent), m_browser(browser), m_player(player) {
    setWindowTitle(tr("Ajustes"));
    setObjectName(QStringLiteral("settingsDialog"));
    resize(720, 480);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    auto* split = new QHBoxLayout;
    split->setSpacing(12);

    m_categories = new QListWidget(this);
    m_categories->setObjectName(QStringLiteral("settingsCategories"));
    m_categories->setFixedWidth(180);
    m_categories->setFrameShape(QFrame::NoFrame);
    new QListWidgetItem(tr("Bibliotecas"),  m_categories);
    new QListWidgetItem(tr("Reproducción"), m_categories);
    m_categories->setCurrentRow(0);
    split->addWidget(m_categories);

    m_pages = new QStackedWidget(this);
    m_pages->addWidget(buildLibrariesPage());
    m_pages->addWidget(buildPlaybackPage());
    split->addWidget(m_pages, 1);

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
    desc->setStyleSheet(QStringLiteral("color:#5d6164;"));
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
        tr("Elige el método de decodificación. «Automático» deja que mpv "
           "elija el mejor disponible."), page);
    desc->setStyleSheet(QStringLiteral("color:#5d6164;"));
    desc->setWordWrap(true);
    layout->addWidget(desc);

    m_hwGroup = new QButtonGroup(this);
    auto* autoBtn = new QRadioButton(
        tr("Automático (detectado: %1)").arg(m_player->detectedDefaultHwdec()),
        page);
    autoBtn->setProperty("hwdec", QString());
    m_hwGroup->addButton(autoBtn);
    layout->addWidget(autoBtn);

    for (const QString& c : m_player->availableHwdecChoices()) {
        auto* rb = new QRadioButton(c, page);
        rb->setProperty("hwdec", c);
        m_hwGroup->addButton(rb);
        layout->addWidget(rb);
    }

    const QString currentHwdec = m_player->hwdecSetting();
    bool any = false;
    for (auto* b : m_hwGroup->buttons()) {
        if (b->property("hwdec").toString() == currentHwdec) {
            b->setChecked(true);
            any = true;
            break;
        }
    }
    if (!any) autoBtn->setChecked(true);

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
    if (m_hwGroup) {
        if (auto* btn = m_hwGroup->checkedButton()) {
            m_player->setHwdecSetting(btn->property("hwdec").toString());
        }
    }
    accept();
}
