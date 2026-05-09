#pragma once

#include "JellyfinClient.h"

#include <QObject>
#include <QStringList>
#include <QUrl>

// Single-item view of a JellyfinItem for the QML DetailPage. Owns a
// JellyfinItem snapshot and exposes its fields as Q_PROPERTYs. load(id)
// asks JellyfinClient for the full details; the model also reacts to
// favorite/played toggles so checkbox-style UI stays in sync after the
// server confirms.
class ItemDetailModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(JellyfinClient* client READ client WRITE setClient NOTIFY clientChanged)

    Q_PROPERTY(QString itemId READ itemId NOTIFY itemChanged)
    Q_PROPERTY(QString name READ name NOTIFY itemChanged)
    Q_PROPERTY(QString type READ type NOTIFY itemChanged)
    Q_PROPERTY(QString seriesName READ seriesName NOTIFY itemChanged)
    Q_PROPERTY(QString seriesId READ seriesId NOTIFY itemChanged)
    Q_PROPERTY(int productionYear READ productionYear NOTIFY itemChanged)
    Q_PROPERTY(int indexNumber READ indexNumber NOTIFY itemChanged)
    Q_PROPERTY(int parentIndexNumber READ parentIndexNumber NOTIFY itemChanged)
    Q_PROPERTY(qint64 runTimeSeconds READ runTimeSeconds NOTIFY itemChanged)
    Q_PROPERTY(qint64 resumeSeconds READ resumeSeconds NOTIFY itemChanged)
    Q_PROPERTY(QString overview READ overview NOTIFY itemChanged)
    Q_PROPERTY(QString officialRating READ officialRating NOTIFY itemChanged)
    Q_PROPERTY(double communityRating READ communityRating NOTIFY itemChanged)
    Q_PROPERTY(bool isFavorite READ isFavorite NOTIFY itemChanged)
    Q_PROPERTY(bool isPlayed READ isPlayed NOTIFY itemChanged)
    Q_PROPERTY(bool playable READ playable NOTIFY itemChanged)
    Q_PROPERTY(bool isFolder READ isFolder NOTIFY itemChanged)
    Q_PROPERTY(QStringList genres READ genres NOTIFY itemChanged)
    Q_PROPERTY(QStringList studios READ studios NOTIFY itemChanged)
    Q_PROPERTY(QUrl posterUrl READ posterUrl NOTIFY itemChanged)
    Q_PROPERTY(QUrl backdropUrl READ backdropUrl NOTIFY itemChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)

public:
    explicit ItemDetailModel(QObject* parent = nullptr);

    JellyfinClient* client() const { return m_client; }
    void setClient(JellyfinClient* c);

    QString itemId() const { return m_item.id; }
    QString name() const { return m_item.name; }
    QString type() const { return m_item.type; }
    QString seriesName() const { return m_item.seriesName; }
    QString seriesId() const { return m_item.seriesId; }
    int productionYear() const { return m_item.productionYear; }
    int indexNumber() const { return m_item.indexNumber; }
    int parentIndexNumber() const { return m_item.parentIndexNumber; }
    qint64 runTimeSeconds() const;
    qint64 resumeSeconds() const;
    QString overview() const { return m_item.overview; }
    QString officialRating() const { return m_item.officialRating; }
    double communityRating() const { return m_item.communityRating; }
    bool isFavorite() const { return m_item.isFavorite; }
    bool isPlayed() const { return m_item.isPlayed; }
    bool playable() const { return m_item.playable(); }
    bool isFolder() const { return m_item.isFolder; }
    QStringList genres() const { return m_item.genres; }
    QStringList studios() const { return m_item.studios; }
    QUrl posterUrl() const;
    QUrl backdropUrl() const;
    bool loading() const { return m_loading; }

    // Pre-fill the detail with whatever fields we already had at click time
    // (name, year, posterUrl, type, seriesName, ...). Lets DetailPage show
    // something immediately before fetchItemDetails resolves. Keys come
    // straight from ItemListModel::roleNames().
    Q_INVOKABLE void prefill(const QVariantMap& m);

    Q_INVOKABLE void load(const QString& itemId);
    Q_INVOKABLE void toggleFavorite();
    Q_INVOKABLE void togglePlayed();

signals:
    void clientChanged();
    void itemChanged();
    void loadingChanged();
    void loadFailed(const QString& message);

private:
    void onItemDetailsLoaded(const JellyfinItem& item);
    void onFavoriteToggled(const QString& itemId, bool isFavorite);
    void onPlayedToggled(const QString& itemId, bool isPlayed);
    void setLoading(bool v);

    JellyfinClient* m_client = nullptr;
    JellyfinItem m_item;
    bool m_loading = false;
};
