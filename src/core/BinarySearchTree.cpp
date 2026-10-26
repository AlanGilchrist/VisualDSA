#include "BinarySearchTree.h"

#include <algorithm>
#include <deque>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace vdsa {

namespace {

template <typename... Args>
std::string cat(const Args&... args)
{
    std::ostringstream out;
    (out << ... << args);
    return out.str();
}

std::string joinKeys(const std::vector<int>& keys, const char* separator = ", ")
{
    std::string text;
    for (std::size_t i = 0; i < keys.size(); ++i) {
        if (i > 0)
            text += separator;
        text += std::to_string(keys[i]);
    }
    return text;
}

struct DescentLines {
    PseudoLine compare;
    PseudoLine goLeft;
    PseudoLine goRight;
};

struct DescentResult {
    bool found = false;
    NodeId parentId = kNoNode;
    int parentKey = 0;
    Direction side = Direction::None;
};

std::unique_ptr<BSTNode> copySubtree(const BSTNode* node)
{
    if (!node)
        return nullptr;
    auto copy = std::make_unique<BSTNode>(node->key, node->id);
    copy->left = copySubtree(node->left.get());
    copy->right = copySubtree(node->right.get());
    return copy;
}

} // namespace

// Collects the events and snapshots of one operation. Path, sequence, queue
// and metrics are running state that gets copied into each emitted event.
//
// A non-recording Recorder (record = false) lets the same algorithm code run
// without storing events or snapshots, e.g. for bulk tree construction.
class Recorder {
public:
    explicit Recorder(OperationType op, bool record = true) : recording_(record), operation_(op)
    {
        record_.result.operation = op;
    }

    void capture(const TreeSnapshot& snapshot)
    {
        if (recording_)
            record_.snapshots.push_back(snapshot);
    }
    void capture(const BinarySearchTree& tree)
    {
        if (recording_)
            record_.snapshots.push_back(tree.snapshot());
    }

    AlgorithmEvent& emit(EventType type, PseudoLine line, NodeId node = kNoNode)
    {
        if (!recording_) {
            scratch_ = AlgorithmEvent{};
            return scratch_;
        }
        AlgorithmEvent event;
        event.type = type;
        event.operation = operation_;
        event.line = line;
        event.snapshotIndex = static_cast<int>(record_.snapshots.size()) - 1;
        event.nodeId = node;
        event.path = path;
        event.completed = completed;
        event.sequence = sequence;
        event.queue = queue;
        event.metrics = metrics;
        record_.events.push_back(std::move(event));
        return record_.events.back();
    }

    OperationResult& result() { return record_.result; }
    bool recording() const { return recording_; }

    OperationRecord finish(ResultStatus status, ResultCode code, std::string message)
    {
        record_.result.status = status;
        record_.result.code = code;
        record_.result.message = std::move(message);
        record_.result.metrics = metrics;
        record_.result.sequence = sequence;
        return std::move(record_);
    }

    OperationMetrics metrics;
    std::vector<NodeId> path;
    std::vector<NodeId> completed;
    std::vector<int> sequence;
    std::vector<NodeId> queue; // level-order traversal only

private:
    bool recording_;
    OperationType operation_;
    OperationRecord record_;
    AlgorithmEvent scratch_;
};

namespace {

// Standard iterative BST descent. Emits a KeyCompared and a MoveLeft/Right
// event for every node passed, and returns the owning link of either the
// node holding `key` (out.found) or the empty slot where it would belong.
// Works on const and non-const links so search() can stay const.
template <typename Link>
Link* descend(Link* link, int key, Recorder& rec, const DescentLines& lines, DescentResult& out)
{
    out = DescentResult{};
    while (*link) {
        BSTNode& node = **link;
        rec.path.push_back(node.id);
        ++rec.metrics.nodesVisited;
        ++rec.metrics.keyComparisons;
        if (key == node.key) {
            out.found = true;
            return link;
        }

        const Direction dir = key < node.key ? Direction::Left : Direction::Right;
        Link* next = dir == Direction::Left ? &node.left : &node.right;
        out.parentId = node.id;
        out.parentKey = node.key;
        out.side = dir;
        if (!rec.recording()) {
            link = next;
            continue;
        }

        const char* side = directionName(dir);
        AlgorithmEvent& cmp = rec.emit(EventType::KeyCompared, lines.compare, node.id);
        cmp.key = key;
        cmp.comparedKey = node.key;
        cmp.direction = dir;
        cmp.action = cat("Compare ", key, " with ", node.key);
        cmp.reason = cat(key, dir == Direction::Left ? " < " : " > ", node.key, ", so ", key,
                         " can only be in the ", side, " subtree of ", node.key, ".");

        AlgorithmEvent& move = rec.emit(dir == Direction::Left ? EventType::MoveLeft : EventType::MoveRight,
                                        dir == Direction::Left ? lines.goLeft : lines.goRight, node.id);
        move.key = key;
        move.direction = dir;
        if (*next) {
            move.secondaryId = (*next)->id;
            move.secondaryRole = SecondaryRole::NextNode;
            move.action = cat("Move to the ", side, " child (", (*next)->key, ")");
            move.reason = cat("The whole ", dir == Direction::Left ? "right" : "left", " subtree of ", node.key,
                              " is skipped: none of its keys can equal ", key, ".");
        } else {
            move.pendingParent = node.id;
            move.pendingSide = dir;
            move.action = cat("The ", side, " child of ", node.key, " is empty");
            move.reason = cat("The search path ends here: there is no ", side, " subtree below ", node.key, ".");
        }
        link = next;
    }
    return link;
}

const char* traversalName(TraversalOrder order)
{
    switch (order) {
    case TraversalOrder::Inorder: return "Inorder";
    case TraversalOrder::Preorder: return "Preorder";
    case TraversalOrder::Postorder: return "Postorder";
    case TraversalOrder::LevelOrder: return "Level-order";
    }
    return "";
}

// Recursive traversal. Recursion depth equals the height of the tree.
void walk(const BSTNode& node, TraversalOrder order, Recorder& rec, PseudoLine entryLine)
{
    rec.path.push_back(node.id);
    ++rec.metrics.nodesVisited;

    AlgorithmEvent& enter = rec.emit(EventType::NodeVisited, entryLine, node.id);
    enter.key = node.key;
    if (entryLine == PseudoLine::TrvCall)
        enter.action = cat("Enter the root, ", node.key);
    else
        enter.action = cat("Go to the ", entryLine == PseudoLine::TrvLeft ? "left" : "right", " child, ", node.key);
    switch (order) {
    case TraversalOrder::Inorder:
        enter.reason = node.left ? cat("Inorder finishes the left subtree of ", node.key, " before outputting it.")
                                 : cat(node.key, " has no left subtree, so it is output next.");
        break;
    case TraversalOrder::Preorder:
        enter.reason = "Preorder outputs a node as soon as it is reached, before either subtree.";
        break;
    case TraversalOrder::Postorder:
        enter.reason = node.isLeaf() ? cat(node.key, " is a leaf, so it can be output immediately.")
                                     : cat("Postorder finishes both subtrees of ", node.key, " before outputting it.");
        break;
    case TraversalOrder::LevelOrder: break; // breadth-first: see levelOrderWalk()
    }

    auto visit = [&]() {
        rec.sequence.push_back(node.key);
        rec.completed.push_back(node.id);
        ++rec.metrics.traversalVisits;
        AlgorithmEvent& out = rec.emit(EventType::TraversalNodeVisited, PseudoLine::TrvVisit, node.id);
        out.key = node.key;
        out.emphasis = Emphasis::Success;
        out.action = cat("Visit ", node.key, ": append it to the output");
        switch (order) {
        case TraversalOrder::Inorder:
            out.reason = "Its left subtree is finished; it is output before its right subtree.";
            break;
        case TraversalOrder::Preorder:
            out.reason = "Root first: it is output before its left and right subtrees.";
            break;
        case TraversalOrder::Postorder:
            out.reason = "Both of its subtrees are finished, so it is output now.";
            break;
        case TraversalOrder::LevelOrder: break;
        }
    };
    auto goLeft = [&]() {
        if (node.left)
            walk(*node.left, order, rec, PseudoLine::TrvLeft);
    };
    auto goRight = [&]() {
        if (node.right)
            walk(*node.right, order, rec, PseudoLine::TrvRight);
    };

    switch (order) {
    case TraversalOrder::Inorder: goLeft(); visit(); goRight(); break;
    case TraversalOrder::Preorder: visit(); goLeft(); goRight(); break;
    case TraversalOrder::Postorder: goLeft(); goRight(); visit(); break;
    case TraversalOrder::LevelOrder: break;
    }
    rec.path.pop_back();
}

// Breadth-first (level-order) traversal with an explicit FIFO queue. Every
// node is enqueued once and dequeued once; the queue is mirrored into
// rec.queue (front first) so each recorded step shows what is waiting.
void levelOrderWalk(const BSTNode& root, Recorder& rec)
{
    struct Entry {
        const BSTNode* node;
        int depth;
    };
    std::deque<Entry> queue{{&root, 0}};
    rec.queue = {root.id};
    int currentDepth = -1;

    while (!queue.empty()) {
        const Entry entry = queue.front();
        queue.pop_front();
        rec.queue.erase(rec.queue.begin());
        const BSTNode& node = *entry.node;
        ++rec.metrics.nodesVisited;

        AlgorithmEvent& out = rec.emit(EventType::NodeDequeued, PseudoLine::BfsDequeue, node.id);
        out.key = node.key;
        out.action = cat("Dequeue ", node.key, " (depth ", entry.depth, ")");
        if (entry.depth != currentDepth) {
            out.reason = entry.depth == 0
                             ? std::string("The root is the only node at depth 0, so it leaves the queue first.")
                             : cat("Every node at depth ", entry.depth - 1, " has left the queue, so level ",
                                   entry.depth, " starts now.");
            currentDepth = entry.depth;
        } else {
            out.reason = "It was at the front of the queue: the earliest discovered node still waiting.";
        }

        rec.sequence.push_back(node.key);
        rec.completed.push_back(node.id);
        ++rec.metrics.traversalVisits;
        AlgorithmEvent& visit = rec.emit(EventType::TraversalNodeVisited, PseudoLine::BfsVisit, node.id);
        visit.key = node.key;
        visit.emphasis = Emphasis::Success;
        visit.action = cat("Visit ", node.key, ": append it to the output");
        visit.reason = node.isLeaf()
                           ? cat("A node is output when it leaves the queue. ", node.key,
                                 " is a leaf, so nothing new is enqueued.")
                           : std::string("A node is output when it leaves the queue, so shallower nodes always "
                                         "come before deeper ones.");

        for (const BSTNode* child : {node.left.get(), node.right.get()}) {
            if (!child)
                continue;
            const bool left = child == node.left.get();
            const std::size_t waiting = queue.size();
            queue.push_back({child, entry.depth + 1});
            rec.queue.push_back(child->id);
            AlgorithmEvent& e = rec.emit(EventType::NodeEnqueued,
                                         left ? PseudoLine::BfsEnqueueLeft : PseudoLine::BfsEnqueueRight, node.id);
            e.key = child->key;
            e.secondaryId = child->id;
            e.secondaryRole = SecondaryRole::Enqueued;
            e.direction = left ? Direction::Left : Direction::Right;
            e.action = cat("Enqueue the ", left ? "left" : "right", " child of ", node.key, " (", child->key, ")");
            e.reason = waiting == 0
                           ? cat(child->key, " is at depth ", entry.depth + 1,
                                 ". The queue was empty, so it will be dequeued next.")
                           : cat(child->key, " is at depth ", entry.depth + 1, ". It joins the back of the queue, "
                                 "behind the ", waiting, " node(s) already waiting.");
        }
    }
}

// Keys grouped by depth, e.g. "[50] [30, 70] [20, 40, 60, 80]".
std::string levelsText(const TreeSnapshot& snapshot)
{
    std::vector<std::vector<int>> levels;
    for (const SnapshotNode& node : snapshot.nodes) {
        if (static_cast<std::size_t>(node.depth) >= levels.size())
            levels.resize(static_cast<std::size_t>(node.depth) + 1);
    }
    // Nodes are stored in preorder, which keeps each level in left-to-right order.
    for (const SnapshotNode& node : snapshot.nodes)
        levels[static_cast<std::size_t>(node.depth)].push_back(node.key);
    std::string text;
    for (const auto& level : levels)
        text += (text.empty() ? "[" : " [") + joinKeys(level) + "]";
    return text;
}

} // namespace

// ---------------------------------------------------------------------------
// Lifetime
// ---------------------------------------------------------------------------

BinarySearchTree::~BinarySearchTree() { clear(); }

BinarySearchTree::BinarySearchTree(BinarySearchTree&& other) noexcept
    : root_(std::move(other.root_)), size_(other.size_), nextId_(other.nextId_), capacity_(other.capacity_)
{
    other.size_ = 0;
}

BinarySearchTree& BinarySearchTree::operator=(BinarySearchTree&& other) noexcept
{
    if (this != &other) {
        clear();
        root_ = std::move(other.root_);
        size_ = other.size_;
        nextId_ = other.nextId_;
        capacity_ = other.capacity_;
        other.size_ = 0;
    }
    return *this;
}

// Destroying a unique_ptr chain recursively could overflow the stack on a
// long skewed tree, so nodes are released by repeatedly rotating the left
// child up to the root and freeing roots that have no left child.
// O(n) time, O(1) extra space, no allocation.
void BinarySearchTree::clear() noexcept
{
    while (root_) {
        if (root_->left) {
            std::unique_ptr<BSTNode> left = std::move(root_->left);
            root_->left = std::move(left->right);
            left->right = std::move(root_);
            root_ = std::move(left);
        } else {
            root_ = std::move(root_->right); // old root has no children left; it is freed here
        }
    }
    size_ = 0;
}

BinarySearchTree BinarySearchTree::clone() const
{
    BinarySearchTree copy;
    copy.root_ = copySubtree(root_.get());
    copy.size_ = size_;
    copy.nextId_ = nextId_;
    copy.capacity_ = capacity_;
    return copy;
}

const std::vector<int>& BinarySearchTree::sampleKeys()
{
    static const std::vector<int> keys = {50, 30, 70, 20, 40, 60, 80};
    return keys;
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

bool BinarySearchTree::contains(int key) const
{
    const BSTNode* node = root_.get();
    while (node) {
        if (key == node->key)
            return true;
        node = key < node->key ? node->left.get() : node->right.get();
    }
    return false;
}

int BinarySearchTree::height() const
{
    return validateTree(root_.get()).height;
}

std::vector<int> BinarySearchTree::inorder() const { return traverse(TraversalOrder::Inorder).result.sequence; }
std::vector<int> BinarySearchTree::preorder() const { return traverse(TraversalOrder::Preorder).result.sequence; }
std::vector<int> BinarySearchTree::postorder() const { return traverse(TraversalOrder::Postorder).result.sequence; }
std::vector<int> BinarySearchTree::levelOrder() const { return traverse(TraversalOrder::LevelOrder).result.sequence; }

TreeSnapshot BinarySearchTree::snapshot() const
{
    TreeSnapshot snap;
    snap.root = root_ ? root_->id : kNoNode;
    snap.nodes.reserve(size_);
    struct Frame {
        const BSTNode* node;
        NodeId parent;
        int depth;
    };
    std::vector<Frame> stack;
    if (root_)
        stack.push_back({root_.get(), kNoNode, 0});
    while (!stack.empty()) {
        const Frame frame = stack.back();
        stack.pop_back();
        const BSTNode* n = frame.node;
        snap.nodes.push_back({n->id, n->key, frame.parent, n->left ? n->left->id : kNoNode,
                              n->right ? n->right->id : kNoNode, frame.depth});
        if (n->right)
            stack.push_back({n->right.get(), n->id, frame.depth + 1});
        if (n->left)
            stack.push_back({n->left.get(), n->id, frame.depth + 1});
    }
    return snap;
}

ValidationReport BinarySearchTree::validate() const
{
    ValidationReport report = validateTree(root_.get());
    if (report.valid && static_cast<std::size_t>(report.nodeCount) != size_) {
        report.valid = false;
        report.error = cat("size counter is ", size_, " but ", report.nodeCount, " nodes are reachable");
    }
    return report;
}

void BinarySearchTree::verifyInvariant() const
{
    const ValidationReport report = validate();
    if (!report.valid)
        throw std::logic_error("BST invariant violated: " + report.error);
}

// ---------------------------------------------------------------------------
// Insertion
// ---------------------------------------------------------------------------

OperationRecord BinarySearchTree::insert(int key)
{
    Recorder rec(OperationType::Insert);
    rec.result().key = key;
    rec.capture(*this);

    if (size_ >= capacity_) {
        AlgorithmEvent& e = rec.emit(EventType::OperationRejected, PseudoLine::InsInit);
        e.key = key;
        e.emphasis = Emphasis::Error;
        e.action = "The tree is full";
        e.reason = cat("This demonstration limits the tree to ", capacity_,
                       " nodes so every node stays readable. Delete a key first.");
        return rec.finish(ResultStatus::Rejected, ResultCode::CapacityReached, "Insertion rejected: the tree is full.");
    }

    AlgorithmEvent& start = rec.emit(EventType::OperationStarted, PseudoLine::InsInit, root_ ? root_->id : kNoNode);
    start.key = key;
    if (root_) {
        start.action = cat("Insert ", key, ": start at the root (", root_->key, ")");
        start.reason = "Insertion follows the search path from the root until it reaches an empty child pointer.";
    } else {
        start.action = cat("Insert ", key, " into an empty tree");
        start.reason = "There are no nodes, so the new key becomes the root.";
    }

    const ResultCode code = insertSteps(key, rec);
    if (code != ResultCode::Ok)
        return rec.finish(ResultStatus::Rejected, code, "Key already exists.");

    verifyInvariant();
    AlgorithmEvent& done = rec.emit(EventType::OperationCompleted, PseudoLine::InsDone, rec.path.back());
    done.key = key;
    done.emphasis = Emphasis::Success;
    done.action = "Insertion completed";
    done.reason = cat("BST invariant verified. The tree now holds ", size_, " node(s) with height ", height(), ".");
    rec.result().treeChanged = true;
    return rec.finish(ResultStatus::Success, ResultCode::Ok, "Insertion completed.");
}

ResultCode BinarySearchTree::insertSteps(int key, Recorder& rec)
{
    DescentResult d;
    std::unique_ptr<BSTNode>* link =
        descend(&root_, key, rec, {PseudoLine::InsCompare, PseudoLine::InsGoLeft, PseudoLine::InsGoRight}, d);

    if (d.found) {
        const BSTNode& existing = **link;
        AlgorithmEvent& e = rec.emit(EventType::OperationRejected, PseudoLine::InsDuplicate, existing.id);
        e.key = key;
        e.comparedKey = existing.key;
        e.emphasis = Emphasis::Error;
        e.action = cat("Key ", key, " already exists");
        e.reason = "Duplicate keys are rejected so every key has exactly one correct position. "
                   "The tree is unchanged.";
        return ResultCode::DuplicateKey;
    }

    const NodeId id = nextId_++;
    *link = std::make_unique<BSTNode>(key, id);
    ++size_;
    ++rec.metrics.structuralChanges;
    rec.path.push_back(id);
    rec.capture(*this);

    AlgorithmEvent& e = rec.emit(EventType::NodeInserted, PseudoLine::InsAttach, id);
    e.key = key;
    e.emphasis = Emphasis::Success;
    e.direction = d.side;
    if (d.parentId == kNoNode) {
        e.action = cat("Insert ", key, " as the root");
        e.reason = "The tree was empty, so the new node becomes the root.";
    } else {
        e.secondaryId = d.parentId;
        e.secondaryRole = SecondaryRole::Parent;
        e.action = cat("Attach ", key, " as the ", directionName(d.side), " child of ", d.parentKey);
        e.reason = "This is the first empty position on the search path, so every ancestor's ordering rule still holds.";
    }
    return ResultCode::Ok;
}

// Same insertion algorithm, without recording (used to build whole trees).
ResultCode BinarySearchTree::insertQuiet(int key)
{
    Recorder rec(OperationType::Insert, false);
    return insertSteps(key, rec);
}

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------

OperationRecord BinarySearchTree::search(int key) const
{
    Recorder rec(OperationType::Search);
    rec.result().key = key;
    rec.capture(*this);

    if (!root_) {
        AlgorithmEvent& e = rec.emit(EventType::NodeNotFound, PseudoLine::SrchNotFound);
        e.key = key;
        e.emphasis = Emphasis::Error;
        e.action = cat("Key ", key, " not found: the tree is empty");
        e.reason = "There is no root to start from, so the key cannot be present.";
        return rec.finish(ResultStatus::NotFound, ResultCode::KeyNotFound, "Key not found.");
    }

    AlgorithmEvent& start = rec.emit(EventType::OperationStarted, PseudoLine::SrchInit, root_->id);
    start.key = key;
    start.action = cat("Search for ", key, ": start at the root (", root_->key, ")");
    start.reason = "Each comparison discards one whole subtree, so at most h + 1 nodes are examined.";

    DescentResult d;
    const std::unique_ptr<BSTNode>* link =
        descend(&root_, key, rec, {PseudoLine::SrchCompare, PseudoLine::SrchGoLeft, PseudoLine::SrchGoRight}, d);

    if (d.found) {
        const BSTNode& node = **link;
        AlgorithmEvent& e = rec.emit(EventType::NodeFound, PseudoLine::SrchFound, node.id);
        e.key = key;
        e.comparedKey = node.key;
        e.emphasis = Emphasis::Success;
        e.action = cat("Key ", key, " found");
        e.reason = cat(key, " = ", node.key, ". Found at depth ", rec.path.size() - 1, " after visiting ",
                       rec.metrics.nodesVisited, " node(s).");
        return rec.finish(ResultStatus::Success, ResultCode::KeyFound, "Key found.");
    }

    AlgorithmEvent& e = rec.emit(EventType::NodeNotFound, PseudoLine::SrchNotFound);
    e.key = key;
    e.emphasis = Emphasis::Error;
    e.pendingParent = d.parentId;
    e.pendingSide = d.side;
    e.action = cat("Key ", key, " not found");
    e.reason = cat("Reached the empty ", directionName(d.side), " pointer of ", d.parentKey, ". If ", key,
                   " existed it would have to be here. Visited ", rec.metrics.nodesVisited, " node(s).");
    return rec.finish(ResultStatus::NotFound, ResultCode::KeyNotFound, "Key not found.");
}

// ---------------------------------------------------------------------------
// Deletion
// ---------------------------------------------------------------------------

OperationRecord BinarySearchTree::remove(int key)
{
    Recorder rec(OperationType::Delete);
    rec.result().key = key;
    rec.capture(*this);

    if (!root_) {
        AlgorithmEvent& e = rec.emit(EventType::NodeNotFound, PseudoLine::DelNotFound);
        e.key = key;
        e.emphasis = Emphasis::Error;
        e.action = cat("Key ", key, " not found: the tree is empty");
        e.reason = "Nothing can be deleted from an empty tree. The tree is unchanged.";
        return rec.finish(ResultStatus::NotFound, ResultCode::KeyNotFound, "Key not found.");
    }

    AlgorithmEvent& start = rec.emit(EventType::OperationStarted, PseudoLine::DelSearch, root_->id);
    start.key = key;
    start.action = cat("Delete ", key, ": first search for it from the root (", root_->key, ")");
    start.reason = "A node can only be removed after locating it and the parent pointer that holds it.";

    const ResultCode code = removeSteps(key, rec);
    if (code != ResultCode::Ok)
        return rec.finish(ResultStatus::NotFound, code, "Key not found.");

    verifyInvariant();
    rec.path.clear();
    AlgorithmEvent& done = rec.emit(EventType::OperationCompleted, PseudoLine::DelVerify);
    done.key = key;
    done.emphasis = Emphasis::Success;
    done.deletionCase = rec.result().deletionCase;
    done.action = "Deletion completed";
    done.reason = cat("BST invariant verified. The tree now holds ", size_, " node(s).");
    rec.result().treeChanged = true;
    return rec.finish(ResultStatus::Success, ResultCode::Ok, "Deletion completed.");
}

ResultCode BinarySearchTree::removeSteps(int key, Recorder& rec)
{
    DescentResult d;
    std::unique_ptr<BSTNode>* link =
        descend(&root_, key, rec, {PseudoLine::DelSearch, PseudoLine::DelSearch, PseudoLine::DelSearch}, d);

    if (!d.found) {
        AlgorithmEvent& e = rec.emit(EventType::NodeNotFound, PseudoLine::DelNotFound);
        e.key = key;
        e.emphasis = Emphasis::Error;
        e.pendingParent = d.parentId;
        e.pendingSide = d.side;
        e.action = cat("Key ", key, " not found");
        e.reason = cat("Reached an empty pointer, so ", key, " is not in the tree. Nothing is removed.");
        return ResultCode::KeyNotFound;
    }

    BSTNode& target = **link;
    const NodeId targetId = target.id;

    AlgorithmEvent& found = rec.emit(EventType::NodeFound, PseudoLine::DelSearch, targetId);
    found.key = key;
    found.comparedKey = target.key;
    found.action = cat("Found ", key);
    found.reason = "Next, count its children to decide which of the three deletion cases applies.";

    if (target.isLeaf()) {
        rec.result().deletionCase = DeletionCase::Leaf;
        AlgorithmEvent& c = rec.emit(EventType::DeletionCaseIdentified, PseudoLine::DelCaseLeaf, targetId);
        c.key = key;
        c.emphasis = Emphasis::Removing;
        c.deletionCase = DeletionCase::Leaf;
        c.action = cat("Case 1: ", key, " is a leaf");
        c.reason = "It has no children, so removing it cannot disconnect any other node. "
                   "Only the pointer that holds it must be cleared.";

        link->reset();
        --size_;
        ++rec.metrics.structuralChanges;
        rec.path.pop_back();
        rec.capture(*this);

        AlgorithmEvent& r = rec.emit(EventType::NodeRemoved, PseudoLine::DelRemoveLeaf, targetId);
        r.key = key;
        r.emphasis = Emphasis::Removing;
        r.deletionCase = DeletionCase::Leaf;
        if (d.parentId == kNoNode) {
            r.action = cat("Removed ", key, "; the tree is now empty");
            r.reason = "It was the only node, so the root pointer becomes null.";
        } else {
            r.secondaryId = d.parentId;
            r.secondaryRole = SecondaryRole::Parent;
            r.action = cat("Removed ", key, "; the ", directionName(d.side), " pointer of ", d.parentKey, " is now null");
            r.reason = "No other node moved, so the ordering of the rest of the tree is untouched.";
        }
    } else if (target.childCount() == 1) {
        std::unique_ptr<BSTNode>& childLink = target.left ? target.left : target.right;
        const NodeId childId = childLink->id;
        const int childKey = childLink->key;
        rec.result().deletionCase = DeletionCase::OneChild;

        AlgorithmEvent& c = rec.emit(EventType::DeletionCaseIdentified, PseudoLine::DelCaseOne, targetId);
        c.key = key;
        c.emphasis = Emphasis::Removing;
        c.deletionCase = DeletionCase::OneChild;
        c.secondaryId = childId;
        c.secondaryRole = SecondaryRole::OnlyChild;
        c.action = cat("Case 2: ", key, " has one child (", childKey, ")");
        c.reason = d.parentId == kNoNode
                       ? cat("The child's subtree can simply become the whole tree.")
                       : cat("Every key below ", key, " lies on the same side of ", d.parentKey, " as ", key,
                             " itself, so the child can take its place.");

        // Move-assignment releases the child from `target` before `target`
        // itself is destroyed, so no node is ever freed twice.
        *link = std::move(childLink);
        --size_;
        ++rec.metrics.structuralChanges;
        rec.path.pop_back();
        rec.path.push_back(childId);
        rec.capture(*this);

        AlgorithmEvent& r = rec.emit(EventType::NodeRemoved, PseudoLine::DelSpliceChild, targetId);
        r.key = key;
        r.emphasis = Emphasis::Removing;
        r.deletionCase = DeletionCase::OneChild;
        r.secondaryId = childId;
        r.secondaryRole = SecondaryRole::OnlyChild;
        r.action = d.parentId == kNoNode ? cat(childKey, " is now the root")
                                         : cat("Linked ", d.parentKey, " directly to ", childKey);
        r.reason = cat(key, " is gone and the subtree rooted at ", childKey,
                       " moved up one level. Its internal shape is unchanged.");
    } else {
        rec.result().deletionCase = DeletionCase::TwoChildren;
        AlgorithmEvent& c = rec.emit(EventType::DeletionCaseIdentified, PseudoLine::DelCaseTwo, targetId);
        c.key = key;
        c.emphasis = Emphasis::Removing;
        c.deletionCase = DeletionCase::TwoChildren;
        c.action = cat("Case 3: ", key, " has two children");
        c.reason = "Removing the node would leave two subtrees competing for one pointer. Instead, its key is "
                   "replaced by its inorder successor: the smallest key in its right subtree.";

        std::unique_ptr<BSTNode>* succLink = &target.right;
        rec.path.push_back((*succLink)->id);
        ++rec.metrics.nodesVisited;
        AlgorithmEvent& s = rec.emit(EventType::SuccessorSearchStarted, PseudoLine::DelFindSucc, (*succLink)->id);
        s.key = (*succLink)->key;
        s.deletionCase = DeletionCase::TwoChildren;
        s.secondaryId = targetId;
        s.secondaryRole = SecondaryRole::Target;
        s.action = cat("Step into the right subtree of ", key, " (", (*succLink)->key, ")");
        s.reason = cat("The successor is the next larger key, so it must be in the right subtree of ", key, ".");

        while ((*succLink)->left) {
            succLink = &(*succLink)->left;
            rec.path.push_back((*succLink)->id);
            ++rec.metrics.nodesVisited;
            AlgorithmEvent& v = rec.emit(EventType::NodeVisited, PseudoLine::DelFindSucc, (*succLink)->id);
            v.key = (*succLink)->key;
            v.deletionCase = DeletionCase::TwoChildren;
            v.secondaryId = targetId;
            v.secondaryRole = SecondaryRole::Target;
            v.action = cat("Move left to ", (*succLink)->key);
            v.reason = "A left child holds a smaller key, so keep going left to reach the minimum.";
        }

        BSTNode& succ = **succLink;
        const NodeId succId = succ.id;
        const int succKey = succ.key;
        AlgorithmEvent& f = rec.emit(EventType::SuccessorFound, PseudoLine::DelFindSucc, succId);
        f.key = succKey;
        f.emphasis = Emphasis::Success;
        f.deletionCase = DeletionCase::TwoChildren;
        f.secondaryId = targetId;
        f.secondaryRole = SecondaryRole::Target;
        f.action = cat("The inorder successor is ", succKey);
        f.reason = cat(succKey, " has no left child, so it is the minimum of the right subtree: larger than every "
                                "key on the left of ",
                       key, " and smaller than every other key on its right.");

        // Unlink the successor first and remember its key, then write that key
        // into the target. Doing it in this order means every intermediate
        // state is a valid BST (copying first would briefly duplicate the key).
        const NodeId replacementId = succ.right ? succ.right->id : kNoNode;
        const int replacementKey = succ.right ? succ.right->key : 0;
        *succLink = std::move(succ.right); // the successor never has a left child
        --size_;
        ++rec.metrics.structuralChanges;
        rec.path.pop_back();
        rec.capture(*this);

        AlgorithmEvent& r = rec.emit(EventType::NodeRemoved, PseudoLine::DelRemoveSucc, succId);
        r.key = succKey;
        r.emphasis = Emphasis::Removing;
        r.deletionCase = DeletionCase::TwoChildren;
        r.action = cat("Unlink the successor's node and keep its key (", succKey, ")");
        if (replacementId != kNoNode) {
            r.secondaryId = replacementId;
            r.secondaryRole = SecondaryRole::OnlyChild;
            r.reason = cat("It has no left child, so its right child (", replacementKey,
                           ") takes its place: the one-child case.");
        } else {
            r.reason = "It is a leaf, so its parent's pointer is simply cleared: the leaf case.";
        }

        target.key = succKey;
        ++rec.metrics.structuralChanges;
        rec.capture(*this);
        AlgorithmEvent& k = rec.emit(EventType::KeyUpdated, PseudoLine::DelCopyKey, targetId);
        k.key = succKey;
        k.comparedKey = std::nullopt;
        k.emphasis = Emphasis::Success;
        k.deletionCase = DeletionCase::TwoChildren;
        k.action = cat("Write ", succKey, " into the node that held ", key);
        k.reason = cat("The node keeps its position and both children; only its key changes. ", succKey,
                       " is larger than everything in the left subtree and smaller than everything left in "
                       "the right subtree, so the order is preserved.");
    }
    return ResultCode::Ok;
}

// ---------------------------------------------------------------------------
// Traversal
// ---------------------------------------------------------------------------

OperationRecord BinarySearchTree::traverse(TraversalOrder order) const
{
    Recorder rec(traversalOperation(order));
    rec.capture(*this);
    const char* name = traversalName(order);

    if (order == TraversalOrder::LevelOrder) {
        if (!root_) {
            AlgorithmEvent& e = rec.emit(EventType::OperationCompleted, PseudoLine::BfsEmpty);
            e.emphasis = Emphasis::Neutral;
            e.action = "Level-order traversal of an empty tree";
            e.reason = "There is no root to enqueue, so the output sequence is empty.";
            return rec.finish(ResultStatus::Success, ResultCode::Ok, "Traversal completed.");
        }

        rec.queue = {root_->id};
        AlgorithmEvent& start = rec.emit(EventType::OperationStarted, PseudoLine::BfsInit, root_->id);
        start.key = root_->key;
        start.action = cat("Level-order traversal (BFS): enqueue the root (", root_->key, ")");
        start.reason = "BFS visits the tree level by level. A first-in, first-out queue holds the nodes that have "
                       "been discovered but not yet visited.";

        levelOrderWalk(*root_, rec);

        AlgorithmEvent& done = rec.emit(EventType::OperationCompleted, PseudoLine::BfsDone);
        done.emphasis = Emphasis::Success;
        done.action = "Traversal completed: the queue is empty";
        done.reason = cat("Visited ", rec.metrics.traversalVisits, " nodes level by level: ", levelsText(snapshot()),
                          ".");
        return rec.finish(ResultStatus::Success, ResultCode::Ok, "Traversal completed.");
    }

    if (!root_) {
        AlgorithmEvent& e = rec.emit(EventType::OperationCompleted, PseudoLine::TrvDone);
        e.emphasis = Emphasis::Neutral;
        e.action = cat(name, " traversal of an empty tree");
        e.reason = "There are no nodes to visit, so the output sequence is empty.";
        return rec.finish(ResultStatus::Success, ResultCode::Ok, "Traversal completed.");
    }

    AlgorithmEvent& start = rec.emit(EventType::OperationStarted, PseudoLine::TrvStart, root_->id);
    start.key = root_->key;
    start.action = cat(name, " traversal: start at the root (", root_->key, ")");
    switch (order) {
    case TraversalOrder::Inorder:
        start.reason = "Inorder = Left → Root → Right. For a BST it outputs the keys in ascending order.";
        break;
    case TraversalOrder::Preorder:
        start.reason = "Preorder = Root → Left → Right. Each node is output as soon as it is reached.";
        break;
    case TraversalOrder::Postorder:
        start.reason = "Postorder = Left → Right → Root. A node is output only after both subtrees are finished.";
        break;
    case TraversalOrder::LevelOrder: break; // handled above
    }

    walk(*root_, order, rec, PseudoLine::TrvCall);

    AlgorithmEvent& done = rec.emit(EventType::OperationCompleted, PseudoLine::TrvDone);
    done.emphasis = Emphasis::Success;
    done.action = "Traversal completed";
    done.reason = cat("Visited ", rec.metrics.traversalVisits, " nodes. Output: ", joinKeys(rec.sequence),
                      order == TraversalOrder::Inorder ? " (ascending, as expected for a BST)." : ".");
    return rec.finish(ResultStatus::Success, ResultCode::Ok, "Traversal completed.");
}

// ---------------------------------------------------------------------------
// Build / clear
// ---------------------------------------------------------------------------

OperationRecord BinarySearchTree::rebuild(const std::vector<int>& keys)
{
    Recorder rec(OperationType::Build);
    rec.capture(*this);

    std::unordered_set<int> seen;
    for (int key : keys) {
        if (!seen.insert(key).second) {
            AlgorithmEvent& e = rec.emit(EventType::OperationRejected, PseudoLine::BldValidate);
            e.key = key;
            e.emphasis = Emphasis::Error;
            e.action = cat("Duplicate key ", key);
            e.reason = "Keys in this BST must be unique. The current tree was not changed.";
            rec.result().key = key;
            return rec.finish(ResultStatus::Rejected, ResultCode::DuplicateKey,
                              cat("Duplicate key ", key, ": the tree was not changed."));
        }
    }
    if (keys.size() > capacity_) {
        AlgorithmEvent& e = rec.emit(EventType::OperationRejected, PseudoLine::BldValidate);
        e.emphasis = Emphasis::Error;
        e.action = cat("Too many keys (", keys.size(), ")");
        e.reason = cat("This demonstration supports at most ", capacity_, " nodes. The current tree was not changed.");
        return rec.finish(ResultStatus::Rejected, ResultCode::CapacityReached, "Too many keys: the tree was not changed.");
    }

    AlgorithmEvent& start = rec.emit(EventType::OperationStarted, PseudoLine::BldValidate);
    start.action = cat("Validated ", keys.size(), " key(s)");
    start.reason = "All entries are integers in range and unique, so the tree can be built.";

    BinarySearchTree fresh;
    fresh.capacity_ = capacity_;
    fresh.nextId_ = nextId_; // keep ids unique across rebuilds so animations never confuse nodes
    for (int key : keys) {
        if (fresh.insertQuiet(key) != ResultCode::Ok)
            throw std::logic_error("rebuild: insertion of a validated key failed");
    }
    fresh.verifyInvariant();
    *this = std::move(fresh);
    rec.result().treeChanged = true;
    rec.capture(*this);

    AlgorithmEvent& done = rec.emit(EventType::OperationCompleted, PseudoLine::BldInsert, root_ ? root_->id : kNoNode);
    done.emphasis = Emphasis::Success;
    if (keys.empty()) {
        done.action = "Created an empty tree";
        done.reason = "No keys were given.";
    } else {
        done.action = cat("Built a tree of ", size_, " node(s)");
        done.reason = cat("Inserted in the given order: ", joinKeys(keys), ". The first key (", keys.front(),
                          ") is the root; insertion order determines the shape.");
    }
    return rec.finish(ResultStatus::Success, ResultCode::Ok, "Tree created.");
}

OperationRecord BinarySearchTree::clearTree()
{
    Recorder rec(OperationType::Clear);
    rec.capture(*this);
    const std::size_t released = size_;

    AlgorithmEvent& start = rec.emit(EventType::OperationStarted, PseudoLine::ClrRelease, root_ ? root_->id : kNoNode);
    start.action = cat("Release ", released, " node(s)");
    start.reason = "Each node is owned by exactly one std::unique_ptr, so each is freed exactly once.";

    clear();
    rec.capture(*this);
    rec.result().treeChanged = released > 0;

    AlgorithmEvent& done = rec.emit(EventType::OperationCompleted, PseudoLine::ClrDone);
    done.emphasis = Emphasis::Success;
    done.action = "Tree cleared";
    done.reason = "The root pointer is null and the size is 0.";
    return rec.finish(ResultStatus::Success, ResultCode::Ok, "Tree cleared.");
}

} // namespace vdsa
