#pragma once

#include "BSTNode.h"
#include "TreeSnapshot.h"

#include <algorithm>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace vdsa {

struct ValidationReport {
    bool valid = true;
    std::string error;
    int nodeCount = 0;
    int height = -1; // edges on the longest root-to-leaf path; -1 for an empty tree
};

namespace detail {
inline ValidationReport fail(ValidationReport report, std::string message)
{
    report.valid = false;
    report.error = std::move(message);
    return report;
}
} // namespace detail

// Checks the BST invariant on a live tree:
//  * every key in a left subtree is smaller than its ancestor's key,
//  * every key in a right subtree is larger,
//  * keys and node ids are unique,
//  * no node is reachable twice (no cycles / shared ownership).
// Implemented iteratively with an explicit (lower, upper) bound per node, so
// it checks the whole-subtree rule, not just parent/child pairs.
inline ValidationReport validateTree(const BSTNode* root)
{
    ValidationReport report;
    struct Frame {
        const BSTNode* node;
        std::optional<int> lower;
        std::optional<int> upper;
        int depth;
    };
    std::unordered_set<const BSTNode*> seenNodes;
    std::unordered_set<NodeId> seenIds;
    std::unordered_set<int> seenKeys;
    std::vector<Frame> stack;
    if (root)
        stack.push_back({root, std::nullopt, std::nullopt, 0});

    while (!stack.empty()) {
        const Frame frame = stack.back();
        stack.pop_back();
        const BSTNode* node = frame.node;
        const std::string keyText = std::to_string(node->key);

        if (!seenNodes.insert(node).second)
            return detail::fail(report, "node " + keyText + " is reachable twice (cycle or shared ownership)");
        if (!seenIds.insert(node->id).second)
            return detail::fail(report, "node id " + std::to_string(node->id) + " is used twice");
        if (!seenKeys.insert(node->key).second)
            return detail::fail(report, "duplicate key " + keyText);
        if (frame.lower && node->key <= *frame.lower)
            return detail::fail(report, "key " + keyText + " must be greater than " + std::to_string(*frame.lower));
        if (frame.upper && node->key >= *frame.upper)
            return detail::fail(report, "key " + keyText + " must be less than " + std::to_string(*frame.upper));

        ++report.nodeCount;
        report.height = std::max(report.height, frame.depth);
        if (node->left)
            stack.push_back({node->left.get(), frame.lower, node->key, frame.depth + 1});
        if (node->right)
            stack.push_back({node->right.get(), node->key, frame.upper, frame.depth + 1});
    }
    return report;
}

// Checks that a snapshot is internally consistent (parent/child links agree,
// depths are correct, every node is reachable from the root) and that it
// satisfies the BST invariant.
inline ValidationReport validateSnapshot(const TreeSnapshot& snapshot)
{
    ValidationReport report;
    if (snapshot.root == kNoNode) {
        if (!snapshot.nodes.empty())
            return detail::fail(report, "snapshot has nodes but no root");
        return report;
    }
    const SnapshotNode* root = snapshot.find(snapshot.root);
    if (!root)
        return detail::fail(report, "root id not present in snapshot");
    if (root->parent != kNoNode || root->depth != 0)
        return detail::fail(report, "root must have no parent and depth 0");

    struct Frame {
        const SnapshotNode* node;
        std::optional<int> lower;
        std::optional<int> upper;
    };
    std::unordered_set<NodeId> seen;
    std::unordered_set<int> keys;
    std::vector<Frame> stack{{root, std::nullopt, std::nullopt}};
    while (!stack.empty()) {
        const Frame frame = stack.back();
        stack.pop_back();
        const SnapshotNode* node = frame.node;
        if (!seen.insert(node->id).second)
            return detail::fail(report, "snapshot node reachable twice");
        if (!keys.insert(node->key).second)
            return detail::fail(report, "duplicate key " + std::to_string(node->key));
        if ((frame.lower && node->key <= *frame.lower) || (frame.upper && node->key >= *frame.upper))
            return detail::fail(report, "ordering violated at key " + std::to_string(node->key));
        ++report.nodeCount;
        report.height = std::max(report.height, node->depth);

        for (NodeId childId : {node->left, node->right}) {
            if (childId == kNoNode)
                continue;
            const SnapshotNode* child = snapshot.find(childId);
            if (!child)
                return detail::fail(report, "child id missing from snapshot");
            if (child->parent != node->id || child->depth != node->depth + 1)
                return detail::fail(report, "parent link or depth inconsistent at key " + std::to_string(child->key));
            const bool isLeft = childId == node->left;
            stack.push_back({child, isLeft ? frame.lower : std::optional<int>(node->key),
                             isLeft ? std::optional<int>(node->key) : frame.upper});
        }
    }
    if (report.nodeCount != snapshot.size())
        return detail::fail(report, "snapshot contains unreachable nodes");
    return report;
}

} // namespace vdsa
