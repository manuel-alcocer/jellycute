#pragma once

#include <QWidget>
#include "JellyfinClient.h"

class QLabel;
class QPushButton;
class QToolButton;
class QNetworkAccessManager;
class QVBoxLayout;
class MediaRow;

class DetailPage : public QWidget {
    Q_OBJECT
public:
    DetailPage(JellyfinClient* client, QWidget* parent = nullptr);

    void showItem(const JellyfinItem& item, const QStringList& ancestors);

signals:
    // startTicks = 0 → from beginning; >0 → resume.
    void playRequested(const JellyfinItem& item, qint64 startTicks);
    void breadcrumbClicked(int index);          // 0..ancestors-1
    void itemSelected(const JellyfinItem& item);  // a related item was clicked

private slots:
    void onItemDetailsLoaded(const JellyfinItem& item);
    void onFavoriteToggled(const QString& itemId, bool fav);
    void onPlayedToggled(const QString& itemId, bool played);
    void onSagaSiblingsLoaded(const QString& itemId, const QList<JellyfinItem>& items);
    void onGenrePeersLoaded(const QString& itemId, const QList<JellyfinItem>& items);
    void onSimilarLoaded(const QString& itemId, const QList<JellyfinItem>& items);

private:
    void rebuildInfo();
    void rebuildMediaInfo();
    void updateActionButtons();
    void fetchPoster();
    void fetchRelatedPoster(MediaRow* row, const JellyfinItem& it);
    void rebuildRelatedRows();
    void clearRelatedRows();
    static QString formatRuntime(qint64 ticks);

    void rebuildBreadcrumb();

    JellyfinClient* m_client;
    JellyfinItem m_item;
    QStringList m_ancestors;
    QNetworkAccessManager* m_imgNam;

    // Header
    QWidget* m_breadcrumb;
    class QHBoxLayout* m_breadcrumbLayout;

    // Visuals
    QLabel* m_posterLabel;
    QLabel* m_titleLabel;
    QLabel* m_metaLabel;             // Year • Runtime • Rating
    QPushButton* m_playBtn;
    QPushButton* m_resumeBtn;
    QPushButton* m_favoriteBtn;
    QPushButton* m_watchedBtn;
    QPushButton* m_unwatchedBtn;
    QLabel* m_overviewLabel;
    QLabel* m_genresLabel;
    QLabel* m_directorsLabel;
    QLabel* m_writersLabel;
    QLabel* m_castLabel;
    QLabel* m_studiosLabel;
    QLabel* m_tagsLabel;

    // Media info (filled from MediaSources/MediaStreams).
    QLabel* m_containerLabel = nullptr;
    QLabel* m_videoStreamLabel = nullptr;
    QLabel* m_audioStreamsLabel = nullptr;
    QLabel* m_subtitleStreamsLabel = nullptr;

    // Related-content rows (saga > genre > similar, capped at 10 combined).
    MediaRow* m_sagaRow = nullptr;
    MediaRow* m_genreRow = nullptr;
    MediaRow* m_similarRow = nullptr;
    QList<JellyfinItem> m_sagaItems;
    QList<JellyfinItem> m_genreItems;
    QList<JellyfinItem> m_similarItems;
};
