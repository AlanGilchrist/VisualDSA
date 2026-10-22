#pragma once

#include "AlgorithmEvent.h"
#include "TreeSnapshot.h"

#include <QPointF>
#include <QRectF>

#include <unordered_map>

namespace vdsa {

struct LayoutMetrics {
    qreal columnWidth = 54.0; // horizontal distance between inorder neighbours
    qreal levelHeight = 82.0; // vertical distance between depths
    qreal nodeRadius = 21.0;
};

// Computes node positions in world coordinates from a tree snapshot.
//
// x = inorder rank * columnWidth, y = depth * levelHeight.
// Because every node gets its own inorder column, no two nodes can overlap
// at any depth, subtrees never interleave, and the horizontal order of the
// nodes on screen is exactly the sorted order of the keys.
class TreeLayout {
public:
    static TreeLayout compute(const TreeSnapshot& snapshot, const LayoutMetrics& metrics = {});

    bool contains(NodeId id) const { return positions_.count(id) != 0; }
    QPointF position(NodeId id) const;
    // Where an absent child of `parent` on `side` would be drawn.
    QPointF slotPosition(NodeId parent, Direction side) const;
    // Bounding box of all node circles (empty rect for an empty tree).
    QRectF bounds() const { return bounds_; }
    const LayoutMetrics& metrics() const { return metrics_; }
    const std::unordered_map<NodeId, QPointF>& positions() const { return positions_; }

private:
    LayoutMetrics metrics_;
    std::unordered_map<NodeId, QPointF> positions_;
    QRectF bounds_;
};

} // namespace vdsa
