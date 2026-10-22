#pragma once

#include "BSTNode.h"

#include <algorithm>
#include <vector>

namespace vdsa {

// One node of a snapshot. Relationships are stored as ids, never pointers,
// so a snapshot stays valid no matter what happens to the live tree later.
struct SnapshotNode {
    NodeId id = kNoNode;
    int key = 0;
    NodeId parent = kNoNode;
    NodeId left = kNoNode;
    NodeId right = kNoNode;
    int depth = 0;
};

// Immutable, pointer-free copy of a tree's structure at one moment.
// Nodes are stored in preorder (root first).
struct TreeSnapshot {
    NodeId root = kNoNode;
    std::vector<SnapshotNode> nodes;

    bool empty() const { return nodes.empty(); }
    int size() const { return static_cast<int>(nodes.size()); }

    const SnapshotNode* find(NodeId id) const
    {
        if (id == kNoNode)
            return nullptr;
        for (const SnapshotNode& node : nodes) {
            if (node.id == id)
                return &node;
        }
        return nullptr;
    }

    const SnapshotNode* findKey(int key) const
    {
        for (const SnapshotNode& node : nodes) {
            if (node.key == key)
                return &node;
        }
        return nullptr;
    }

    // Height measured in edges: empty tree = -1, single node = 0.
    int height() const
    {
        int result = -1;
        for (const SnapshotNode& node : nodes)
            result = std::max(result, node.depth);
        return result;
    }

    int edgeCount() const { return nodes.empty() ? 0 : size() - 1; }

    // Node ids in inorder (ascending key) order, computed iteratively.
    std::vector<NodeId> inorderIds() const
    {
        std::vector<NodeId> order;
        order.reserve(nodes.size());
        std::vector<NodeId> stack;
        NodeId current = root;
        while (current != kNoNode || !stack.empty()) {
            while (current != kNoNode) {
                stack.push_back(current);
                current = find(current)->left;
            }
            current = stack.back();
            stack.pop_back();
            order.push_back(current);
            current = find(current)->right;
        }
        return order;
    }

    std::vector<int> inorderKeys() const
    {
        std::vector<int> keys;
        for (NodeId id : inorderIds())
            keys.push_back(find(id)->key);
        return keys;
    }

    std::vector<int> preorderKeys() const
    {
        std::vector<int> keys;
        for (const SnapshotNode& node : nodes)
            keys.push_back(node.key);
        return keys;
    }

    // Ids from the root down to (and including) the given node.
    std::vector<NodeId> pathTo(NodeId id) const
    {
        std::vector<NodeId> path;
        for (const SnapshotNode* node = find(id); node; node = find(node->parent))
            path.push_back(node->id);
        std::reverse(path.begin(), path.end());
        return path;
    }

    int subtreeSize(NodeId id) const
    {
        const SnapshotNode* node = find(id);
        if (!node)
            return 0;
        return 1 + subtreeSize(node->left) + subtreeSize(node->right);
    }
};

} // namespace vdsa
