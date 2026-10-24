// VisualDSA web — pseudocode listings and complexity table (port of
// src/core/Pseudocode.cpp). Listings are keyed by stable PseudoLine ids, so
// the panel highlights lines by id and never by matching text.
(function (global) {
    'use strict';

    const VDSA = global.VDSA;
    const { OperationType: Op, PseudoLine: L } = VDSA;

    const line = (id, indent, text) => ({ id, indent, text });

    const listings = {
        [Op.Insert]: [
            line(L.None, 0, 'insert(key):'),
            line(L.InsInit, 1, 'node ← root;  parent ← null'),
            line(L.None, 1, 'while node ≠ null:'),
            line(L.InsCompare, 2, 'compare key with node.key'),
            line(L.InsDuplicate, 2, 'if key = node.key: reject duplicate'),
            line(L.InsGoLeft, 2, 'if key < node.key: go left'),
            line(L.InsGoRight, 2, 'else: go right'),
            line(L.InsAttach, 1, 'attach Node(key) under parent'),
            line(L.InsDone, 1, 'verify invariant; return success'),
        ],
        [Op.Search]: [
            line(L.None, 0, 'search(key):'),
            line(L.SrchInit, 1, 'node ← root'),
            line(L.None, 1, 'while node ≠ null:'),
            line(L.SrchCompare, 2, 'compare key with node.key'),
            line(L.SrchFound, 2, 'if key = node.key: return FOUND'),
            line(L.SrchGoLeft, 2, 'if key < node.key: node ← node.left'),
            line(L.SrchGoRight, 2, 'else: node ← node.right'),
            line(L.SrchNotFound, 1, 'return NOT FOUND'),
        ],
        [Op.Delete]: [
            line(L.None, 0, 'delete(key):'),
            line(L.DelSearch, 1, 'node ← search(key), tracking parent'),
            line(L.DelNotFound, 1, 'if node = null: report NOT FOUND'),
            line(L.DelCaseLeaf, 1, 'if node has no children:'),
            line(L.DelRemoveLeaf, 2, 'parent.link ← null'),
            line(L.DelCaseOne, 1, 'else if node has one child:'),
            line(L.DelSpliceChild, 2, 'parent.link ← that child'),
            line(L.DelCaseTwo, 1, 'else:  // two children'),
            line(L.DelFindSucc, 2, 'succ ← leftmost of node.right'),
            line(L.DelRemoveSucc, 2, 'unlink succ (← succ.right)'),
            line(L.DelCopyKey, 2, 'node.key ← succ.key'),
            line(L.DelVerify, 1, 'verify BST invariant'),
        ],
        [Op.Update]: [
            line(L.None, 0, 'update(old, new):'),
            line(L.UpdFindOld, 1, 'node ← search(old)'),
            line(L.UpdOldMissing, 1, 'if node = null: reject'),
            line(L.UpdSame, 1, 'if new = old: no change'),
            line(L.UpdCheckNew, 1, 'if search(new) found: reject'),
            line(L.UpdBegin, 1, 'copy ← tree  // transaction'),
            line(L.UpdDelete, 1, 'delete(old) on copy'),
            line(L.UpdInsert, 1, 'insert(new) on copy'),
            line(L.UpdCommit, 1, 'if copy is valid: tree ← copy'),
            line(L.UpdRollback, 1, 'else: discard copy (rollback)'),
        ],
        [Op.InorderTraversal]: [
            line(L.TrvStart, 0, 'output ← [ ];  inorder(root)'),
            line(L.TrvDone, 0, 'return output'),
            line(L.TrvCall, 0, 'inorder(node):'),
            line(L.TrvNull, 1, 'if node = null: return'),
            line(L.TrvLeft, 1, 'inorder(node.left)'),
            line(L.TrvVisit, 1, 'output.append(node.key)'),
            line(L.TrvRight, 1, 'inorder(node.right)'),
        ],
        [Op.PreorderTraversal]: [
            line(L.TrvStart, 0, 'output ← [ ];  preorder(root)'),
            line(L.TrvDone, 0, 'return output'),
            line(L.TrvCall, 0, 'preorder(node):'),
            line(L.TrvNull, 1, 'if node = null: return'),
            line(L.TrvVisit, 1, 'output.append(node.key)'),
            line(L.TrvLeft, 1, 'preorder(node.left)'),
            line(L.TrvRight, 1, 'preorder(node.right)'),
        ],
        [Op.PostorderTraversal]: [
            line(L.TrvStart, 0, 'output ← [ ];  postorder(root)'),
            line(L.TrvDone, 0, 'return output'),
            line(L.TrvCall, 0, 'postorder(node):'),
            line(L.TrvNull, 1, 'if node = null: return'),
            line(L.TrvLeft, 1, 'postorder(node.left)'),
            line(L.TrvRight, 1, 'postorder(node.right)'),
            line(L.TrvVisit, 1, 'output.append(node.key)'),
        ],
        [Op.Build]: [
            line(L.None, 0, 'build(keys):'),
            line(L.BldValidate, 1, 'validate: integers, in range, unique'),
            line(L.BldInsert, 1, 'for each key in order: insert(key)'),
            line(L.BldDone, 1, 'verify invariant; replace tree'),
        ],
        [Op.Clear]: [
            line(L.None, 0, 'clear():'),
            line(L.ClrRelease, 1, 'release every node exactly once'),
            line(L.ClrDone, 1, 'root ← null;  size ← 0'),
        ],
    };

    const complexity = (best, average, worst, space, note) => ({ best, average, worst, space, note });

    const searchPath = complexity('O(1)', 'O(log n)', 'O(n)', 'O(1) auxiliary (iterative)',
        'Follows one root-to-leaf path, so the cost is O(h). A balanced-shaped tree has h ≈ log₂ n; a skewed tree '
        + '(e.g. sorted insertions) has h = n − 1.');
    const deletion = complexity('O(1)', 'O(log n)', 'O(n)', 'O(1) auxiliary (iterative)',
        'Search O(h) + successor search O(h) + O(1) re-linking, so O(h) overall. Not guaranteed O(log n): this tree '
        + 'does not rebalance.');
    const update = complexity('O(1)', 'O(log n)', 'O(n)', 'O(n) here (working copy); O(1) in-place',
        'Two searches, one deletion and one insertion: O(h). This implementation copies the tree to make the update '
        + 'atomic, which costs O(n) extra time and space.');
    const traversal = complexity('O(n)', 'O(n)', 'O(n)', 'O(h) recursion stack + O(n) output',
        'Every node is visited exactly once regardless of shape. Recursion depth equals the height h: O(log n) when '
        + 'balanced-shaped, O(n) when skewed.');
    const build = complexity('O(n log n)', 'O(n log n)', 'O(n²)', 'O(n) for the nodes',
        'n insertions of O(h) each. Inserting already-sorted keys produces a skewed tree, which is the O(n²) worst case.');
    const clear = complexity('O(1)', 'O(1)', 'O(1)', 'O(1); the garbage collector reclaims the nodes',
        'Detaching the root releases the whole tree at once. (The C++ version frees each node explicitly in O(n).)');

    function pseudocodeFor(op) { return listings[op] || listings[Op.Insert]; }

    function pseudocodeRow(op, lineId) {
        if (lineId === L.None)
            return -1;
        return pseudocodeFor(op).findIndex((l) => l.id === lineId);
    }

    function complexityFor(op) {
        switch (op) {
        case Op.Insert:
        case Op.Search: return searchPath;
        case Op.Delete: return deletion;
        case Op.Update: return update;
        case Op.InorderTraversal:
        case Op.PreorderTraversal:
        case Op.PostorderTraversal: return traversal;
        case Op.Build: return build;
        case Op.Clear: return clear;
        }
        return searchPath;
    }

    Object.assign(VDSA, { pseudocodeFor, pseudocodeRow, complexityFor });
})(typeof globalThis !== 'undefined' ? globalThis : this);
