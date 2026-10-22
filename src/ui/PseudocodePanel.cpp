#include "PseudocodePanel.h"

#include "Palette.h"
#include "Pseudocode.h"

#include <QFontMetrics>
#include <QPainter>

namespace vdsa {

namespace {
constexpr int kPadding = 8;
constexpr int kGutter = 30;
} // namespace

PseudocodePanel::PseudocodePanel(QWidget* parent) : QWidget(parent)
{
    setFont(palette::monoFont(9.5));
    setAccessibleName(tr("Pseudocode"));
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void PseudocodePanel::setOperation(OperationType op)
{
    if (op == operation_)
        return;
    operation_ = op;
    activeLine_ = PseudoLine::None;
    updateGeometry();
    update();
    emit activeRowChanged(-1);
}

void PseudocodePanel::setActiveLine(PseudoLine line)
{
    if (line == activeLine_)
        return;
    activeLine_ = line;
    update();
    emit activeRowChanged(activeRow());
}

int PseudocodePanel::activeRow() const { return pseudocodeRow(operation_, activeLine_); }

int PseudocodePanel::rowHeight() const { return QFontMetrics(font()).height() + 7; }

QRect PseudocodePanel::rowRect(int row) const
{
    return {0, kPadding + row * rowHeight(), width(), rowHeight()};
}

QSize PseudocodePanel::sizeHint() const
{
    const auto& listing = pseudocodeFor(operation_);
    const QFontMetrics fm(font());
    int widest = 0;
    for (const PseudoLineSpec& line : listing)
        widest = std::max(widest, fm.horizontalAdvance(QString(line.indent * 2, QLatin1Char(' '))
                                                       + QString::fromUtf8(line.text)));
    return {kGutter + widest + 16, 2 * kPadding + static_cast<int>(listing.size()) * rowHeight()};
}

void PseudocodePanel::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setPen(QPen(palette::border, 1));
    p.setBrush(palette::background);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);

    const auto& listing = pseudocodeFor(operation_);
    const int active = activeRow();
    const QFontMetrics fm(font());
    for (int row = 0; row < static_cast<int>(listing.size()); ++row) {
        const QRect r = rowRect(row);
        const bool isActive = row == active;
        if (isActive) {
            QColor tint = palette::accent;
            tint.setAlphaF(0.13f);
            p.fillRect(r.adjusted(1, 0, -1, 0), tint);
            p.fillRect(QRect(r.left() + 1, r.top(), 2, r.height()), palette::accent);
        }
        p.setPen(isActive ? palette::accent : palette::textMuted);
        p.drawText(QRect(r.left(), r.top(), kGutter - 8, r.height()), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(row + 1));
        p.setPen(isActive ? palette::textPrimary : palette::textSecondary);
        const QString text = QString(listing[row].indent * 2, QLatin1Char(' ')) + QString::fromUtf8(listing[row].text);
        p.drawText(QRect(r.left() + kGutter, r.top(), r.width() - kGutter - 6, r.height()),
                   Qt::AlignLeft | Qt::AlignVCenter, fm.elidedText(text, Qt::ElideRight, r.width() - kGutter - 6));
    }
}

} // namespace vdsa
