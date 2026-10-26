#include "TreeCanvas.h"

#include "Palette.h"

#include <QEasingCurve>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace vdsa {

namespace {

QColor withAlpha(QColor c, qreal opacity)
{
    c.setAlphaF(std::clamp(c.alphaF() * opacity, 0.0, 1.0));
    return c;
}

QColor emphasisColor(Emphasis emphasis)
{
    switch (emphasis) {
    case Emphasis::Success: return palette::success;
    case Emphasis::Error:
    case Emphasis::Removing: return palette::error;
    case Emphasis::Neutral: return palette::textSecondary;
    case Emphasis::Active: break;
    }
    return palette::accent;
}

QColor emphasisFill(Emphasis emphasis)
{
    switch (emphasis) {
    case Emphasis::Success: return palette::successFill;
    case Emphasis::Error:
    case Emphasis::Removing: return palette::errorFill;
    case Emphasis::Neutral: return palette::nodeFill;
    case Emphasis::Active: break;
    }
    return palette::accentFill;
}

const char* roleLabel(SecondaryRole role)
{
    switch (role) {
    case SecondaryRole::NextNode: return "next";
    case SecondaryRole::OnlyChild: return "only child";
    case SecondaryRole::Successor: return "successor";
    case SecondaryRole::Parent: return "parent";
    case SecondaryRole::Target: return "target";
    case SecondaryRole::Enqueued: return "enqueued";
    case SecondaryRole::None: break;
    }
    return "";
}

QFont pixelFont(const QFont& base, int pixelSize, int weight = QFont::Normal)
{
    QFont f = base;
    f.setPixelSize(pixelSize);
    f.setWeight(static_cast<QFont::Weight>(weight));
    return f;
}

// Line between two circle centres, trimmed so it starts and ends at the rims.
QLineF rimToRim(const QPointF& a, const QPointF& b, qreal ra, qreal rb)
{
    const QLineF line(a, b);
    const qreal len = line.length();
    if (len <= ra + rb)
        return {};
    return {line.pointAt(ra / len), line.pointAt(1.0 - rb / len)};
}

} // namespace

TreeCanvas::TreeCanvas(QWidget* parent) : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAccessibleName(tr("Binary search tree canvas"));

    transition_ = new QVariantAnimation(this);
    transition_->setStartValue(0.0);
    transition_->setEndValue(1.0);
    transition_->setEasingCurve(QEasingCurve::OutCubic);
    connect(transition_, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        progress_ = value.toReal();
        update();
    });
    connect(transition_, &QVariantAnimation::finished, this, [this] {
        progress_ = 1.0;
        update();
    });
}

void TreeCanvas::showTree(const TreeSnapshot& snapshot, int transitionMs)
{
    setFrame(snapshot, std::nullopt, transitionMs);
}

void TreeCanvas::showStep(const TreeSnapshot& snapshot, const AlgorithmEvent& event, int transitionMs)
{
    setFrame(snapshot, event, transitionMs);
}

void TreeCanvas::setFrame(const TreeSnapshot& snapshot, std::optional<AlgorithmEvent> event, int transitionMs)
{
    // Start the next transition from whatever is on screen right now, so a
    // new frame arriving mid-animation never makes nodes jump.
    std::unordered_map<NodeId, QPointF> onScreen;
    for (const SnapshotNode& node : snapshot_.nodes)
        onScreen[node.id] = animatedPosition(node.id);
    transition_->stop();

    fromPositions_ = std::move(onScreen);
    fromSnapshot_ = snapshot_;
    snapshot_ = snapshot;
    layout_ = TreeLayout::compute(snapshot_);
    event_ = std::move(event);
    if (selected_ != kNoNode && !snapshot_.find(selected_))
        selected_ = kNoNode;

    bool moved = fromSnapshot_.size() != snapshot_.size();
    for (const SnapshotNode& node : snapshot_.nodes) {
        const auto it = fromPositions_.find(node.id);
        if (it == fromPositions_.end() || it->second != layout_.position(node.id)) {
            moved = true;
            break;
        }
    }

    if (autoFit_)
        applyFit();
    if (moved && transitionMs > 0) {
        progress_ = 0.0;
        transition_->setDuration(transitionMs);
        transition_->start();
    } else {
        progress_ = 1.0;
    }
    update();
}

bool TreeCanvas::isAnimating() const
{
    return transition_->state() == QAbstractAnimation::Running;
}

bool TreeCanvas::isNewNode(NodeId id) const
{
    return progress_ < 1.0 && fromPositions_.find(id) == fromPositions_.end();
}

QPointF TreeCanvas::animatedPosition(NodeId id) const
{
    const QPointF target = layout_.position(id);
    if (progress_ >= 1.0)
        return target;
    const auto it = fromPositions_.find(id);
    if (it == fromPositions_.end())
        return target;
    return it->second + (target - it->second) * progress_;
}

void TreeCanvas::setSelectedNode(NodeId id)
{
    selected_ = snapshot_.find(id) ? id : kNoNode;
    update();
}

// ---------------------------------------------------------------------------
// View transform: zoom, pan, fit
// ---------------------------------------------------------------------------

QRectF TreeCanvas::contentBounds() const
{
    const QRectF b = layout_.bounds();
    if (b.isNull())
        return {};
    // Room for the ROOT label and comparison badges above, and for ghost
    // slots / role tags one level below the deepest node.
    return b.adjusted(-48, -44, 48, layout_.metrics().levelHeight * 0.85);
}

QRectF TreeCanvas::viewportArea() const
{
    // Leave space for the overlay badges at the top and the output strip (plus
    // the queue strip during a level-order traversal) at the bottom.
    const bool queueStrip = event_ && event_->operation == OperationType::LevelOrderTraversal;
    return QRectF(rect()).adjusted(16, 48, -16, queueStrip ? -98 : -56);
}

QTransform TreeCanvas::worldTransform() const
{
    const QPointF c = QRectF(rect()).center() + pan_;
    QTransform t;
    t.translate(c.x(), c.y());
    t.scale(zoom_, zoom_);
    t.translate(-worldCenter_.x(), -worldCenter_.y());
    return t;
}

void TreeCanvas::applyFit()
{
    const QRectF bounds = contentBounds();
    const QRectF area = viewportArea();
    if (bounds.isNull() || area.width() <= 0 || area.height() <= 0) {
        zoom_ = 1.0;
        worldCenter_ = {};
        pan_ = {};
        return;
    }
    const qreal fit = std::min(area.width() / bounds.width(), area.height() / bounds.height());
    zoom_ = std::clamp(fit, kMinZoom, 1.25); // never blow small trees up beyond 125%
    worldCenter_ = bounds.center();
    pan_ = area.center() - QRectF(rect()).center();
}

void TreeCanvas::setZoomAround(qreal zoom, const QPointF& anchor)
{
    zoom = std::clamp(zoom, kMinZoom, kMaxZoom);
    const QPointF world = worldTransform().inverted().map(anchor);
    zoom_ = zoom;
    pan_ = anchor - QRectF(rect()).center() - (world - worldCenter_) * zoom_;
    autoFit_ = false;
    update();
    emit zoomChanged(zoom_);
}

void TreeCanvas::zoomIn() { setZoomAround(zoom_ * 1.2, QRectF(rect()).center()); }
void TreeCanvas::zoomOut() { setZoomAround(zoom_ / 1.2, QRectF(rect()).center()); }

void TreeCanvas::fitToView()
{
    autoFit_ = true;
    applyFit();
    update();
    emit zoomChanged(zoom_);
}

void TreeCanvas::resetZoom()
{
    autoFit_ = false;
    zoom_ = 1.0;
    const QRectF bounds = contentBounds();
    worldCenter_ = bounds.isNull() ? QPointF() : bounds.center();
    pan_ = viewportArea().center() - QRectF(rect()).center();
    update();
    emit zoomChanged(zoom_);
}

QPointF TreeCanvas::nodeScreenPosition(NodeId id) const
{
    return worldTransform().map(animatedPosition(id));
}

NodeId TreeCanvas::nodeAt(const QPointF& screenPos) const
{
    const QPointF world = worldTransform().inverted().map(screenPos);
    const qreal r = layout_.metrics().nodeRadius + 2;
    for (const SnapshotNode& node : snapshot_.nodes) {
        if (QLineF(world, animatedPosition(node.id)).length() <= r)
            return node.id;
    }
    return kNoNode;
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void TreeCanvas::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (autoFit_)
        applyFit();
}

void TreeCanvas::wheelEvent(QWheelEvent* event)
{
    const qreal steps = event->angleDelta().y() / 120.0;
    if (steps != 0.0)
        setZoomAround(zoom_ * std::pow(1.15, steps), event->position());
    event->accept();
}

void TreeCanvas::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        dragging_ = true;
        pressPos_ = lastMousePos_ = event->position().toPoint();
    }
    QWidget::mousePressEvent(event);
}

void TreeCanvas::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint pos = event->position().toPoint();
    if (dragging_ && (event->buttons() & Qt::LeftButton)) {
        if ((pos - pressPos_).manhattanLength() > 3) {
            setCursor(Qt::ClosedHandCursor);
            pan_ += QPointF(pos - lastMousePos_);
            autoFit_ = false;
            update();
        }
        lastMousePos_ = pos;
    } else {
        setCursor(nodeAt(event->position()) != kNoNode ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
}

void TreeCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && dragging_) {
        dragging_ = false;
        unsetCursor();
        if ((event->position().toPoint() - pressPos_).manhattanLength() <= 3) {
            const NodeId id = nodeAt(event->position());
            setSelectedNode(id);
            emit nodeSelected(id);
        }
    }
    QWidget::mouseReleaseEvent(event);
}

void TreeCanvas::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (nodeAt(event->position()) == kNoNode)
        fitToView();
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

void TreeCanvas::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.fillRect(rect(), palette::background);
    drawGrid(p);

    p.save();
    p.setTransform(worldTransform());
    drawEdges(p);
    drawPendingSlot(p);
    drawNodes(p);
    drawBadges(p);
    p.restore();

    drawOverlays(p);
}

void TreeCanvas::drawGrid(QPainter& p) const
{
    // A faint dot grid gives the workspace depth without competing with the tree.
    const qreal spacing = 28.0;
    const QPointF origin = worldTransform().map(QPointF(0, 0));
    const qreal ox = std::fmod(origin.x(), spacing);
    const qreal oy = std::fmod(origin.y(), spacing);
    p.setPen(QPen(QColor(255, 255, 255, 14), 1.4, Qt::SolidLine, Qt::RoundCap));
    for (qreal x = ox; x < width(); x += spacing)
        for (qreal y = oy; y < height(); y += spacing)
            p.drawPoint(QPointF(x, y));
}

void TreeCanvas::drawEdges(QPainter& p) const
{
    const qreal r = layout_.metrics().nodeRadius;

    // Edges on the active path: each consecutive (parent, child) pair.
    std::unordered_set<NodeId> pathChildren;
    if (event_) {
        for (std::size_t i = 1; i < event_->path.size(); ++i) {
            const SnapshotNode* child = snapshot_.find(event_->path[i]);
            if (child && child->parent == event_->path[i - 1])
                pathChildren.insert(child->id);
        }
    }

    // Edges of removed nodes fade out during a transition.
    if (progress_ < 1.0) {
        for (const SnapshotNode& old : fromSnapshot_.nodes) {
            if (old.parent == kNoNode || snapshot_.find(old.id))
                continue;
            const auto a = fromPositions_.find(old.parent);
            const auto b = fromPositions_.find(old.id);
            if (a == fromPositions_.end() || b == fromPositions_.end())
                continue;
            p.setPen(QPen(withAlpha(palette::error, 1.0 - progress_), 1.6));
            p.drawLine(rimToRim(a->second, b->second, r, r));
        }
    }

    for (const SnapshotNode& node : snapshot_.nodes) {
        if (node.parent == kNoNode)
            continue;
        const QLineF line = rimToRim(animatedPosition(node.parent), animatedPosition(node.id), r, r);
        const qreal opacity = isNewNode(node.id) ? progress_ : 1.0;
        const bool onPath = pathChildren.count(node.id) != 0;
        const bool nextHop = event_ && event_->secondaryRole == SecondaryRole::NextNode
                          && event_->secondaryId == node.id && event_->nodeId == node.parent;
        if (nextHop) {
            QPen pen(withAlpha(palette::accent, opacity), 2.4, Qt::DashLine, Qt::RoundCap);
            pen.setDashPattern({3, 2.5});
            p.setPen(pen);
        } else if (onPath) {
            p.setPen(QPen(withAlpha(palette::accent, opacity), 2.4, Qt::SolidLine, Qt::RoundCap));
        } else {
            p.setPen(QPen(withAlpha(palette::edge, opacity), 1.6, Qt::SolidLine, Qt::RoundCap));
        }
        p.drawLine(line);
    }
}

void TreeCanvas::drawPendingSlot(QPainter& p) const
{
    if (!event_ || event_->pendingParent == kNoNode || !layout_.contains(event_->pendingParent))
        return;
    const qreal r = layout_.metrics().nodeRadius;
    const QPointF parent = animatedPosition(event_->pendingParent);
    const QPointF slot = parent + (layout_.slotPosition(event_->pendingParent, event_->pendingSide)
                                   - layout_.position(event_->pendingParent));
    const QColor color = event_->emphasis == Emphasis::Error ? palette::error : palette::accent;

    QPen dashed(withAlpha(color, 0.85), 1.6, Qt::DashLine, Qt::RoundCap);
    dashed.setDashPattern({3, 3});
    p.setPen(dashed);
    p.drawLine(rimToRim(parent, slot, r, r * 0.8));
    p.setBrush(withAlpha(color, 0.06));
    p.drawEllipse(slot, r * 0.8, r * 0.8);

    const bool inserting = event_->operation == OperationType::Insert;
    p.setFont(pixelFont(palette::monoFont(), 11, QFont::DemiBold));
    p.setPen(color);
    const QString label = inserting ? QStringLiteral("+%1").arg(event_->key) : QStringLiteral("null");
    p.drawText(QRectF(slot.x() - 30, slot.y() - 10, 60, 20), Qt::AlignCenter, label);
}

TreeCanvas::NodeStyle TreeCanvas::styleFor(const SnapshotNode& node) const
{
    NodeStyle s{palette::nodeFill, palette::nodeBorder, 1.5, palette::textPrimary, {}, {}, 1.0, 1.0};
    if (!event_)
        return s;

    const auto& completed = event_->completed;
    if (std::find(completed.begin(), completed.end(), node.id) != completed.end()) {
        s.fill = palette::successFill;
        s.border = withAlpha(palette::success, 0.75);
    }
    const auto& path = event_->path;
    if (std::find(path.begin(), path.end(), node.id) != path.end()) {
        s.border = withAlpha(palette::accent, 0.6);
        s.borderWidth = 1.8;
    }
    // Level-order traversal: every node waiting in the queue gets a dashed ring.
    const auto& queue = event_->queue;
    if (std::find(queue.begin(), queue.end(), node.id) != queue.end())
        s.dashedRing = withAlpha(palette::accent, 0.7);
    if (node.id == event_->secondaryId) {
        switch (event_->secondaryRole) {
        case SecondaryRole::Target: s.dashedRing = palette::error; break;
        case SecondaryRole::Parent: s.dashedRing = palette::textSecondary; break;
        case SecondaryRole::Successor:
        case SecondaryRole::OnlyChild:
        case SecondaryRole::Enqueued:
        case SecondaryRole::NextNode: s.dashedRing = palette::accent; break;
        case SecondaryRole::None: break;
        }
    }
    if (node.id == event_->nodeId) {
        s.fill = emphasisFill(event_->emphasis);
        s.border = emphasisColor(event_->emphasis);
        s.borderWidth = 2.6;
        if (event_->emphasis != Emphasis::Neutral)
            s.halo = withAlpha(emphasisColor(event_->emphasis), 0.22);
    }
    return s;
}

void TreeCanvas::drawNode(QPainter& p, const QPointF& c, int key, const NodeStyle& s) const
{
    const qreal r = layout_.metrics().nodeRadius * s.scale;
    p.save();
    p.setOpacity(s.opacity);
    if (s.halo.isValid()) {
        p.setPen(Qt::NoPen);
        p.setBrush(s.halo);
        p.drawEllipse(c, r + 7, r + 7);
    }
    if (s.dashedRing.isValid()) {
        QPen ring(s.dashedRing, 1.6, Qt::DashLine);
        ring.setDashPattern({2.5, 2.5});
        p.setPen(ring);
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(c, r + 6, r + 6);
    }
    p.setPen(QPen(s.border, s.borderWidth));
    p.setBrush(s.fill);
    p.drawEllipse(c, r, r);

    const QString text = QString::number(key);
    const int pixelSize = text.size() >= 5 ? 11 : (text.size() == 4 ? 12 : 14);
    p.setFont(pixelFont(palette::uiFont(), static_cast<int>(pixelSize * s.scale), QFont::DemiBold));
    p.setPen(s.text);
    p.drawText(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r), Qt::AlignCenter, text);
    p.restore();
}

void TreeCanvas::drawNodes(QPainter& p) const
{
    // Removed nodes shrink and fade out in red during the transition.
    if (progress_ < 1.0) {
        for (const SnapshotNode& old : fromSnapshot_.nodes) {
            if (snapshot_.find(old.id))
                continue;
            const auto it = fromPositions_.find(old.id);
            if (it == fromPositions_.end())
                continue;
            NodeStyle s{palette::errorFill, palette::error, 2.0, palette::textPrimary, {}, {}, 1.0 - progress_,
                        1.0 - 0.3 * progress_};
            drawNode(p, it->second, old.key, s);
        }
    }

    // Plain nodes first, highlighted ones on top.
    auto isHighlighted = [this](NodeId id) {
        return event_ && (id == event_->nodeId || id == event_->secondaryId);
    };
    for (int pass = 0; pass < 2; ++pass) {
        for (const SnapshotNode& node : snapshot_.nodes) {
            if (isHighlighted(node.id) != (pass == 1))
                continue;
            NodeStyle s = styleFor(node);
            if (isNewNode(node.id)) {
                s.opacity = progress_;
                s.scale = 0.55 + 0.45 * progress_;
            }
            const QPointF c = animatedPosition(node.id);
            drawNode(p, c, node.key, s);
            if (node.id == selected_) {
                QPen pen(withAlpha(palette::textPrimary, 0.55), 1.2, Qt::DotLine);
                p.setPen(pen);
                p.setBrush(Qt::NoBrush);
                const qreal r = layout_.metrics().nodeRadius + 11;
                p.drawEllipse(c, r, r);
            }
        }
    }
}

void TreeCanvas::drawBadges(QPainter& p) const
{
    const qreal r = layout_.metrics().nodeRadius;

    if (snapshot_.root != kNoNode) {
        const QPointF root = animatedPosition(snapshot_.root);
        QFont f = pixelFont(palette::uiFont(), 9, QFont::DemiBold);
        f.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
        p.setFont(f);
        p.setPen(palette::textMuted);
        p.drawText(QRectF(root.x() - 40, root.y() - r - 22, 80, 14), Qt::AlignCenter, QStringLiteral("ROOT"));
    }
    if (!event_)
        return;

    // Order badges on nodes already output by a traversal.
    p.setFont(pixelFont(palette::uiFont(), 9, QFont::Bold));
    for (std::size_t i = 0; i < event_->completed.size(); ++i) {
        const NodeId id = event_->completed[i];
        if (!layout_.contains(id))
            continue;
        const QPointF c = animatedPosition(id) + QPointF(r * 0.78, -r * 0.78);
        p.setPen(Qt::NoPen);
        p.setBrush(palette::success);
        p.drawEllipse(c, 8, 8);
        p.setPen(palette::background);
        p.drawText(QRectF(c.x() - 8, c.y() - 8, 16, 16), Qt::AlignCenter, QString::number(i + 1));
    }

    // Role tag under the secondary node.
    if (event_->secondaryRole != SecondaryRole::None && layout_.contains(event_->secondaryId)) {
        const QPointF c = animatedPosition(event_->secondaryId);
        p.setFont(pixelFont(palette::uiFont(), 10, QFont::Medium));
        p.setPen(event_->secondaryRole == SecondaryRole::Target ? palette::error : palette::textSecondary);
        p.drawText(QRectF(c.x() - 50, c.y() + r + 8, 100, 14), Qt::AlignCenter,
                   QString::fromLatin1(roleLabel(event_->secondaryRole)));
    }

    // Comparison badge next to the node being compared.
    const std::string comparison = comparisonText(*event_);
    if (!comparison.empty() && layout_.contains(event_->nodeId)) {
        const QPointF c = animatedPosition(event_->nodeId);
        const QString text = QString::fromStdString(comparison);
        const QFont f = pixelFont(palette::monoFont(), 12, QFont::DemiBold);
        const QFontMetricsF fm(f);
        const QRectF pill(c.x() + r + 6, c.y() - r - 16, fm.horizontalAdvance(text) + 14, fm.height() + 6);
        const QColor tone = *event_->comparedKey == event_->key ? palette::success : palette::accent;
        p.setPen(QPen(withAlpha(tone, 0.55), 1.0));
        p.setBrush(palette::elevated);
        p.drawRoundedRect(pill, 4, 4);
        p.setFont(f);
        p.setPen(tone);
        p.drawText(pill, Qt::AlignCenter, text);
    }
}

void TreeCanvas::drawOverlays(QPainter& p) const
{
    const QRectF area = QRectF(rect()).adjusted(16, 14, -16, -14);

    if (snapshot_.empty() && progress_ >= 1.0) {
        p.setFont(palette::uiFont(13, QFont::Medium));
        p.setPen(palette::textSecondary);
        p.drawText(QRectF(rect()).adjusted(0, -20, 0, -20), Qt::AlignCenter, tr("The tree is empty"));
        p.setFont(palette::uiFont(10));
        p.setPen(palette::textMuted);
        p.drawText(QRectF(rect()).adjusted(0, 24, 0, 24), Qt::AlignCenter,
                   tr("Insert a key, or open Overview to build or load the sample tree."));
    }
    if (!event_)
        return;

    if (isTraversal(event_->operation)) {
        // A labelled strip of keys anchored at the bottom-left; `bottom` is its lower edge.
        auto drawStrip = [&](const QString& label, const QString& text, const QColor& tone, qreal bottom) {
            QFont lf = palette::uiFont(8, QFont::DemiBold);
            lf.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
            const QFont vf = palette::monoFont(10.5, QFont::Medium);
            const QFontMetricsF lfm(lf);
            const QFontMetricsF vfm(vf);
            const QString values = vfm.elidedText(text, Qt::ElideLeft, area.width() * 0.75);
            const qreal labelWidth = lfm.horizontalAdvance(label);
            const QRectF strip(area.left(), bottom - 34, labelWidth + vfm.horizontalAdvance(values) + 40, 34);
            p.setPen(QPen(palette::border, 1));
            p.setBrush(palette::surface);
            p.drawRoundedRect(strip, 5, 5);
            p.setFont(lf);
            p.setPen(palette::textMuted);
            p.drawText(strip.adjusted(14, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, label);
            p.setFont(vf);
            p.setPen(tone);
            p.drawText(strip.adjusted(26 + labelWidth, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, values);
        };

        QStringList keys;
        for (int key : event_->sequence)
            keys << QString::number(key);
        drawStrip(tr("OUTPUT"), keys.isEmpty() ? QStringLiteral("—") : keys.join(QStringLiteral("  →  ")),
                  palette::success, area.bottom());

        if (event_->operation == OperationType::LevelOrderTraversal) {
            QStringList waiting;
            for (NodeId id : event_->queue) {
                if (const SnapshotNode* node = snapshot_.find(id))
                    waiting << QString::number(node->key);
            }
            drawStrip(tr("QUEUE  FRONT ›"), waiting.isEmpty() ? tr("empty") : waiting.join(QStringLiteral("   ")),
                      palette::accent, area.bottom() - 42);
        }
    }

    // Colour legend, bottom-right, only when there is room.
    if (width() >= 760) {
        struct Item {
            QColor color;
            const char* text;
        };
        const Item items[] = {{palette::accent, "Current / path"},
                              {palette::success, "Found / output"},
                              {palette::error, "Removing / missing"}};
        p.setFont(palette::uiFont(8.5));
        const QFontMetricsF fm(p.font());
        qreal x = area.right();
        for (auto it = std::rbegin(items); it != std::rend(items); ++it) {
            const QString text = tr(it->text);
            const qreal tw = fm.horizontalAdvance(text);
            x -= tw;
            p.setPen(palette::textMuted);
            p.drawText(QPointF(x, area.bottom() - 13), text);
            x -= 14;
            p.setPen(Qt::NoPen);
            p.setBrush(it->color);
            p.drawEllipse(QPointF(x + 5, area.bottom() - 17), 4, 4);
            x -= 18;
        }
    }
}

} // namespace vdsa
