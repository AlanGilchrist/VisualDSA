#pragma once

#include "AlgorithmEvent.h"
#include "TreeLayout.h"
#include "TreeSnapshot.h"

#include <QWidget>

#include <optional>
#include <unordered_map>

class QVariantAnimation;

namespace vdsa {

// Custom QPainter widget that draws a tree snapshot and the highlights of
// one recorded algorithm step. Structural changes between consecutive frames
// are animated by node id: surviving nodes slide to their new positions,
// new nodes grow in, removed nodes fade out in red.
class TreeCanvas : public QWidget {
    Q_OBJECT

public:
    static constexpr qreal kMinZoom = 0.25;
    static constexpr qreal kMaxZoom = 3.0;

    explicit TreeCanvas(QWidget* parent = nullptr);

    // A tree with no algorithm step highlighted.
    void showTree(const TreeSnapshot& snapshot, int transitionMs = 0);
    // One recorded step: the snapshot it refers to plus its highlights.
    void showStep(const TreeSnapshot& snapshot, const AlgorithmEvent& event, int transitionMs);

    void zoomIn();
    void zoomOut();
    void fitToView();
    void resetZoom();
    qreal zoom() const { return zoom_; }
    bool isAutoFit() const { return autoFit_; }

    // Inspection (used by the main window and by tests).
    const TreeSnapshot& snapshot() const { return snapshot_; }
    int nodeCount() const { return snapshot_.size(); }
    int edgeCount() const { return snapshot_.edgeCount(); }
    const AlgorithmEvent* currentEvent() const { return event_ ? &*event_ : nullptr; }
    NodeId highlightedNode() const { return event_ ? event_->nodeId : kNoNode; }
    bool isAnimating() const;
    NodeId selectedNode() const { return selected_; }
    void setSelectedNode(NodeId id);
    QPointF nodeScreenPosition(NodeId id) const;
    NodeId nodeAt(const QPointF& screenPos) const;

    QSize sizeHint() const override { return {720, 480}; }
    QSize minimumSizeHint() const override { return {360, 260}; }

signals:
    void nodeSelected(vdsa::NodeId id);
    void zoomChanged(qreal zoom);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    struct NodeStyle {
        QColor fill;
        QColor border;
        qreal borderWidth = 1.5;
        QColor text;
        QColor halo;       // invalid = none
        QColor dashedRing; // invalid = none
        qreal opacity = 1.0;
        qreal scale = 1.0;
    };

    void setFrame(const TreeSnapshot& snapshot, std::optional<AlgorithmEvent> event, int transitionMs);
    QPointF animatedPosition(NodeId id) const;
    bool isNewNode(NodeId id) const;
    QRectF contentBounds() const;
    QRectF viewportArea() const;
    QTransform worldTransform() const;
    void applyFit();
    void setZoomAround(qreal zoom, const QPointF& anchor);

    void drawGrid(QPainter& p) const;
    void drawEdges(QPainter& p) const;
    void drawPendingSlot(QPainter& p) const;
    void drawNodes(QPainter& p) const;
    void drawNode(QPainter& p, const QPointF& center, int key, const NodeStyle& style) const;
    void drawBadges(QPainter& p) const;
    void drawOverlays(QPainter& p) const;
    NodeStyle styleFor(const SnapshotNode& node) const;

    TreeSnapshot snapshot_;
    TreeLayout layout_;
    TreeSnapshot fromSnapshot_;
    std::unordered_map<NodeId, QPointF> fromPositions_;
    std::optional<AlgorithmEvent> event_;
    QVariantAnimation* transition_ = nullptr;
    qreal progress_ = 1.0;

    qreal zoom_ = 1.0;
    QPointF pan_;
    QPointF worldCenter_;
    bool autoFit_ = true;
    bool dragging_ = false;
    QPoint pressPos_;
    QPoint lastMousePos_;
    NodeId selected_ = kNoNode;
};

} // namespace vdsa
