#pragma once

#include <memory>

namespace vdsa {

// Stable identifier for a node. The visualizer tracks nodes by id (not by
// pointer or key) so a node can be animated across structural changes and
// historical snapshots remain meaningful after the live tree changes.
using NodeId = int;
inline constexpr NodeId kNoNode = -1;

// A single Binary Search Tree node. Each node owns its children through
// std::unique_ptr, so every node has exactly one owner and is freed exactly
// once. No GUI state is stored here.
struct BSTNode {
    BSTNode(int nodeKey, NodeId nodeId) : key(nodeKey), id(nodeId) {}

    int key;
    NodeId id;
    std::unique_ptr<BSTNode> left;
    std::unique_ptr<BSTNode> right;

    bool isLeaf() const { return !left && !right; }
    int childCount() const { return (left ? 1 : 0) + (right ? 1 : 0); }
};

} // namespace vdsa
