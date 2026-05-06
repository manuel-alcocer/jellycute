#pragma once

#include <QFontMetrics>
#include <QPainter>
#include <QPalette>
#include <QPolygon>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>

// Item delegate for poster grids/rows. It paints the default item, optionally
// substitutes the caption with a fixed two-line layout (title + year, with
// the title elided to one line), and overlays a translucent play-circle on
// hovered items that opt in via PlayableRole.
class PlayEmblemDelegate : public QStyledItemDelegate {
public:
    static constexpr int PlayableRole = Qt::UserRole + 100;
    static constexpr int YearRole = Qt::UserRole + 101;
    static constexpr int Size = 36;
    static constexpr int Margin = 8;
    static constexpr int IconTopGap = 4;
    static constexpr int CaptionTopGap = 6;

    explicit PlayEmblemDelegate(bool twoLineCaption, QObject* parent = nullptr)
        : QStyledItemDelegate(parent), m_twoLineCaption(twoLineCaption) {}
    explicit PlayEmblemDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent) {}

    void initStyleOption(QStyleOptionViewItem* opt,
                         const QModelIndex& idx) const override {
        QStyledItemDelegate::initStyleOption(opt, idx);
        // We render the two-line caption ourselves; suppress the default text.
        if (m_twoLineCaption) opt->text.clear();
    }

    void paint(QPainter* p, const QStyleOptionViewItem& opt,
               const QModelIndex& idx) const override {
        QStyledItemDelegate::paint(p, opt, idx);
        if (m_twoLineCaption) drawTwoLineCaption(p, opt, idx);
        if (idx.data(PlayableRole).toBool()
            && (opt.state & QStyle::State_MouseOver)) {
            drawPlayEmblem(p, opt);
        }
    }

private:
    bool m_twoLineCaption = false;

    static QRect iconRect(const QStyleOptionViewItem& opt) {
        const QSize iconSize = opt.decorationSize;
        const int ix = opt.rect.x() + (opt.rect.width() - iconSize.width()) / 2;
        const int iy = opt.rect.y() + IconTopGap;
        return QRect(ix, iy, iconSize.width(), iconSize.height());
    }

    void drawPlayEmblem(QPainter* p, const QStyleOptionViewItem& opt) const {
        const QRect ir = iconRect(opt);
        const QRect r(ir.right() - Margin - Size + 1,
                      ir.bottom() - Margin - Size + 1,
                      Size, Size);
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->setBrush(QColor(0, 0, 0, 170));
        p->setPen(Qt::NoPen);
        p->drawEllipse(r);
        QPolygon tri;
        const int cx = r.center().x();
        const int cy = r.center().y();
        const int s = Size / 4;
        tri << QPoint(cx - s + 2, cy - s)
            << QPoint(cx - s + 2, cy + s)
            << QPoint(cx + s + 2, cy);
        p->setBrush(Qt::white);
        p->drawPolygon(tri);
        p->restore();
    }

    void drawTwoLineCaption(QPainter* p, const QStyleOptionViewItem& opt,
                            const QModelIndex& idx) const {
        const QString title = idx.data(Qt::DisplayRole).toString();
        const QString year = idx.data(YearRole).toString();
        if (title.isEmpty() && year.isEmpty()) return;

        const QRect ir = iconRect(opt);
        const int textTop = ir.bottom() + CaptionTopGap;
        const int textLeft = opt.rect.x() + 6;
        const int textWidth = opt.rect.width() - 12;
        if (textWidth <= 0) return;

        p->save();
        p->setPen(opt.palette.color(QPalette::Text));

        QFont titleFont = opt.font;
        const QFontMetrics tfm(titleFont);
        const int titleH = tfm.lineSpacing();
        const QString elided = tfm.elidedText(title, Qt::ElideRight, textWidth);
        p->setFont(titleFont);
        p->drawText(QRect(textLeft, textTop, textWidth, titleH),
                    Qt::AlignHCenter | Qt::AlignTop, elided);

        if (!year.isEmpty()) {
            QFont yearFont = opt.font;
            yearFont.setPointSizeF(qMax(7.0, yearFont.pointSizeF() - 1.0));
            const QFontMetrics yfm(yearFont);
            QColor muted = opt.palette.color(QPalette::Text);
            muted.setAlpha(170);
            p->setFont(yearFont);
            p->setPen(muted);
            p->drawText(QRect(textLeft, textTop + titleH,
                              textWidth, yfm.lineSpacing()),
                        Qt::AlignHCenter | Qt::AlignTop,
                        yfm.elidedText(year, Qt::ElideRight, textWidth));
        }
        p->restore();
    }
};
