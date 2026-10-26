// Tests for the web port of the BST engine. They mirror the Qt Test suites in
// tests/ so the browser version is held to the same behaviour as the C++ core.
//
// Run with:  node --test web/tests
'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');

require('../js/core.js');
require('../js/pseudocode.js');
require('../js/validation.js');

const {
    BinarySearchTree, TraversalOrder, EventType, DeletionCase, SecondaryRole, PseudoLine, ResultStatus, ResultCode,
    OperationType, validateTree, validateSnapshot, pseudocodeFor, pseudocodeRow, parseKey, parseKeyList, InputError,
} = globalThis.VDSA;

function treeOf(keys) {
    const tree = new BinarySearchTree();
    const rec = tree.rebuild(keys);
    assert.equal(rec.result.status, ResultStatus.Success);
    return tree;
}
const sampleTree = () => treeOf(BinarySearchTree.sampleKeys());
const preorderOf = (tree) => tree.snapshot().preorderKeys();
const firstEvent = (rec, type) => rec.events.find((e) => e.type === type);
const hasEvent = (rec, type) => rec.events.some((e) => e.type === type);

// --- Insert ----------------------------------------------------------------

test('insert into an empty tree makes the root', () => {
    const tree = new BinarySearchTree();
    const rec = tree.insert(42);
    assert.equal(rec.result.status, ResultStatus.Success);
    assert.equal(tree.root.key, 42);
    assert.equal(tree.size, 1);
    assert.equal(tree.height(), 0);
});

test('sample tree has the expected shape', () => {
    const tree = sampleTree();
    assert.deepEqual(preorderOf(tree), [50, 30, 20, 40, 70, 60, 80]);
    assert.equal(tree.root.left.key, 30);
    assert.equal(tree.root.right.right.key, 80);
    assert.equal(tree.height(), 2);
});

test('duplicate insert is rejected and leaves the tree unchanged', () => {
    const tree = sampleTree();
    const before = preorderOf(tree);
    const rec = tree.insert(40);
    assert.equal(rec.result.status, ResultStatus.Rejected);
    assert.equal(rec.result.code, ResultCode.DuplicateKey);
    assert.equal(rec.result.treeChanged, false);
    assert.deepEqual(preorderOf(tree), before);
    assert.equal(rec.events.at(-1).type, EventType.OperationRejected);
});

test('insert events describe the path', () => {
    const tree = sampleTree();
    const rec = tree.insert(45); // 50 -> left 30 -> right 40 -> right (empty)
    const compared = rec.events.filter((e) => e.type === EventType.KeyCompared).map((e) => e.comparedKey);
    assert.deepEqual(compared, [50, 30, 40]);
    const moves = rec.events.filter((e) => e.type === EventType.MoveLeft || e.type === EventType.MoveRight)
        .map((e) => e.type);
    assert.deepEqual(moves, [EventType.MoveLeft, EventType.MoveRight, EventType.MoveRight]);
    const lastMove = rec.events.filter((e) => e.type === EventType.MoveRight).at(-1);
    assert.equal(lastMove.pendingParent, tree.root.left.right.id);
    const inserted = firstEvent(rec, EventType.NodeInserted);
    assert.equal(inserted.action, 'Attach 45 as the right child of 40');
    assert.equal(inserted.secondaryRole, SecondaryRole.Parent);
    assert.equal(rec.result.metrics.keyComparisons, 3);
    assert.equal(rec.result.metrics.structuralChanges, 1);
});

test('insert respects capacity', () => {
    const tree = treeOf([1, 2, 3]);
    tree.capacity = 3;
    const rec = tree.insert(4);
    assert.equal(rec.result.code, ResultCode.CapacityReached);
    assert.equal(tree.size, 3);
});

test('many inserts keep the invariant', () => {
    const tree = new BinarySearchTree();
    for (const key of [50, 25, 75, 12, 37, 62, 87, 6, 18, 31, 43, 56, 68, 81, 93, 1, 99, -5])
        assert.equal(tree.insert(key).result.status, ResultStatus.Success);
    assert.ok(tree.validate().valid);
    assert.equal(tree.size, 18);
});

// --- Search ----------------------------------------------------------------

test('search finds root, leaf and reports missing keys', () => {
    const tree = sampleTree();
    assert.equal(tree.search(50).result.code, ResultCode.KeyFound);

    const leaf = tree.search(60);
    assert.equal(leaf.result.status, ResultStatus.Success);
    assert.equal(leaf.result.metrics.nodesVisited, 3);
    assert.match(firstEvent(leaf, EventType.NodeFound).reason, /depth 2/);

    const missing = tree.search(65);
    assert.equal(missing.result.status, ResultStatus.NotFound);
    const last = missing.events.at(-1);
    assert.equal(last.type, EventType.NodeNotFound);
    assert.equal(last.pendingParent, tree.root.right.left.id);
    assert.equal(last.pendingSide, 'right');
});

test('search on an empty tree', () => {
    const rec = new BinarySearchTree().search(1);
    assert.equal(rec.result.status, ResultStatus.NotFound);
    assert.equal(rec.events.length, 1);
});

// --- Traversals --------------------------------------------------------------

test('traversals of the sample tree', () => {
    const tree = sampleTree();
    assert.deepEqual(tree.inorder(), [20, 30, 40, 50, 60, 70, 80]);
    assert.deepEqual(tree.preorder(), [50, 30, 20, 40, 70, 60, 80]);
    assert.deepEqual(tree.postorder(), [20, 40, 30, 60, 80, 70, 50]);
});

test('traversals on empty and single-node trees', () => {
    assert.deepEqual(new BinarySearchTree().inorder(), []);
    assert.deepEqual(new BinarySearchTree().levelOrder(), []);
    assert.deepEqual(treeOf([7]).postorder(), [7]);
    assert.deepEqual(treeOf([7]).levelOrder(), [7]);
});

// --- Breadth-first (level-order) traversal ---------------------------------

test('level order of the sample tree', () => {
    assert.deepEqual(sampleTree().levelOrder(), [50, 30, 70, 20, 40, 60, 80]);
});

test('level order of a skewed tree keeps at most one node waiting', () => {
    const rec = treeOf([1, 2, 3, 4, 5]).traverse(TraversalOrder.LevelOrder);
    assert.deepEqual(rec.result.sequence, [1, 2, 3, 4, 5]);
    for (const e of rec.events)
        assert.ok(e.queue.length <= 1);
});

test('level order of an empty tree', () => {
    const rec = new BinarySearchTree().traverse(TraversalOrder.LevelOrder);
    assert.equal(rec.events.length, 1);
    assert.equal(rec.events[0].line, PseudoLine.BfsEmpty);
});

test('level-order queue is first-in, first-out', () => {
    const rec = sampleTree().traverse(TraversalOrder.LevelOrder);
    const snap = rec.snapshots[0];
    assert.deepEqual(rec.events[0].queue, [snap.root]);
    let dequeued = 0;
    let enqueued = 0;
    let widest = 0;
    let lastDepth = 0;
    for (let i = 1; i < rec.events.length; ++i) {
        const prev = rec.events[i - 1];
        const e = rec.events[i];
        widest = Math.max(widest, e.queue.length);
        if (e.type === EventType.NodeDequeued) {
            ++dequeued;
            assert.equal(e.nodeId, prev.queue[0]); // the front leaves
            assert.deepEqual(e.queue, prev.queue.slice(1));
            const depth = snap.find(e.nodeId).depth;
            assert.ok(depth >= lastDepth); // levels never go backwards
            lastDepth = depth;
        } else if (e.type === EventType.NodeEnqueued) {
            ++enqueued;
            assert.equal(e.queue.at(-1), e.secondaryId); // new nodes join the back
            assert.equal(e.secondaryRole, SecondaryRole.Enqueued);
            assert.equal(snap.find(e.secondaryId).parent, e.nodeId);
        }
    }
    assert.equal(dequeued, 7);
    assert.equal(enqueued, 6);
    assert.equal(widest, 4);
    assert.deepEqual(rec.events.at(-1).queue, []);
    assert.equal(rec.result.metrics.keyComparisons, 0);
    assert.equal(rec.events.at(-1).reason, 'Visited 7 nodes level by level: [50] [30, 70] [20, 40, 60, 80].');
});

test('traversal events emit the output progressively', () => {
    const rec = sampleTree().traverse(TraversalOrder.Inorder);
    const visits = rec.events.filter((e) => e.type === EventType.TraversalNodeVisited);
    assert.equal(visits.length, 7);
    visits.forEach((e, i) => {
        assert.equal(e.sequence.length, i + 1);
        assert.equal(e.completed.length, i + 1);
    });
    assert.equal(rec.events.at(-1).reason, 'Visited 7 nodes. Output: 20, 30, 40, 50, 60, 70, 80 (ascending, as expected for a BST).');
});

test('every event line exists in its operation listing', () => {
    const tree = sampleTree();
    const records = [
        tree.search(60), tree.search(65), tree.insert(45), tree.insert(45), tree.remove(20), tree.remove(70),
        tree.remove(30), tree.remove(1), tree.traverse(TraversalOrder.Inorder), tree.traverse(TraversalOrder.Preorder),
        tree.traverse(TraversalOrder.Postorder), tree.traverse(TraversalOrder.LevelOrder),
        new BinarySearchTree().traverse(TraversalOrder.LevelOrder), tree.rebuild([3, 1, 2]), tree.rebuild([1, 1]),
        tree.clearTree(),
    ];
    for (const rec of records) {
        for (const e of rec.events) {
            assert.ok(pseudocodeRow(rec.result.operation, e.line) >= 0,
                `${e.line} missing from ${rec.result.operation} listing`);
            assert.ok(e.snapshotIndex >= 0 && e.snapshotIndex < rec.snapshots.length);
        }
    }
    assert.ok(pseudocodeFor(OperationType.Delete).length > 0);
});

// --- Delete ----------------------------------------------------------------

test('delete a leaf', () => {
    const tree = sampleTree();
    const rec = tree.remove(20);
    assert.equal(rec.result.deletionCase, DeletionCase.Leaf);
    assert.equal(tree.root.left.left, null);
    assert.ok(tree.validate().valid);
});

test('delete a node with one child splices the child up', () => {
    const tree = treeOf([50, 30, 70, 20]);
    const rec = tree.remove(30);
    assert.equal(rec.result.deletionCase, DeletionCase.OneChild);
    assert.equal(tree.root.left.key, 20);
    assert.equal(firstEvent(rec, EventType.NodeRemoved).action, 'Linked 50 directly to 20');
});

test('delete a node with two children uses the inorder successor', () => {
    const tree = sampleTree();
    const rec = tree.remove(30);
    assert.equal(rec.result.deletionCase, DeletionCase.TwoChildren);
    assert.equal(tree.root.left.key, 40);
    assert.deepEqual(preorderOf(tree), [50, 40, 20, 70, 60, 80]);
    assert.equal(firstEvent(rec, EventType.SuccessorFound).key, 40);
    assert.ok(hasEvent(rec, EventType.KeyUpdated));
    assert.equal(rec.result.metrics.structuralChanges, 2);
    assert.equal(rec.result.metrics.keyComparisons, 2);
    assert.equal(rec.result.metrics.nodesVisited, 3);
});

test('two-children delete walks left to the successor', () => {
    const tree = treeOf([50, 30, 70, 60, 80, 55, 57]);
    const rec = tree.remove(50);
    assert.equal(tree.root.key, 55);
    assert.equal(tree.root.right.left.left.key, 57);
    const path = rec.events.filter((e) => e.type === EventType.SuccessorSearchStarted || e.type === EventType.NodeVisited)
        .map((e) => e.key);
    assert.deepEqual(path, [70, 60, 55]);
    const removed = firstEvent(rec, EventType.NodeRemoved);
    assert.equal(removed.key, 55);
    assert.equal(removed.secondaryRole, SecondaryRole.OnlyChild);
});

test('two-children delete never records an invalid tree', () => {
    const rec = sampleTree().remove(50);
    assert.equal(rec.snapshots.length, 3);
    for (const s of rec.snapshots)
        assert.ok(validateSnapshot(s).valid, validateSnapshot(s).error);
    const removedAt = rec.events.findIndex((e) => e.type === EventType.NodeRemoved);
    const copiedAt = rec.events.findIndex((e) => e.type === EventType.KeyUpdated);
    assert.ok(removedAt < copiedAt);
});

test('delete the only node empties the tree', () => {
    const tree = treeOf([42]);
    const rec = tree.remove(42);
    assert.equal(rec.result.deletionCase, DeletionCase.Leaf);
    assert.ok(tree.empty());
    assert.equal(tree.height(), -1);
    assert.ok(rec.snapshots.at(-1).empty());
});

test('delete a missing key leaves the tree unchanged', () => {
    const tree = sampleTree();
    const before = preorderOf(tree);
    const rec = tree.remove(65);
    assert.equal(rec.result.status, ResultStatus.NotFound);
    assert.equal(rec.result.treeChanged, false);
    assert.deepEqual(preorderOf(tree), before);
    assert.equal(rec.snapshots.length, 1);
});

test('delete every key in several orders', () => {
    const orders = [[50, 30, 70, 20, 40, 60, 80], [20, 40, 60, 80, 30, 70, 50], [80, 70, 60, 50, 40, 30, 20],
        [40, 50, 20, 70, 30, 80, 60]];
    for (const order of orders) {
        const tree = sampleTree();
        let expected = 7;
        for (const key of order) {
            assert.equal(tree.remove(key).result.status, ResultStatus.Success);
            assert.equal(tree.size, --expected);
            assert.ok(tree.validate().valid, tree.validate().error);
            assert.ok(!tree.contains(key));
        }
        assert.ok(tree.empty());
    }
});

// --- Snapshots, build, clear, invariants -----------------------------------

test('snapshots are independent of the live tree', () => {
    const tree = sampleTree();
    const snap = tree.snapshot();
    tree.remove(50);
    tree.insert(1);
    assert.deepEqual(snap.preorderKeys(), [50, 30, 20, 40, 70, 60, 80]);
});

test('metrics are cumulative and monotonic', () => {
    const rec = sampleTree().remove(50);
    for (let i = 1; i < rec.events.length; ++i) {
        for (const k of Object.keys(rec.events[i].metrics))
            assert.ok(rec.events[i].metrics[k] >= rec.events[i - 1].metrics[k]);
    }
});

test('rebuild rejects duplicates without changing the tree and keeps ids unique', () => {
    const tree = sampleTree();
    const before = preorderOf(tree);
    assert.equal(tree.rebuild([1, 2, 1]).result.code, ResultCode.DuplicateKey);
    assert.deepEqual(preorderOf(tree), before);

    const oldIds = new Set(tree.snapshot().nodes.map((n) => n.id));
    tree.rebuild([5, 3, 8]);
    for (const n of tree.snapshot().nodes)
        assert.ok(!oldIds.has(n.id));
});

test('clear empties everything', () => {
    const tree = sampleTree();
    const rec = tree.clearTree();
    assert.ok(tree.empty());
    assert.equal(tree.root, null);
    assert.equal(rec.result.treeChanged, true);
});

test('validator checks the whole subtree, not just the parent', () => {
    const tree = treeOf([50, 30]);
    tree.root.left.right = new globalThis.VDSA.BSTNode(60, 99); // 60 < 50 rule broken two levels down
    assert.equal(validateTree(tree.root).valid, false);
});

test('clone is deep and preserves ids', () => {
    const tree = sampleTree();
    const copy = tree.clone();
    assert.deepEqual(copy.snapshot().nodes, tree.snapshot().nodes);
    copy.remove(50);
    assert.equal(tree.root.key, 50);
});

test('randomized operations match a reference set', () => {
    let seed = 12345;
    const rand = () => (seed = (seed * 1103515245 + 12345) % 2147483648) / 2147483648;
    const tree = new BinarySearchTree();
    const reference = new Set();
    for (let i = 0; i < 2000; ++i) {
        const key = Math.floor(rand() * 200) - 100;
        const choice = rand();
        if (choice < 0.45) {
            const ok = tree.insert(key).result.status === ResultStatus.Success;
            assert.equal(ok, !reference.has(key));
            reference.add(key);
        } else if (choice < 0.8) {
            const ok = tree.remove(key).result.status === ResultStatus.Success;
            assert.equal(ok, reference.has(key));
            reference.delete(key);
        } else {
            const found = tree.search(key).result.code === ResultCode.KeyFound;
            assert.equal(found, reference.has(key));
        }
        assert.ok(tree.validate().valid);
    }
    assert.deepEqual(tree.inorder(), [...reference].sort((a, b) => a - b));
});

// --- Input validation ------------------------------------------------------

test('parseKey accepts valid keys', () => {
    assert.equal(parseKey(' -42 ').value, -42);
    assert.equal(parseKey('+7').value, 7);
    assert.equal(parseKey('9999').value, 9999);
});

test('parseKey rejects bad input with the right error', () => {
    const cases = [
        ['', InputError.Empty], ['  ', InputError.Empty], ['abc', InputError.InvalidInteger], ['4.5', InputError.InvalidInteger],
        ['-', InputError.InvalidInteger], ['1e3', InputError.InvalidInteger], ['99999999999999999999', InputError.Overflow],
        ['2147483648', InputError.Overflow], ['10000', InputError.OutOfRange], ['-10000', InputError.OutOfRange],
    ];
    for (const [text, error] of cases)
        assert.equal(parseKey(text).error, error, `input "${text}"`);
});

test('parseKeyList', () => {
    assert.deepEqual(parseKeyList('50, 30, 70,').keys, [50, 30, 70]);
    assert.deepEqual(parseKeyList('1 2, 3').keys, [1, 2, 3]);
    assert.equal(parseKeyList('1,,2').error, InputError.EmptyEntry);
    assert.equal(parseKeyList('1, 2, 1').error, InputError.DuplicateKeys);
    assert.equal(parseKeyList('').error, InputError.Empty);
    assert.equal(parseKeyList(Array.from({ length: 65 }, (_, i) => i).join(',')).error, InputError.TooManyKeys);
});
