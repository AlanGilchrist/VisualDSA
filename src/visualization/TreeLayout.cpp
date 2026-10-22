#include "TreeLayout.h"

namespace vdsa {

TreeLayout TreeLayout::compute(const TreeSnapshot& snapshot, const LayoutMetrics& metrics)
{
    TreeLayout layout;
    layout.metrics_ = metrics;
    const std::vector<NodeId> order = snapshot.inorderIds();
    for (std::size_t rank = 0; rank < order.size(); ++rank) {
        const SnapshotNode* node = snapshot.find(order[rank]);
        const QPointF pos(static_cast<qreal>(rank) * metrics.columnWidth, node->depth * metrics.levelHeight);
        layout.positions_[node->id] = pos;
        const qreal r = metrics.nodeRadius;
        const QRectF circle(pos.x() - r, pos.y() - r, 2 * r, 2 * r);
        layout.bounds_ = layout.bounds_.isNull() ? circle : layout.bounds_.united(circle);
    }
    return layout;
}

QPointF TreeLayout::position(NodeId id) const
{
    const auto it = positions_.find(id);
    return it != positions_.end() ? it->second : QPointF();
}

QPointF TreeLayout::slotPosition(NodeId parent, Direction side) const
{
    const QPointF p = position(parent);
    const qreal dx = metrics_.columnWidth * 0.5;
    return {side == Direction::Left ? p.x() - dx : p.x() + dx, p.y() + metrics_.levelHeight};
}

} // namespace vdsa
