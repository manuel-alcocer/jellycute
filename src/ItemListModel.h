#pragma once

#include "JellyfinClient.h"

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QString>

// QAbstractListModel wrapper around a list of JellyfinItem, exposing the
// fields QML delegates need as roles. The roles intentionally stay close to
// the struct fields so the same model serves Home rows, Grid pages, and the
// detail page's children/related lists. Image URLs are computed on demand
// from the item's id + tag through the JellyfinClient pointer; consumers
// must keep that pointer alive for the lifetime of the model.
class ItemListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        NameRole,
        TypeRole,
        CollectionTypeRole,
        SeriesNameRole,
        ProductionYearRole,
        IndexNumberRole,
        ParentIndexNumberRole,
        RunTimeSecondsRole,
        ResumeSecondsRole,
        IsFolderRole,
        PlayableRole,
        IsFavoriteRole,
        IsPlayedRole,
        OverviewRole,
        OfficialRatingRole,
        CommunityRatingRole,
        PrimaryImageTagRole,
        BackdropImageTagRole,
        SeriesIdRole,
        SeriesPrimaryImageTagRole,
        PosterUrlRole,
        BackdropUrlRole,
    };

    explicit ItemListModel(QObject* parent = nullptr);

    void setClient(JellyfinClient* client);
    JellyfinClient* client() const { return m_client; }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setItems(QList<JellyfinItem> items);
    void appendItems(const QList<JellyfinItem>& items);
    void clear();

    const QList<JellyfinItem>& items() const { return m_items; }
    Q_INVOKABLE QVariantMap get(int index) const;

signals:
    void countChanged();

protected:
    JellyfinClient* m_client = nullptr;
    QList<JellyfinItem> m_items;
};
