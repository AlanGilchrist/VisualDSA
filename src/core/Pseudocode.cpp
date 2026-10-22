#include "Pseudocode.h"

namespace vdsa {

namespace {

using L = PseudoLine;

const std::vector<PseudoLineSpec> kInsert = {
    {L::None, 0, "insert(key):"},
    {L::InsInit, 1, "node ← root;  parent ← null"},
    {L::None, 1, "while node ≠ null:"},
    {L::InsCompare, 2, "compare key with node.key"},
    {L::InsDuplicate, 2, "if key = node.key: reject duplicate"},
    {L::InsGoLeft, 2, "if key < node.key: go left"},
    {L::InsGoRight, 2, "else: go right"},
    {L::InsAttach, 1, "attach Node(key) under parent"},
    {L::InsDone, 1, "verify invariant; return success"},
};

const std::vector<PseudoLineSpec> kSearch = {
    {L::None, 0, "search(key):"},
    {L::SrchInit, 1, "node ← root"},
    {L::None, 1, "while node ≠ null:"},
    {L::SrchCompare, 2, "compare key with node.key"},
    {L::SrchFound, 2, "if key = node.key: return FOUND"},
    {L::SrchGoLeft, 2, "if key < node.key: node ← node.left"},
    {L::SrchGoRight, 2, "else: node ← node.right"},
    {L::SrchNotFound, 1, "return NOT FOUND"},
};

const std::vector<PseudoLineSpec> kDelete = {
    {L::None, 0, "delete(key):"},
    {L::DelSearch, 1, "node ← search(key), tracking parent"},
    {L::DelNotFound, 1, "if node = null: report NOT FOUND"},
    {L::DelCaseLeaf, 1, "if node has no children:"},
    {L::DelRemoveLeaf, 2, "parent.link ← null"},
    {L::DelCaseOne, 1, "else if node has one child:"},
    {L::DelSpliceChild, 2, "parent.link ← that child"},
    {L::DelCaseTwo, 1, "else:  // two children"},
    {L::DelFindSucc, 2, "succ ← leftmost of node.right"},
    {L::DelRemoveSucc, 2, "unlink succ (← succ.right)"},
    {L::DelCopyKey, 2, "node.key ← succ.key"},
    {L::DelVerify, 1, "verify BST invariant"},
};

const std::vector<PseudoLineSpec> kUpdate = {
    {L::None, 0, "update(old, new):"},
    {L::UpdFindOld, 1, "node ← search(old)"},
    {L::UpdOldMissing, 1, "if node = null: reject"},
    {L::UpdSame, 1, "if new = old: no change"},
    {L::UpdCheckNew, 1, "if search(new) found: reject"},
    {L::UpdBegin, 1, "copy ← tree  // transaction"},
    {L::UpdDelete, 1, "delete(old) on copy"},
    {L::UpdInsert, 1, "insert(new) on copy"},
    {L::UpdCommit, 1, "if copy is valid: tree ← copy"},
    {L::UpdRollback, 1, "else: discard copy (rollback)"},
};

const std::vector<PseudoLineSpec> kInorder = {
    {L::TrvStart, 0, "output ← [ ];  inorder(root)"},
    {L::TrvDone, 0, "return output"},
    {L::TrvCall, 0, "inorder(node):"},
    {L::TrvNull, 1, "if node = null: return"},
    {L::TrvLeft, 1, "inorder(node.left)"},
    {L::TrvVisit, 1, "output.append(node.key)"},
    {L::TrvRight, 1, "inorder(node.right)"},
};

const std::vector<PseudoLineSpec> kPreorder = {
    {L::TrvStart, 0, "output ← [ ];  preorder(root)"},
    {L::TrvDone, 0, "return output"},
    {L::TrvCall, 0, "preorder(node):"},
    {L::TrvNull, 1, "if node = null: return"},
    {L::TrvVisit, 1, "output.append(node.key)"},
    {L::TrvLeft, 1, "preorder(node.left)"},
    {L::TrvRight, 1, "preorder(node.right)"},
};

const std::vector<PseudoLineSpec> kPostorder = {
    {L::TrvStart, 0, "output ← [ ];  postorder(root)"},
    {L::TrvDone, 0, "return output"},
    {L::TrvCall, 0, "postorder(node):"},
    {L::TrvNull, 1, "if node = null: return"},
    {L::TrvLeft, 1, "postorder(node.left)"},
    {L::TrvRight, 1, "postorder(node.right)"},
    {L::TrvVisit, 1, "output.append(node.key)"},
};

const std::vector<PseudoLineSpec> kBuild = {
    {L::None, 0, "build(keys):"},
    {L::BldValidate, 1, "validate: integers, in range, unique"},
    {L::BldInsert, 1, "for each key in order: insert(key)"},
    {L::BldDone, 1, "verify invariant; replace tree"},
};

const std::vector<PseudoLineSpec> kClear = {
    {L::None, 0, "clear():"},
    {L::ClrRelease, 1, "release every node exactly once"},
    {L::ClrDone, 1, "root ← null;  size ← 0"},
};

const ComplexityInfo kSearchPathComplexity = {
    "O(1)", "O(log n)", "O(n)", "O(1) auxiliary (iterative)",
    "Follows one root-to-leaf path, so the cost is O(h). A balanced-shaped tree has "
    "h ≈ log₂ n; a skewed tree (e.g. sorted insertions) has h = n − 1."};

const ComplexityInfo kDeleteComplexity = {
    "O(1)", "O(log n)", "O(n)", "O(1) auxiliary (iterative)",
    "Search O(h) + successor search O(h) + O(1) re-linking, so O(h) overall. "
    "Not guaranteed O(log n): this tree does not rebalance."};

const ComplexityInfo kUpdateComplexity = {
    "O(1)", "O(log n)", "O(n)", "O(n) here (working copy); O(1) in-place",
    "Two searches, one deletion and one insertion: O(h). This implementation copies "
    "the tree to make the update atomic, which costs O(n) extra time and space."};

const ComplexityInfo kTraversalComplexity = {
    "O(n)", "O(n)", "O(n)", "O(h) recursion stack + O(n) output",
    "Every node is visited exactly once regardless of shape. Recursion depth equals "
    "the height h: O(log n) when balanced-shaped, O(n) when skewed."};

const ComplexityInfo kBuildComplexity = {
    "O(n log n)", "O(n log n)", "O(n²)", "O(n) for the nodes",
    "n insertions of O(h) each. Inserting already-sorted keys produces a skewed tree, "
    "which is the O(n²) worst case."};

const ComplexityInfo kClearComplexity = {
    "O(n)", "O(n)", "O(n)", "O(h) explicit stack",
    "Each node is released exactly once. An explicit stack avoids deep recursion on "
    "skewed trees."};

} // namespace

const std::vector<PseudoLineSpec>& pseudocodeFor(OperationType op)
{
    switch (op) {
    case OperationType::Insert: return kInsert;
    case OperationType::Delete: return kDelete;
    case OperationType::Search: return kSearch;
    case OperationType::Update: return kUpdate;
    case OperationType::InorderTraversal: return kInorder;
    case OperationType::PreorderTraversal: return kPreorder;
    case OperationType::PostorderTraversal: return kPostorder;
    case OperationType::Build: return kBuild;
    case OperationType::Clear: return kClear;
    }
    return kInsert;
}

int pseudocodeRow(OperationType op, PseudoLine line)
{
    if (line == PseudoLine::None)
        return -1;
    const auto& listing = pseudocodeFor(op);
    for (std::size_t i = 0; i < listing.size(); ++i) {
        if (listing[i].id == line)
            return static_cast<int>(i);
    }
    return -1;
}

const ComplexityInfo& complexityFor(OperationType op)
{
    switch (op) {
    case OperationType::Insert:
    case OperationType::Search: return kSearchPathComplexity;
    case OperationType::Delete: return kDeleteComplexity;
    case OperationType::Update: return kUpdateComplexity;
    case OperationType::InorderTraversal:
    case OperationType::PreorderTraversal:
    case OperationType::PostorderTraversal: return kTraversalComplexity;
    case OperationType::Build: return kBuildComplexity;
    case OperationType::Clear: return kClearComplexity;
    }
    return kSearchPathComplexity;
}

} // namespace vdsa
