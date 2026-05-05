#pragma once

#include <QWidget>
#include "JellyfinClient.h"

class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;

class MediaRow : public QWidget {
    Q_OBJECT
public:
    enum class PosterShape {
        Vertical,        // 2:3 movie/series posters
        Backdrop,        // 16:9 episode/featured cards
    };

    explicit MediaRow(const QString& sectionId,
                      const QString& title,
                      PosterShape shape,
                      QWidget* parent = nullptr);

    QString sectionId() const { return m_sectionId; }
    PosterShape shape() const { return m_shape; }
    QSize posterSize() const;

    void setSeeAllVisible(bool v);
    void setItems(const QList<JellyfinItem>& items);
    void setItemIcon(const QString& itemId, const QIcon& icon);
    void clear();
    bool isEmpty() const;

signals:
    void itemActivated(const JellyfinItem& item);
    void playClicked(const JellyfinItem& item);
    void seeAllClicked(const QString& sectionId);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onActivated(QListWidgetItem* w);

private:
    QString m_sectionId;
    PosterShape m_shape;
    QLabel* m_title;
    QPushButton* m_seeAll;
    QListWidget* m_list;
    QHash<QString, QListWidgetItem*> m_byId;
    QList<JellyfinItem> m_items;
};
