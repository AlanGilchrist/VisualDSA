#pragma once

#include "BSTNode.h"

#include <optional>
#include <string>
#include <vector>

namespace vdsa {

enum class OperationType {
    Insert,
    Delete,
    Search,
    InorderTraversal,
    PreorderTraversal,
    PostorderTraversal,
    LevelOrderTraversal,
    Build,
    Clear,
};

// Inorder, preorder and postorder are depth-first; LevelOrder is breadth-first (BFS).
enum class TraversalOrder { Inorder, Preorder, Postorder, LevelOrder };

enum class EventType {
    OperationStarted,
    NodeVisited,
    KeyCompared,
    MoveLeft,
    MoveRight,
    NodeInserted,
    NodeFound,
    NodeNotFound,
    DeletionCaseIdentified,
    SuccessorSearchStarted,
    SuccessorFound,
    NodeRemoved,
    KeyUpdated,
    TraversalNodeVisited,
    NodeEnqueued,
    NodeDequeued,
    OperationCompleted,
    OperationRejected,
    OperationFailed,
};

enum class Direction { None, Left, Right };

// Semantic emphasis of the primary node in an event. The visualizer maps
// these to colours; the core never deals with colours itself.
enum class Emphasis { Neutral, Active, Success, Error, Removing };

enum class DeletionCase { None, Leaf, OneChild, TwoChildren };

// Why a second node is highlighted alongside the primary one.
enum class SecondaryRole { None, NextNode, OnlyChild, Successor, Parent, Target, Enqueued };

// Stable identifiers for pseudocode lines. Events refer to these ids, and
// the pseudocode panel highlights the line with the matching id, so the
// highlight never depends on matching text.
enum class PseudoLine {
    None,
    // Insertion
    InsInit, InsCompare, InsDuplicate, InsGoLeft, InsGoRight, InsAttach, InsDone,
    // Search
    SrchInit, SrchCompare, SrchFound, SrchGoLeft, SrchGoRight, SrchNotFound,
    // Deletion
    DelSearch, DelNotFound, DelCaseLeaf, DelRemoveLeaf, DelCaseOne, DelSpliceChild,
    DelCaseTwo, DelFindSucc, DelCopyKey, DelRemoveSucc, DelVerify,
    // Depth-first traversals (shared ids, arranged differently per order)
    TrvStart, TrvDone, TrvCall, TrvNull, TrvLeft, TrvVisit, TrvRight,
    // Breadth-first (level-order) traversal
    BfsEmpty, BfsInit, BfsDequeue, BfsVisit, BfsEnqueueLeft, BfsEnqueueRight, BfsDone,
    // Build / clear
    BldValidate, BldInsert, BldDone, ClrRelease, ClrDone,
};

// Counters measured while the algorithm runs. Values in an event are
// cumulative up to and including that event.
struct OperationMetrics {
    int nodesVisited = 0;      // nodes the algorithm stepped onto
    int keyComparisons = 0;    // three-way key comparisons (one per search-path node)
    int traversalVisits = 0;   // keys emitted by a traversal
    int structuralChanges = 0; // pointer re-links and key writes on the tree
};

// One recorded step of an algorithm. Everything the visualizer needs to draw
// this step is carried here; explanations are generated from the same data.
struct AlgorithmEvent {
    EventType type = EventType::OperationStarted;
    OperationType operation = OperationType::Insert;
    PseudoLine line = PseudoLine::None;
    int snapshotIndex = 0; // index into OperationRecord::snapshots

    NodeId nodeId = kNoNode; // primary node (may refer to a node removed in this step)
    Emphasis emphasis = Emphasis::Active;
    int key = 0;                    // key being processed
    std::optional<int> comparedKey; // key it was compared against, if any
    Direction direction = Direction::None;

    NodeId secondaryId = kNoNode;
    SecondaryRole secondaryRole = SecondaryRole::None;

    // An empty child slot relevant to this step (insertion point / dead end).
    NodeId pendingParent = kNoNode;
    Direction pendingSide = Direction::None;

    DeletionCase deletionCase = DeletionCase::None;

    std::vector<NodeId> path;      // active path from the root to the current node
    std::vector<NodeId> completed; // traversal: nodes already output, in order
    std::vector<int> sequence;     // traversal: keys output so far
    std::vector<NodeId> queue;     // level-order traversal: nodes waiting, front first
    OperationMetrics metrics;

    std::string action; // what the algorithm does in this step
    std::string reason; // why it does it
};

inline const char* operationName(OperationType op)
{
    switch (op) {
    case OperationType::Insert: return "Insert";
    case OperationType::Delete: return "Delete";
    case OperationType::Search: return "Search";
    case OperationType::InorderTraversal: return "Inorder traversal";
    case OperationType::PreorderTraversal: return "Preorder traversal";
    case OperationType::PostorderTraversal: return "Postorder traversal";
    case OperationType::LevelOrderTraversal: return "Level-order traversal (BFS)";
    case OperationType::Build: return "Build tree";
    case OperationType::Clear: return "Clear tree";
    }
    return "Operation";
}

inline bool isTraversal(OperationType op)
{
    return op == OperationType::InorderTraversal || op == OperationType::PreorderTraversal
        || op == OperationType::PostorderTraversal || op == OperationType::LevelOrderTraversal;
}

inline OperationType traversalOperation(TraversalOrder order)
{
    switch (order) {
    case TraversalOrder::Inorder: return OperationType::InorderTraversal;
    case TraversalOrder::Preorder: return OperationType::PreorderTraversal;
    case TraversalOrder::Postorder: return OperationType::PostorderTraversal;
    case TraversalOrder::LevelOrder: return OperationType::LevelOrderTraversal;
    }
    return OperationType::InorderTraversal;
}

inline const char* directionName(Direction d)
{
    return d == Direction::Left ? "left" : (d == Direction::Right ? "right" : "none");
}

// "45 > 30" style comparison text for events that compared two keys.
inline std::string comparisonText(const AlgorithmEvent& e)
{
    if (!e.comparedKey)
        return {};
    const int other = *e.comparedKey;
    const char* op = e.key < other ? " < " : (e.key > other ? " > " : " = ");
    return std::to_string(e.key) + op + std::to_string(other);
}

} // namespace vdsa
