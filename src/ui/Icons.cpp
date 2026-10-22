#include "Icons.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace vdsa::icons {

namespace {

void paint(QPainter& p, Glyph glyph, const QColor& color, qreal s)
{
    const qreal stroke = s * 0.09;
    QPen pen(color, stroke, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    switch (glyph) {
    case Glyph::Play: {
        QPainterPath tri;
        tri.moveTo(s * 0.32, s * 0.22);
        tri.lineTo(s * 0.78, s * 0.5);
        tri.lineTo(s * 0.32, s * 0.78);
        tri.closeSubpath();
        p.setBrush(color);
        p.drawPath(tri);
        break;
    }
    case Glyph::Pause:
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawRoundedRect(QRectF(s * 0.27, s * 0.22, s * 0.16, s * 0.56), 1, 1);
        p.drawRoundedRect(QRectF(s * 0.57, s * 0.22, s * 0.16, s * 0.56), 1, 1);
        break;
    case Glyph::Previous:
    case Glyph::Next: {
        const bool next = glyph == Glyph::Next;
        auto x = [&](qreal v) { return next ? v : s - v; };
        QPainterPath tri;
        tri.moveTo(x(s * 0.26), s * 0.24);
        tri.lineTo(x(s * 0.64), s * 0.5);
        tri.lineTo(x(s * 0.26), s * 0.76);
        tri.closeSubpath();
        p.setBrush(color);
        p.drawPath(tri);
        p.drawLine(QPointF(x(s * 0.74), s * 0.24), QPointF(x(s * 0.74), s * 0.76));
        break;
    }
    case Glyph::Restart: {
        const QRectF arc(s * 0.2, s * 0.2, s * 0.6, s * 0.6);
        p.drawArc(arc, 60 * 16, 290 * 16);
        // Arrow head at the start of the arc (top right).
        QPainterPath head;
        head.moveTo(s * 0.70, s * 0.10);
        head.lineTo(s * 0.68, s * 0.30);
        head.lineTo(s * 0.88, s * 0.28);
        p.drawPath(head);
        break;
    }
    case Glyph::ZoomIn:
    case Glyph::ZoomOut:
        p.drawLine(QPointF(s * 0.24, s * 0.5), QPointF(s * 0.76, s * 0.5));
        if (glyph == Glyph::ZoomIn)
            p.drawLine(QPointF(s * 0.5, s * 0.24), QPointF(s * 0.5, s * 0.76));
        break;
    case Glyph::Fit: {
        const qreal a = s * 0.2, b = s * 0.8, l = s * 0.18;
        p.drawPolyline(QPolygonF({{a, a + l}, {a, a}, {a + l, a}}));
        p.drawPolyline(QPolygonF({{b - l, a}, {b, a}, {b, a + l}}));
        p.drawPolyline(QPolygonF({{b, b - l}, {b, b}, {b - l, b}}));
        p.drawPolyline(QPolygonF({{a + l, b}, {a, b}, {a, b - l}}));
        break;
    }
    case Glyph::OneToOne:
        p.drawRoundedRect(QRectF(s * 0.22, s * 0.22, s * 0.56, s * 0.56), 2, 2);
        p.setBrush(color);
        p.drawEllipse(QPointF(s * 0.5, s * 0.5), s * 0.06, s * 0.06);
        break;
    }
}

} // namespace

QIcon make(Glyph glyph, const QColor& color, int size)
{
    QIcon icon;
    for (qreal ratio : {1.0, 2.0}) {
        QPixmap pm(static_cast<int>(size * ratio), static_cast<int>(size * ratio));
        pm.fill(Qt::transparent);
        {
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing);
            p.scale(ratio, ratio);
            paint(p, glyph, color, size);
        }
        pm.setDevicePixelRatio(ratio);
        icon.addPixmap(pm, QIcon::Normal);
        QPixmap disabled = pm;
        {
            QPainter p(&disabled);
            p.setCompositionMode(QPainter::CompositionMode_SourceIn);
            p.fillRect(disabled.rect(), QColor(0x55, 0x55, 0x55));
        }
        icon.addPixmap(disabled, QIcon::Disabled);
    }
    return icon;
}

} // namespace vdsa::icons
