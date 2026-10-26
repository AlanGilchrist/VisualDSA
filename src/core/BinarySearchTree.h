#pragma once

#include "AlgorithmEvent.h"
#include "BSTNode.h"
#include "OperationResult.h"
#include "TreeSnapshot.h"
#include "TreeValidator.h"

#include <cstddef>
#include <limits>
#include <memory>
#include <vector>

namespace vdsa {

class Recorder; // defined in BinarySearchTree.cpp

// Binary Search Tree of unique int keys.
//
// Every public operation runs the real algorithm on the real tree and
// returns an OperationRecord describing each step it took. The GUI only
// replays those records; it never mutates the tree itself.
//
// Duplicate policy: duplicate keys are rejected.
class BinarySearchTree {
public:
    BinarySearchTree() = default;
    ~BinarySearchTree();

    BinarySearchTree(const BinarySearchTree&) = delete;
    BinarySearchTree& operator=(const BinarySearchTree&) = delete;
    BinarySearchTree(BinarySearchTree&& other) noexcept;
    BinarySearchTree& operator=(BinarySearchTree&& other) noexcept;

    // --- The required operations (recorded) ---
    OperationRecord insert(int key);
    OperationRecord remove(int key);
    OperationRecord search(int key) const;
    // Depth-first (inorder / preorder / postorder) or breadth-first (level order).
    OperationRecord traverse(TraversalOrder order) const;

    // --- Tree setup (recorded) ---
    // Replaces the whole tree with `keys` inserted in order. Rejected, with
    // the current tree untouched, if keys contain duplicates or exceed capacity.
    OperationRecord rebuild(const std::vector<int>& keys);
    OperationRecord clearTree();

    // --- Queries ---
    bool contains(int key) const;
    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    int height() const; // edges; -1 for an empty tree
    const BSTNode* root() const { return root_.get(); }
    std::vector<int> inorder() const;
    std::vector<int> preorder() const;
    std::vector<int> postorder() const;
    std::vector<int> levelOrder() const;
    TreeSnapshot snapshot() const;
    ValidationReport validate() const;

    std::size_t capacity() const { return capacity_; }
    void setCapacity(std::size_t capacity) { capacity_ = capacity; }

    // Deep copy that preserves node ids.
    BinarySearchTree clone() const;

    // Releases all nodes iteratively (safe for very deep, skewed trees).
    void clear() noexcept;

    static const std::vector<int>& sampleKeys(); // 50, 30, 70, 20, 40, 60, 80

private:
    ResultCode insertSteps(int key, Recorder& rec);
    ResultCode insertQuiet(int key);
    ResultCode removeSteps(int key, Recorder& rec);
    void verifyInvariant() const; // throws std::logic_error if violated

    std::unique_ptr<BSTNode> root_;
    std::size_t size_ = 0;
    NodeId nextId_ = 0;
    std::size_t capacity_ = std::numeric_limits<std::size_t>::max();
};

} // namespace vdsa
