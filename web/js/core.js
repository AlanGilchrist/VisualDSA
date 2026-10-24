// VisualDSA web — Layer A: the BST engine.
//
// A direct port of src/core (BinarySearchTree, Recorder, TreeSnapshot,
// TreeValidator). Every public operation runs the real algorithm on the real
// tree and returns an OperationRecord describing each step it took. The UI
// only replays those records; it never mutates the tree itself.
//
// Plain script (no modules) so index.html also works when opened from disk;
// everything is published on globalThis.VDSA, which Node tests can load too.
(function (global) {
    'use strict';

    const NO_NODE = -1;

    const OperationType = Object.freeze({
        Insert: 'Insert',
        Delete: 'Delete',
        Search: 'Search',
        Update: 'Update',
        InorderTraversal: 'InorderTraversal',
        PreorderTraversal: 'PreorderTraversal',
        PostorderTraversal: 'PostorderTraversal',
        Build: 'Build',
        Clear: 'Clear',
    });

    const TraversalOrder = Object.freeze({ Inorder: 'Inorder', Preorder: 'Preorder', Postorder: 'Postorder' });

    const EventType = Object.freeze({
        OperationStarted: 'OperationStarted',
        NodeVisited: 'NodeVisited',
        KeyCompared: 'KeyCompared',
        MoveLeft: 'MoveLeft',
        MoveRight: 'MoveRight',
        NodeInserted: 'NodeInserted',
        NodeFound: 'NodeFound',
        NodeNotFound: 'NodeNotFound',
        DeletionCaseIdentified: 'DeletionCaseIdentified',
        SuccessorSearchStarted: 'SuccessorSearchStarted',
        SuccessorFound: 'SuccessorFound',
        NodeRemoved: 'NodeRemoved',
        KeyUpdated: 'KeyUpdated',
        TraversalNodeVisited: 'TraversalNodeVisited',
        PhaseStarted: 'PhaseStarted',
        TransactionStarted: 'TransactionStarted',
        TransactionCommitted: 'TransactionCommitted',
        TransactionRolledBack: 'TransactionRolledBack',
        OperationCompleted: 'OperationCompleted',
        OperationRejected: 'OperationRejected',
        OperationFailed: 'OperationFailed',
    });

    const Direction = Object.freeze({ None: 'none', Left: 'left', Right: 'right' });

    // Semantic emphasis of the primary node in an event. The canvas maps these
    // to colours; the core never deals with colours itself.
    const Emphasis = Object.freeze({
        Neutral: 'Neutral',
        Active: 'Active',
        Success: 'Success',
        Error: 'Error',
        Removing: 'Removing',
    });

    const DeletionCase = Object.freeze({ None: 'None', Leaf: 'Leaf', OneChild: 'OneChild', TwoChildren: 'TwoChildren' });

    // Why a second node is highlighted alongside the primary one.
    const SecondaryRole = Object.freeze({
        None: 'None',
        NextNode: 'NextNode',
        OnlyChild: 'OnlyChild',
        Successor: 'Successor',
        Parent: 'Parent',
        Target: 'Target',
    });

    // Stable identifiers for pseudocode lines. Events refer to these ids and the
    // pseudocode panel highlights the line with the matching id.
    const PseudoLine = Object.freeze(Object.fromEntries([
        'None',
        'InsInit', 'InsCompare', 'InsDuplicate', 'InsGoLeft', 'InsGoRight', 'InsAttach', 'InsDone',
        'SrchInit', 'SrchCompare', 'SrchFound', 'SrchGoLeft', 'SrchGoRight', 'SrchNotFound',
        'DelSearch', 'DelNotFound', 'DelCaseLeaf', 'DelRemoveLeaf', 'DelCaseOne', 'DelSpliceChild',
        'DelCaseTwo', 'DelFindSucc', 'DelCopyKey', 'DelRemoveSucc', 'DelVerify',
        'UpdFindOld', 'UpdOldMissing', 'UpdSame', 'UpdCheckNew', 'UpdBegin', 'UpdDelete', 'UpdInsert',
        'UpdCommit', 'UpdRollback',
        'TrvStart', 'TrvDone', 'TrvCall', 'TrvNull', 'TrvLeft', 'TrvVisit', 'TrvRight',
        'BldValidate', 'BldInsert', 'BldDone', 'ClrRelease', 'ClrDone',
    ].map((name) => [name, name])));

    const ResultStatus = Object.freeze({
        Success: 'Success',   // operation did what was asked
        NotFound: 'NotFound', // key absent; tree unchanged
        NoChange: 'NoChange', // request was valid but nothing needed to change
        Rejected: 'Rejected', // request violated a rule; tree unchanged
        Failed: 'Failed',     // an internal check failed and the change was rolled back
    });

    const ResultCode = Object.freeze({
        Ok: 'Ok',
        KeyFound: 'KeyFound',
        KeyNotFound: 'KeyNotFound',
        DuplicateKey: 'DuplicateKey',
        KeyMissing: 'KeyMissing',
        SameKey: 'SameKey',
        NewKeyExists: 'NewKeyExists',
        CapacityReached: 'CapacityReached',
        InvariantViolation: 'InvariantViolation',
    });

    function operationName(op) {
        switch (op) {
        case OperationType.Insert: return 'Insert';
        case OperationType.Delete: return 'Delete';
        case OperationType.Search: return 'Search';
        case OperationType.Update: return 'Update';
        case OperationType.InorderTraversal: return 'Inorder traversal';
        case OperationType.PreorderTraversal: return 'Preorder traversal';
        case OperationType.PostorderTraversal: return 'Postorder traversal';
        case OperationType.Build: return 'Build tree';
        case OperationType.Clear: return 'Clear tree';
        }
        return 'Operation';
    }

    function isTraversal(op) {
        return op === OperationType.InorderTraversal || op === OperationType.PreorderTraversal
            || op === OperationType.PostorderTraversal;
    }

    function traversalOperation(order) {
        switch (order) {
        case TraversalOrder.Preorder: return OperationType.PreorderTraversal;
        case TraversalOrder.Postorder: return OperationType.PostorderTraversal;
        default: return OperationType.InorderTraversal;
        }
    }

    // "45 > 30" style comparison text for events that compared two keys.
    function comparisonText(e) {
        if (e.comparedKey === null || e.comparedKey === undefined)
            return '';
        const other = e.comparedKey;
        const op = e.key < other ? ' < ' : (e.key > other ? ' > ' : ' = ');
        return `${e.key}${op}${other}`;
    }

    const joinKeys = (keys, separator = ', ') => keys.join(separator);

    // -----------------------------------------------------------------------
    // Nodes, snapshots, validation
    // -----------------------------------------------------------------------

    class BSTNode {
        constructor(key, id) {
            this.key = key;
            this.id = id;
            this.left = null;
            this.right = null;
        }
        isLeaf() { return !this.left && !this.right; }
        childCount() { return (this.left ? 1 : 0) + (this.right ? 1 : 0); }
    }

    // Immutable, pointer-free copy of a tree's structure at one moment.
    // Nodes are stored in preorder (root first); relationships are ids.
    class TreeSnapshot {
        constructor(root = NO_NODE, nodes = []) {
            this.root = root;
            this.nodes = nodes;
            this.byId = new Map(nodes.map((n) => [n.id, n]));
            Object.freeze(this.nodes);
        }

        get size() { return this.nodes.length; }
        empty() { return this.nodes.length === 0; }
        find(id) { return id === NO_NODE ? null : (this.byId.get(id) || null); }
        findKey(key) { return this.nodes.find((n) => n.key === key) || null; }

        // Height measured in edges: empty tree = -1, single node = 0.
        height() {
            let result = -1;
            for (const n of this.nodes)
                result = Math.max(result, n.depth);
            return result;
        }

        // Node ids in inorder (ascending key) order, computed iteratively.
        inorderIds() {
            const order = [];
            const stack = [];
            let current = this.root;
            while (current !== NO_NODE || stack.length) {
                while (current !== NO_NODE) {
                    stack.push(current);
                    current = this.find(current).left;
                }
                current = stack.pop();
                order.push(current);
                current = this.find(current).right;
            }
            return order;
        }

        inorderKeys() { return this.inorderIds().map((id) => this.find(id).key); }
        preorderKeys() { return this.nodes.map((n) => n.key); }

        // Ids from the root down to (and including) the given node.
        pathTo(id) {
            const path = [];
            for (let n = this.find(id); n; n = this.find(n.parent))
                path.push(n.id);
            return path.reverse();
        }
    }

    // Checks the BST invariant on a live tree with an explicit (lower, upper)
    // bound per node, so it checks the whole-subtree rule, not just
    // parent/child pairs. Also rejects cycles, shared nodes and duplicates.
    function validateTree(root) {
        const report = { valid: true, error: '', nodeCount: 0, height: -1 };
        const fail = (message) => Object.assign(report, { valid: false, error: message });
        const seenNodes = new Set();
        const seenIds = new Set();
        const seenKeys = new Set();
        const stack = root ? [{ node: root, lower: null, upper: null, depth: 0 }] : [];
        while (stack.length) {
            const { node, lower, upper, depth } = stack.pop();
            if (seenNodes.has(node))
                return fail(`node ${node.key} is reachable twice (cycle or shared ownership)`);
            seenNodes.add(node);
            if (seenIds.has(node.id))
                return fail(`node id ${node.id} is used twice`);
            seenIds.add(node.id);
            if (seenKeys.has(node.key))
                return fail(`duplicate key ${node.key}`);
            seenKeys.add(node.key);
            if (lower !== null && node.key <= lower)
                return fail(`key ${node.key} must be greater than ${lower}`);
            if (upper !== null && node.key >= upper)
                return fail(`key ${node.key} must be less than ${upper}`);

            ++report.nodeCount;
            report.height = Math.max(report.height, depth);
            if (node.left)
                stack.push({ node: node.left, lower, upper: node.key, depth: depth + 1 });
            if (node.right)
                stack.push({ node: node.right, lower: node.key, upper, depth: depth + 1 });
        }
        return report;
    }

    // Checks that a snapshot is internally consistent (links agree, depths are
    // correct, every node is reachable) and satisfies the BST invariant.
    function validateSnapshot(snapshot) {
        const report = { valid: true, error: '', nodeCount: 0, height: -1 };
        const fail = (message) => Object.assign(report, { valid: false, error: message });
        if (snapshot.root === NO_NODE)
            return snapshot.nodes.length ? fail('snapshot has nodes but no root') : report;
        const root = snapshot.find(snapshot.root);
        if (!root)
            return fail('root id not present in snapshot');
        if (root.parent !== NO_NODE || root.depth !== 0)
            return fail('root must have no parent and depth 0');

        const seen = new Set();
        const keys = new Set();
        const stack = [{ node: root, lower: null, upper: null }];
        while (stack.length) {
            const { node, lower, upper } = stack.pop();
            if (seen.has(node.id))
                return fail('snapshot node reachable twice');
            seen.add(node.id);
            if (keys.has(node.key))
                return fail(`duplicate key ${node.key}`);
            keys.add(node.key);
            if ((lower !== null && node.key <= lower) || (upper !== null && node.key >= upper))
                return fail(`ordering violated at key ${node.key}`);
            ++report.nodeCount;
            report.height = Math.max(report.height, node.depth);

            for (const childId of [node.left, node.right]) {
                if (childId === NO_NODE)
                    continue;
                const child = snapshot.find(childId);
                if (!child)
                    return fail('child id missing from snapshot');
                if (child.parent !== node.id || child.depth !== node.depth + 1)
                    return fail(`parent link or depth inconsistent at key ${child.key}`);
                const isLeft = childId === node.left;
                stack.push({ node: child, lower: isLeft ? lower : node.key, upper: isLeft ? node.key : upper });
            }
        }
        if (report.nodeCount !== snapshot.size)
            return fail('snapshot contains unreachable nodes');
        return report;
    }

    // -----------------------------------------------------------------------
    // Recorder
    // -----------------------------------------------------------------------

    function makeEvent() {
        return {
            type: EventType.OperationStarted,
            operation: OperationType.Insert,
            line: PseudoLine.None,
            snapshotIndex: 0,
            nodeId: NO_NODE,
            emphasis: Emphasis.Active,
            key: 0,
            comparedKey: null,
            direction: Direction.None,
            secondaryId: NO_NODE,
            secondaryRole: SecondaryRole.None,
            pendingParent: NO_NODE,
            pendingSide: Direction.None,
            deletionCase: DeletionCase.None,
            tentative: false,
            path: [],
            completed: [],
            sequence: [],
            metrics: { nodesVisited: 0, keyComparisons: 0, traversalVisits: 0, structuralChanges: 0 },
            action: '',
            reason: '',
        };
    }

    // Collects the events and snapshots of one operation. Path, sequence and
    // metrics are running state that gets copied into each emitted event.
    // A non-recording Recorder lets the same algorithm code run without
    // storing anything, e.g. for bulk tree construction.
    class Recorder {
        constructor(op, record = true) {
            this.recording = record;
            this.operation = op;
            this.record = {
                result: {
                    operation: op,
                    status: ResultStatus.Success,
                    code: ResultCode.Ok,
                    key: 0,
                    secondaryKey: null,
                    message: '',
                    metrics: null,
                    sequence: [],
                    deletionCase: DeletionCase.None,
                    treeChanged: false,
                },
                snapshots: [], // [0] = state before the operation
                events: [],
            };
            this.metrics = { nodesVisited: 0, keyComparisons: 0, traversalVisits: 0, structuralChanges: 0 };
            this.path = [];
            this.completed = [];
            this.sequence = [];
            this.tentative = false;
            // While set, every emitted event uses this line (nested phases of update).
            this.lineOverride = PseudoLine.None;
        }

        get result() { return this.record.result; }

        capture(treeOrSnapshot) {
            if (this.recording)
                this.record.snapshots.push(treeOrSnapshot instanceof TreeSnapshot ? treeOrSnapshot : treeOrSnapshot.snapshot());
        }

        emit(type, line, node = NO_NODE) {
            const event = makeEvent();
            if (!this.recording)
                return event; // scratch event, discarded
            event.type = type;
            event.operation = this.operation;
            event.line = this.lineOverride !== PseudoLine.None ? this.lineOverride : line;
            event.snapshotIndex = this.record.snapshots.length - 1;
            event.nodeId = node;
            event.tentative = this.tentative;
            event.path = this.path.slice();
            event.completed = this.completed.slice();
            event.sequence = this.sequence.slice();
            event.metrics = { ...this.metrics };
            this.record.events.push(event);
            return event;
        }

        finish(status, code, message) {
            Object.assign(this.record.result, {
                status,
                code,
                message,
                metrics: { ...this.metrics },
                sequence: this.sequence.slice(),
            });
            return this.record;
        }
    }

    // A "link" is the owning pointer of a node: the tree's root pointer or a
    // parent's left/right field. JS has no pointer-to-member, so links are
    // small get/set objects; this mirrors std::unique_ptr<BSTNode>* in C++.
    const rootLink = (tree) => ({ get: () => tree.root, set: (n) => { tree.root = n; } });
    const childLink = (node, side) => ({ get: () => node[side], set: (n) => { node[side] = n; } });

    // Standard iterative BST descent. Emits a KeyCompared and a MoveLeft/Right
    // event for every node passed, and returns the link of either the node
    // holding `key` (found) or the empty slot where it would belong.
    function descend(link, key, rec, lines) {
        const out = { link, found: false, parentId: NO_NODE, parentKey: 0, side: Direction.None };
        while (link.get()) {
            const node = link.get();
            rec.path.push(node.id);
            ++rec.metrics.nodesVisited;
            ++rec.metrics.keyComparisons;
            if (key === node.key) {
                out.found = true;
                out.link = link;
                return out;
            }

            const dir = key < node.key ? Direction.Left : Direction.Right;
            const next = childLink(node, dir);
            out.parentId = node.id;
            out.parentKey = node.key;
            out.side = dir;
            if (!rec.recording) {
                link = next;
                continue;
            }

            const cmp = rec.emit(EventType.KeyCompared, lines.compare, node.id);
            cmp.key = key;
            cmp.comparedKey = node.key;
            cmp.direction = dir;
            cmp.action = `Compare ${key} with ${node.key}`;
            cmp.reason = `${key}${dir === Direction.Left ? ' < ' : ' > '}${node.key}, so ${key} can only be in the `
                + `${dir} subtree of ${node.key}.`;

            const move = rec.emit(dir === Direction.Left ? EventType.MoveLeft : EventType.MoveRight,
                dir === Direction.Left ? lines.goLeft : lines.goRight, node.id);
            move.key = key;
            move.direction = dir;
            const child = next.get();
            if (child) {
                move.secondaryId = child.id;
                move.secondaryRole = SecondaryRole.NextNode;
                move.action = `Move to the ${dir} child (${child.key})`;
                move.reason = `The whole ${dir === Direction.Left ? 'right' : 'left'} subtree of ${node.key} is skipped: `
                    + `none of its keys can equal ${key}.`;
            } else {
                move.pendingParent = node.id;
                move.pendingSide = dir;
                move.action = `The ${dir} child of ${node.key} is empty`;
                move.reason = `The search path ends here: there is no ${dir} subtree below ${node.key}.`;
            }
            link = next;
        }
        out.link = link;
        return out;
    }

    function traversalName(order) { return order; }

    // Recursive traversal. Recursion depth equals the height of the tree.
    function walk(node, order, rec, entryLine) {
        rec.path.push(node.id);
        ++rec.metrics.nodesVisited;

        const enter = rec.emit(EventType.NodeVisited, entryLine, node.id);
        enter.key = node.key;
        if (entryLine === PseudoLine.TrvCall)
            enter.action = `Enter the root, ${node.key}`;
        else
            enter.action = `Go to the ${entryLine === PseudoLine.TrvLeft ? 'left' : 'right'} child, ${node.key}`;
        switch (order) {
        case TraversalOrder.Inorder:
            enter.reason = node.left ? `Inorder finishes the left subtree of ${node.key} before outputting it.`
                : `${node.key} has no left subtree, so it is output next.`;
            break;
        case TraversalOrder.Preorder:
            enter.reason = 'Preorder outputs a node as soon as it is reached, before either subtree.';
            break;
        case TraversalOrder.Postorder:
            enter.reason = node.isLeaf() ? `${node.key} is a leaf, so it can be output immediately.`
                : `Postorder finishes both subtrees of ${node.key} before outputting it.`;
            break;
        }

        const visit = () => {
            rec.sequence.push(node.key);
            rec.completed.push(node.id);
            ++rec.metrics.traversalVisits;
            const out = rec.emit(EventType.TraversalNodeVisited, PseudoLine.TrvVisit, node.id);
            out.key = node.key;
            out.emphasis = Emphasis.Success;
            out.action = `Visit ${node.key}: append it to the output`;
            switch (order) {
            case TraversalOrder.Inorder:
                out.reason = 'Its left subtree is finished; it is output before its right subtree.';
                break;
            case TraversalOrder.Preorder:
                out.reason = 'Root first: it is output before its left and right subtrees.';
                break;
            case TraversalOrder.Postorder:
                out.reason = 'Both of its subtrees are finished, so it is output now.';
                break;
            }
        };
        const goLeft = () => { if (node.left) walk(node.left, order, rec, PseudoLine.TrvLeft); };
        const goRight = () => { if (node.right) walk(node.right, order, rec, PseudoLine.TrvRight); };

        switch (order) {
        case TraversalOrder.Inorder: goLeft(); visit(); goRight(); break;
        case TraversalOrder.Preorder: visit(); goLeft(); goRight(); break;
        case TraversalOrder.Postorder: goLeft(); goRight(); visit(); break;
        }
        rec.path.pop();
    }

    function copySubtree(node) {
        if (!node)
            return null;
        const copy = new BSTNode(node.key, node.id);
        copy.left = copySubtree(node.left);
        copy.right = copySubtree(node.right);
        return copy;
    }

    // -----------------------------------------------------------------------
    // BinarySearchTree
    // -----------------------------------------------------------------------

    // Binary Search Tree of unique integer keys. Duplicate keys are rejected.
    class BinarySearchTree {
        constructor() {
            this.root = null;
            this.size = 0;
            this.nextId = 0;
            this.capacity = Infinity;
        }

        static sampleKeys() { return [50, 30, 70, 20, 40, 60, 80]; }

        // --- Queries ---

        empty() { return this.size === 0; }

        contains(key) {
            let node = this.root;
            while (node) {
                if (key === node.key)
                    return true;
                node = key < node.key ? node.left : node.right;
            }
            return false;
        }

        height() { return validateTree(this.root).height; }
        inorder() { return this.traverse(TraversalOrder.Inorder).result.sequence; }
        preorder() { return this.traverse(TraversalOrder.Preorder).result.sequence; }
        postorder() { return this.traverse(TraversalOrder.Postorder).result.sequence; }

        snapshot() {
            const nodes = [];
            const stack = this.root ? [{ node: this.root, parent: NO_NODE, depth: 0 }] : [];
            while (stack.length) {
                const { node: n, parent, depth } = stack.pop();
                nodes.push(Object.freeze({
                    id: n.id,
                    key: n.key,
                    parent,
                    left: n.left ? n.left.id : NO_NODE,
                    right: n.right ? n.right.id : NO_NODE,
                    depth,
                }));
                if (n.right)
                    stack.push({ node: n.right, parent: n.id, depth: depth + 1 });
                if (n.left)
                    stack.push({ node: n.left, parent: n.id, depth: depth + 1 });
            }
            return new TreeSnapshot(this.root ? this.root.id : NO_NODE, nodes);
        }

        validate() {
            const report = validateTree(this.root);
            if (report.valid && report.nodeCount !== this.size) {
                report.valid = false;
                report.error = `size counter is ${this.size} but ${report.nodeCount} nodes are reachable`;
            }
            return report;
        }

        verifyInvariant() {
            const report = this.validate();
            if (!report.valid)
                throw new Error(`BST invariant violated: ${report.error}`);
        }

        // Deep copy that preserves node ids (used for transactional updates).
        clone() {
            const copy = new BinarySearchTree();
            copy.root = copySubtree(this.root);
            copy.size = this.size;
            copy.nextId = this.nextId;
            copy.capacity = this.capacity;
            return copy;
        }

        assignFrom(other) {
            this.root = other.root;
            this.size = other.size;
            this.nextId = other.nextId;
            this.capacity = other.capacity;
        }

        clear() {
            this.root = null;
            this.size = 0;
        }

        // --- Insertion ---

        insert(key) {
            const rec = new Recorder(OperationType.Insert);
            rec.result.key = key;
            rec.capture(this);

            if (this.size >= this.capacity) {
                const e = rec.emit(EventType.OperationRejected, PseudoLine.InsInit);
                e.key = key;
                e.emphasis = Emphasis.Error;
                e.action = 'The tree is full';
                e.reason = `This demonstration limits the tree to ${this.capacity} nodes so every node stays readable. `
                    + 'Delete a key first.';
                return rec.finish(ResultStatus.Rejected, ResultCode.CapacityReached, 'Insertion rejected: the tree is full.');
            }

            const start = rec.emit(EventType.OperationStarted, PseudoLine.InsInit, this.root ? this.root.id : NO_NODE);
            start.key = key;
            if (this.root) {
                start.action = `Insert ${key}: start at the root (${this.root.key})`;
                start.reason = 'Insertion follows the search path from the root until it reaches an empty child pointer.';
            } else {
                start.action = `Insert ${key} into an empty tree`;
                start.reason = 'There are no nodes, so the new key becomes the root.';
            }

            const code = this.insertSteps(key, rec);
            if (code !== ResultCode.Ok)
                return rec.finish(ResultStatus.Rejected, code, 'Key already exists.');

            this.verifyInvariant();
            const done = rec.emit(EventType.OperationCompleted, PseudoLine.InsDone, rec.path[rec.path.length - 1]);
            done.key = key;
            done.emphasis = Emphasis.Success;
            done.action = 'Insertion completed';
            done.reason = `BST invariant verified. The tree now holds ${this.size} node(s) with height ${this.height()}.`;
            rec.result.treeChanged = true;
            return rec.finish(ResultStatus.Success, ResultCode.Ok, 'Insertion completed.');
        }

        insertSteps(key, rec) {
            const d = descend(rootLink(this), key, rec,
                { compare: PseudoLine.InsCompare, goLeft: PseudoLine.InsGoLeft, goRight: PseudoLine.InsGoRight });

            if (d.found) {
                const existing = d.link.get();
                const e = rec.emit(EventType.OperationRejected, PseudoLine.InsDuplicate, existing.id);
                e.key = key;
                e.comparedKey = existing.key;
                e.emphasis = Emphasis.Error;
                e.action = `Key ${key} already exists`;
                e.reason = 'Duplicate keys are rejected so every key has exactly one correct position. The tree is unchanged.';
                return ResultCode.DuplicateKey;
            }

            const id = this.nextId++;
            d.link.set(new BSTNode(key, id));
            ++this.size;
            ++rec.metrics.structuralChanges;
            rec.path.push(id);
            rec.capture(this);

            const e = rec.emit(EventType.NodeInserted, PseudoLine.InsAttach, id);
            e.key = key;
            e.emphasis = Emphasis.Success;
            e.direction = d.side;
            if (d.parentId === NO_NODE) {
                e.action = `Insert ${key} as the root`;
                e.reason = 'The tree was empty, so the new node becomes the root.';
            } else {
                e.secondaryId = d.parentId;
                e.secondaryRole = SecondaryRole.Parent;
                e.action = `Attach ${key} as the ${d.side} child of ${d.parentKey}`;
                e.reason = "This is the first empty position on the search path, so every ancestor's ordering rule still holds.";
            }
            return ResultCode.Ok;
        }

        // Same insertion algorithm, without recording (used to build whole trees).
        insertQuiet(key) {
            return this.insertSteps(key, new Recorder(OperationType.Insert, false));
        }

        // --- Search ---

        search(key) {
            const rec = new Recorder(OperationType.Search);
            rec.result.key = key;
            rec.capture(this);

            if (!this.root) {
                const e = rec.emit(EventType.NodeNotFound, PseudoLine.SrchNotFound);
                e.key = key;
                e.emphasis = Emphasis.Error;
                e.action = `Key ${key} not found: the tree is empty`;
                e.reason = 'There is no root to start from, so the key cannot be present.';
                return rec.finish(ResultStatus.NotFound, ResultCode.KeyNotFound, 'Key not found.');
            }

            const start = rec.emit(EventType.OperationStarted, PseudoLine.SrchInit, this.root.id);
            start.key = key;
            start.action = `Search for ${key}: start at the root (${this.root.key})`;
            start.reason = 'Each comparison discards one whole subtree, so at most h + 1 nodes are examined.';

            const d = descend(rootLink(this), key, rec,
                { compare: PseudoLine.SrchCompare, goLeft: PseudoLine.SrchGoLeft, goRight: PseudoLine.SrchGoRight });

            if (d.found) {
                const node = d.link.get();
                const e = rec.emit(EventType.NodeFound, PseudoLine.SrchFound, node.id);
                e.key = key;
                e.comparedKey = node.key;
                e.emphasis = Emphasis.Success;
                e.action = `Key ${key} found`;
                e.reason = `${key} = ${node.key}. Found at depth ${rec.path.length - 1} after visiting `
                    + `${rec.metrics.nodesVisited} node(s).`;
                return rec.finish(ResultStatus.Success, ResultCode.KeyFound, 'Key found.');
            }

            const e = rec.emit(EventType.NodeNotFound, PseudoLine.SrchNotFound);
            e.key = key;
            e.emphasis = Emphasis.Error;
            e.pendingParent = d.parentId;
            e.pendingSide = d.side;
            e.action = `Key ${key} not found`;
            e.reason = `Reached the empty ${d.side} pointer of ${d.parentKey}. If ${key} existed it would have to be here. `
                + `Visited ${rec.metrics.nodesVisited} node(s).`;
            return rec.finish(ResultStatus.NotFound, ResultCode.KeyNotFound, 'Key not found.');
        }

        // --- Deletion ---

        remove(key) {
            const rec = new Recorder(OperationType.Delete);
            rec.result.key = key;
            rec.capture(this);

            if (!this.root) {
                const e = rec.emit(EventType.NodeNotFound, PseudoLine.DelNotFound);
                e.key = key;
                e.emphasis = Emphasis.Error;
                e.action = `Key ${key} not found: the tree is empty`;
                e.reason = 'Nothing can be deleted from an empty tree. The tree is unchanged.';
                return rec.finish(ResultStatus.NotFound, ResultCode.KeyNotFound, 'Key not found.');
            }

            const start = rec.emit(EventType.OperationStarted, PseudoLine.DelSearch, this.root.id);
            start.key = key;
            start.action = `Delete ${key}: first search for it from the root (${this.root.key})`;
            start.reason = 'A node can only be removed after locating it and the parent pointer that holds it.';

            const code = this.removeSteps(key, rec);
            if (code !== ResultCode.Ok)
                return rec.finish(ResultStatus.NotFound, code, 'Key not found.');

            this.verifyInvariant();
            rec.path = [];
            const done = rec.emit(EventType.OperationCompleted, PseudoLine.DelVerify);
            done.key = key;
            done.emphasis = Emphasis.Success;
            done.deletionCase = rec.result.deletionCase;
            done.action = 'Deletion completed';
            done.reason = `BST invariant verified. The tree now holds ${this.size} node(s).`;
            rec.result.treeChanged = true;
            return rec.finish(ResultStatus.Success, ResultCode.Ok, 'Deletion completed.');
        }

        removeSteps(key, rec) {
            const d = descend(rootLink(this), key, rec,
                { compare: PseudoLine.DelSearch, goLeft: PseudoLine.DelSearch, goRight: PseudoLine.DelSearch });

            if (!d.found) {
                const e = rec.emit(EventType.NodeNotFound, PseudoLine.DelNotFound);
                e.key = key;
                e.emphasis = Emphasis.Error;
                e.pendingParent = d.parentId;
                e.pendingSide = d.side;
                e.action = `Key ${key} not found`;
                e.reason = `Reached an empty pointer, so ${key} is not in the tree. Nothing is removed.`;
                return ResultCode.KeyNotFound;
            }

            const link = d.link;
            const target = link.get();
            const targetId = target.id;

            const found = rec.emit(EventType.NodeFound, PseudoLine.DelSearch, targetId);
            found.key = key;
            found.comparedKey = target.key;
            found.action = `Found ${key}`;
            found.reason = 'Next, count its children to decide which of the three deletion cases applies.';

            if (target.isLeaf()) {
                rec.result.deletionCase = DeletionCase.Leaf;
                const c = rec.emit(EventType.DeletionCaseIdentified, PseudoLine.DelCaseLeaf, targetId);
                c.key = key;
                c.emphasis = Emphasis.Removing;
                c.deletionCase = DeletionCase.Leaf;
                c.action = `Case 1: ${key} is a leaf`;
                c.reason = 'It has no children, so removing it cannot disconnect any other node. '
                    + 'Only the pointer that holds it must be cleared.';

                link.set(null);
                --this.size;
                ++rec.metrics.structuralChanges;
                rec.path.pop();
                rec.capture(this);

                const r = rec.emit(EventType.NodeRemoved, PseudoLine.DelRemoveLeaf, targetId);
                r.key = key;
                r.emphasis = Emphasis.Removing;
                r.deletionCase = DeletionCase.Leaf;
                if (d.parentId === NO_NODE) {
                    r.action = `Removed ${key}; the tree is now empty`;
                    r.reason = 'It was the only node, so the root pointer becomes null.';
                } else {
                    r.secondaryId = d.parentId;
                    r.secondaryRole = SecondaryRole.Parent;
                    r.action = `Removed ${key}; the ${d.side} pointer of ${d.parentKey} is now null`;
                    r.reason = 'No other node moved, so the ordering of the rest of the tree is untouched.';
                }
            } else if (target.childCount() === 1) {
                const child = target.left || target.right;
                const childId = child.id;
                const childKey = child.key;
                rec.result.deletionCase = DeletionCase.OneChild;

                const c = rec.emit(EventType.DeletionCaseIdentified, PseudoLine.DelCaseOne, targetId);
                c.key = key;
                c.emphasis = Emphasis.Removing;
                c.deletionCase = DeletionCase.OneChild;
                c.secondaryId = childId;
                c.secondaryRole = SecondaryRole.OnlyChild;
                c.action = `Case 2: ${key} has one child (${childKey})`;
                c.reason = d.parentId === NO_NODE
                    ? "The child's subtree can simply become the whole tree."
                    : `Every key below ${key} lies on the same side of ${d.parentKey} as ${key} itself, so the child can take its place.`;

                link.set(child);
                --this.size;
                ++rec.metrics.structuralChanges;
                rec.path.pop();
                rec.path.push(childId);
                rec.capture(this);

                const r = rec.emit(EventType.NodeRemoved, PseudoLine.DelSpliceChild, targetId);
                r.key = key;
                r.emphasis = Emphasis.Removing;
                r.deletionCase = DeletionCase.OneChild;
                r.secondaryId = childId;
                r.secondaryRole = SecondaryRole.OnlyChild;
                r.action = d.parentId === NO_NODE ? `${childKey} is now the root` : `Linked ${d.parentKey} directly to ${childKey}`;
                r.reason = `${key} is gone and the subtree rooted at ${childKey} moved up one level. Its internal shape is unchanged.`;
            } else {
                rec.result.deletionCase = DeletionCase.TwoChildren;
                const c = rec.emit(EventType.DeletionCaseIdentified, PseudoLine.DelCaseTwo, targetId);
                c.key = key;
                c.emphasis = Emphasis.Removing;
                c.deletionCase = DeletionCase.TwoChildren;
                c.action = `Case 3: ${key} has two children`;
                c.reason = 'Removing the node would leave two subtrees competing for one pointer. Instead, its key is '
                    + 'replaced by its inorder successor: the smallest key in its right subtree.';

                let succLink = childLink(target, 'right');
                rec.path.push(succLink.get().id);
                ++rec.metrics.nodesVisited;
                const s = rec.emit(EventType.SuccessorSearchStarted, PseudoLine.DelFindSucc, succLink.get().id);
                s.key = succLink.get().key;
                s.deletionCase = DeletionCase.TwoChildren;
                s.secondaryId = targetId;
                s.secondaryRole = SecondaryRole.Target;
                s.action = `Step into the right subtree of ${key} (${succLink.get().key})`;
                s.reason = `The successor is the next larger key, so it must be in the right subtree of ${key}.`;

                while (succLink.get().left) {
                    succLink = childLink(succLink.get(), 'left');
                    const n = succLink.get();
                    rec.path.push(n.id);
                    ++rec.metrics.nodesVisited;
                    const v = rec.emit(EventType.NodeVisited, PseudoLine.DelFindSucc, n.id);
                    v.key = n.key;
                    v.deletionCase = DeletionCase.TwoChildren;
                    v.secondaryId = targetId;
                    v.secondaryRole = SecondaryRole.Target;
                    v.action = `Move left to ${n.key}`;
                    v.reason = 'A left child holds a smaller key, so keep going left to reach the minimum.';
                }

                const succ = succLink.get();
                const succId = succ.id;
                const succKey = succ.key;
                const f = rec.emit(EventType.SuccessorFound, PseudoLine.DelFindSucc, succId);
                f.key = succKey;
                f.emphasis = Emphasis.Success;
                f.deletionCase = DeletionCase.TwoChildren;
                f.secondaryId = targetId;
                f.secondaryRole = SecondaryRole.Target;
                f.action = `The inorder successor is ${succKey}`;
                f.reason = `${succKey} has no left child, so it is the minimum of the right subtree: larger than every `
                    + `key on the left of ${key} and smaller than every other key on its right.`;

                // Unlink the successor first and remember its key, then write
                // that key into the target, so every intermediate state is a
                // valid BST (copying first would briefly duplicate the key).
                const replacement = succ.right;
                succLink.set(succ.right); // the successor never has a left child
                --this.size;
                ++rec.metrics.structuralChanges;
                rec.path.pop();
                rec.capture(this);

                const r = rec.emit(EventType.NodeRemoved, PseudoLine.DelRemoveSucc, succId);
                r.key = succKey;
                r.emphasis = Emphasis.Removing;
                r.deletionCase = DeletionCase.TwoChildren;
                r.action = `Unlink the successor's node and keep its key (${succKey})`;
                if (replacement) {
                    r.secondaryId = replacement.id;
                    r.secondaryRole = SecondaryRole.OnlyChild;
                    r.reason = `It has no left child, so its right child (${replacement.key}) takes its place: the one-child case.`;
                } else {
                    r.reason = "It is a leaf, so its parent's pointer is simply cleared: the leaf case.";
                }

                target.key = succKey;
                ++rec.metrics.structuralChanges;
                rec.capture(this);
                const k = rec.emit(EventType.KeyUpdated, PseudoLine.DelCopyKey, targetId);
                k.key = succKey;
                k.comparedKey = null;
                k.emphasis = Emphasis.Success;
                k.deletionCase = DeletionCase.TwoChildren;
                k.action = `Write ${succKey} into the node that held ${key}`;
                k.reason = `The node keeps its position and both children; only its key changes. ${succKey} is larger `
                    + 'than everything in the left subtree and smaller than everything left in the right subtree, so '
                    + 'the order is preserved.';
            }
            return ResultCode.Ok;
        }

        // --- Update ---

        update(oldKey, newKey) {
            const rec = new Recorder(OperationType.Update);
            rec.result.key = oldKey;
            rec.result.secondaryKey = newKey;
            const original = this.snapshot();
            rec.capture(original);

            if (!this.root) {
                const e = rec.emit(EventType.OperationRejected, PseudoLine.UpdOldMissing);
                e.key = oldKey;
                e.emphasis = Emphasis.Error;
                e.action = `Key ${oldKey} does not exist: the tree is empty`;
                e.reason = 'There is nothing to update. The tree is unchanged.';
                return rec.finish(ResultStatus.Rejected, ResultCode.KeyMissing, 'Update rejected: the old key does not exist.');
            }

            const start = rec.emit(EventType.OperationStarted, PseudoLine.UpdFindOld, this.root.id);
            start.key = oldKey;
            start.action = `Update ${oldKey} → ${newKey}: locate ${oldKey}`;
            start.reason = 'An update first confirms that the old key exists.';

            const d = descend(rootLink(this), oldKey, rec,
                { compare: PseudoLine.UpdFindOld, goLeft: PseudoLine.UpdFindOld, goRight: PseudoLine.UpdFindOld });
            if (!d.found) {
                const e = rec.emit(EventType.OperationRejected, PseudoLine.UpdOldMissing);
                e.key = oldKey;
                e.emphasis = Emphasis.Error;
                e.pendingParent = d.parentId;
                e.pendingSide = d.side;
                e.action = `Key ${oldKey} does not exist`;
                e.reason = 'There is nothing to update. The tree is unchanged.';
                return rec.finish(ResultStatus.Rejected, ResultCode.KeyMissing, 'Update rejected: the old key does not exist.');
            }
            const oldId = d.link.get().id;

            const found = rec.emit(EventType.NodeFound, PseudoLine.UpdFindOld, oldId);
            found.key = oldKey;
            found.comparedKey = oldKey;
            found.action = `Found ${oldKey}`;
            found.reason = 'Next, check that the new key is acceptable.';

            if (oldKey === newKey) {
                const e = rec.emit(EventType.OperationCompleted, PseudoLine.UpdSame, oldId);
                e.key = oldKey;
                e.emphasis = Emphasis.Neutral;
                e.action = 'No change required';
                e.reason = `The new key equals the old key (${oldKey}), so the tree already holds the requested value.`;
                return rec.finish(ResultStatus.NoChange, ResultCode.SameKey, 'No change required: the keys are identical.');
            }

            rec.path = [];
            const check = rec.emit(EventType.PhaseStarted, PseudoLine.UpdCheckNew, this.root.id);
            check.key = newKey;
            check.action = `Check whether ${newKey} already exists`;
            check.reason = 'Keys must stay unique, so the new key must not already be stored at another node.';

            const dn = descend(rootLink(this), newKey, rec,
                { compare: PseudoLine.UpdCheckNew, goLeft: PseudoLine.UpdCheckNew, goRight: PseudoLine.UpdCheckNew });
            if (dn.found) {
                const e = rec.emit(EventType.OperationRejected, PseudoLine.UpdCheckNew, dn.link.get().id);
                e.key = newKey;
                e.comparedKey = newKey;
                e.emphasis = Emphasis.Error;
                e.action = `${newKey} already exists at another node`;
                e.reason = 'Accepting it would create a duplicate key. The update is rejected and the tree is unchanged.';
                return rec.finish(ResultStatus.Rejected, ResultCode.NewKeyExists, 'Update rejected: the new key already exists.');
            }
            const available = rec.emit(EventType.NodeNotFound, PseudoLine.UpdCheckNew);
            available.key = newKey;
            available.pendingParent = dn.parentId;
            available.pendingSide = dn.side;
            available.action = `${newKey} is not in the tree`;
            available.reason = 'The new key is free to use.';

            // Explain why the key cannot simply be overwritten: a key at this
            // node must lie strictly between its inorder neighbours.
            const keys = original.inorderKeys();
            const pos = keys.indexOf(oldKey);
            const hasPred = pos > 0;
            const hasSucc = pos + 1 < keys.length;
            const fitsInPlace = (!hasPred || keys[pos - 1] < newKey) && (!hasSucc || newKey < keys[pos + 1]);
            let bounds;
            if (hasPred && hasSucc)
                bounds = `greater than ${keys[pos - 1]} and less than ${keys[pos + 1]}`;
            else if (hasPred)
                bounds = `greater than ${keys[pos - 1]}`;
            else if (hasSucc)
                bounds = `less than ${keys[pos + 1]}`;
            else
                bounds = 'any value (it is the only node)';

            rec.path = [];
            const working = this.clone();
            rec.tentative = true;
            rec.capture(working);
            const begin = rec.emit(EventType.TransactionStarted, PseudoLine.UpdBegin, oldId);
            begin.key = oldKey;
            begin.action = 'Begin a transaction on a working copy';
            if (fitsInPlace) {
                begin.reason = `A key at this node must be ${bounds}. ${newKey} happens to fit here, but in general a `
                    + `changed key belongs elsewhere, so the update always deletes ${oldKey} and inserts ${newKey}. `
                    + 'The real tree is untouched until both steps succeed.';
            } else {
                begin.reason = `A key at this node must be ${bounds}. Writing ${newKey} here would break the BST order, `
                    + `so the update deletes ${oldKey} and inserts ${newKey} where its own search path leads. `
                    + 'The real tree is untouched until both steps succeed.';
            }

            rec.lineOverride = PseudoLine.UpdDelete;
            const phase1 = rec.emit(EventType.PhaseStarted, PseudoLine.UpdDelete, oldId);
            phase1.key = oldKey;
            phase1.action = `Phase 1: delete ${oldKey} from the working copy`;
            phase1.reason = 'Standard BST deletion; the matching deletion case is chosen automatically.';
            const deleted = working.removeSteps(oldKey, rec);

            let inserted = ResultCode.KeyMissing;
            if (deleted === ResultCode.Ok) {
                rec.lineOverride = PseudoLine.UpdInsert;
                rec.path = [];
                const phase2 = rec.emit(EventType.PhaseStarted, PseudoLine.UpdInsert, working.root ? working.root.id : NO_NODE);
                phase2.key = newKey;
                phase2.action = `Phase 2: insert ${newKey} into the working copy`;
                phase2.reason = 'Standard BST insertion places the new key where its own search path ends.';
                inserted = working.insertSteps(newKey, rec);
            }
            rec.lineOverride = PseudoLine.None;

            const report = working.validate();
            if (deleted !== ResultCode.Ok || inserted !== ResultCode.Ok || !report.valid) {
                rec.tentative = false;
                rec.path = [];
                rec.capture(this);
                const e = rec.emit(EventType.TransactionRolledBack, PseudoLine.UpdRollback);
                e.key = oldKey;
                e.emphasis = Emphasis.Error;
                e.action = 'Transaction rolled back';
                e.reason = report.valid ? 'A phase did not succeed, so the working copy was discarded. The tree is unchanged.'
                    : `Invariant check failed (${report.error}), so the working copy was discarded. The tree is unchanged.`;
                return rec.finish(ResultStatus.Failed, ResultCode.InvariantViolation, 'Update failed: changes rolled back.');
            }

            const newId = rec.path.length ? rec.path[rec.path.length - 1] : NO_NODE;
            this.assignFrom(working);
            rec.tentative = false;
            rec.capture(this);
            const commit = rec.emit(EventType.TransactionCommitted, PseudoLine.UpdCommit, newId);
            commit.key = newKey;
            commit.emphasis = Emphasis.Success;
            commit.action = 'Update committed';
            commit.reason = `${oldKey} was replaced by ${newKey}. The working copy passed the BST invariant check before it replaced the tree.`;
            rec.result.treeChanged = true;
            return rec.finish(ResultStatus.Success, ResultCode.Ok, 'Update completed.');
        }

        // --- Traversal ---

        traverse(order) {
            const rec = new Recorder(traversalOperation(order));
            rec.capture(this);
            const name = traversalName(order);

            if (!this.root) {
                const e = rec.emit(EventType.OperationCompleted, PseudoLine.TrvDone);
                e.emphasis = Emphasis.Neutral;
                e.action = `${name} traversal of an empty tree`;
                e.reason = 'There are no nodes to visit, so the output sequence is empty.';
                return rec.finish(ResultStatus.Success, ResultCode.Ok, 'Traversal completed.');
            }

            const start = rec.emit(EventType.OperationStarted, PseudoLine.TrvStart, this.root.id);
            start.key = this.root.key;
            start.action = `${name} traversal: start at the root (${this.root.key})`;
            switch (order) {
            case TraversalOrder.Inorder:
                start.reason = 'Inorder = Left → Root → Right. For a BST it outputs the keys in ascending order.';
                break;
            case TraversalOrder.Preorder:
                start.reason = 'Preorder = Root → Left → Right. Each node is output as soon as it is reached.';
                break;
            case TraversalOrder.Postorder:
                start.reason = 'Postorder = Left → Right → Root. A node is output only after both subtrees are finished.';
                break;
            }

            walk(this.root, order, rec, PseudoLine.TrvCall);

            const done = rec.emit(EventType.OperationCompleted, PseudoLine.TrvDone);
            done.emphasis = Emphasis.Success;
            done.action = 'Traversal completed';
            done.reason = `Visited ${rec.metrics.traversalVisits} nodes. Output: ${joinKeys(rec.sequence)}`
                + (order === TraversalOrder.Inorder ? ' (ascending, as expected for a BST).' : '.');
            return rec.finish(ResultStatus.Success, ResultCode.Ok, 'Traversal completed.');
        }

        // --- Build / clear ---

        // Replaces the whole tree with `keys` inserted in order. Rejected, with
        // the current tree untouched, if keys contain duplicates or exceed capacity.
        rebuild(keys) {
            const rec = new Recorder(OperationType.Build);
            rec.capture(this);

            const seen = new Set();
            for (const key of keys) {
                if (seen.has(key)) {
                    const e = rec.emit(EventType.OperationRejected, PseudoLine.BldValidate);
                    e.key = key;
                    e.emphasis = Emphasis.Error;
                    e.action = `Duplicate key ${key}`;
                    e.reason = 'Keys in this BST must be unique. The current tree was not changed.';
                    rec.result.key = key;
                    return rec.finish(ResultStatus.Rejected, ResultCode.DuplicateKey,
                        `Duplicate key ${key}: the tree was not changed.`);
                }
                seen.add(key);
            }
            if (keys.length > this.capacity) {
                const e = rec.emit(EventType.OperationRejected, PseudoLine.BldValidate);
                e.emphasis = Emphasis.Error;
                e.action = `Too many keys (${keys.length})`;
                e.reason = `This demonstration supports at most ${this.capacity} nodes. The current tree was not changed.`;
                return rec.finish(ResultStatus.Rejected, ResultCode.CapacityReached, 'Too many keys: the tree was not changed.');
            }

            const start = rec.emit(EventType.OperationStarted, PseudoLine.BldValidate);
            start.action = `Validated ${keys.length} key(s)`;
            start.reason = 'All entries are integers in range and unique, so the tree can be built.';

            const fresh = new BinarySearchTree();
            fresh.capacity = this.capacity;
            fresh.nextId = this.nextId; // keep ids unique across rebuilds so animations never confuse nodes
            for (const key of keys) {
                if (fresh.insertQuiet(key) !== ResultCode.Ok)
                    throw new Error('rebuild: insertion of a validated key failed');
            }
            fresh.verifyInvariant();
            this.assignFrom(fresh);
            rec.result.treeChanged = true;
            rec.capture(this);

            const done = rec.emit(EventType.OperationCompleted, PseudoLine.BldInsert, this.root ? this.root.id : NO_NODE);
            done.emphasis = Emphasis.Success;
            if (!keys.length) {
                done.action = 'Created an empty tree';
                done.reason = 'No keys were given.';
            } else {
                done.action = `Built a tree of ${this.size} node(s)`;
                done.reason = `Inserted in the given order: ${joinKeys(keys)}. The first key (${keys[0]}) is the root; `
                    + 'insertion order determines the shape.';
            }
            return rec.finish(ResultStatus.Success, ResultCode.Ok, 'Tree created.');
        }

        clearTree() {
            const rec = new Recorder(OperationType.Clear);
            rec.capture(this);
            const released = this.size;

            const start = rec.emit(EventType.OperationStarted, PseudoLine.ClrRelease, this.root ? this.root.id : NO_NODE);
            start.action = `Release ${released} node(s)`;
            start.reason = 'Every node is detached from the tree, so each is released exactly once.';

            this.clear();
            rec.capture(this);
            rec.result.treeChanged = released > 0;

            const done = rec.emit(EventType.OperationCompleted, PseudoLine.ClrDone);
            done.emphasis = Emphasis.Success;
            done.action = 'Tree cleared';
            done.reason = 'The root pointer is null and the size is 0.';
            return rec.finish(ResultStatus.Success, ResultCode.Ok, 'Tree cleared.');
        }
    }

    const VDSA = global.VDSA || (global.VDSA = {});
    Object.assign(VDSA, {
        NO_NODE,
        OperationType,
        TraversalOrder,
        EventType,
        Direction,
        Emphasis,
        DeletionCase,
        SecondaryRole,
        PseudoLine,
        ResultStatus,
        ResultCode,
        operationName,
        isTraversal,
        traversalOperation,
        comparisonText,
        BSTNode,
        TreeSnapshot,
        validateTree,
        validateSnapshot,
        BinarySearchTree,
    });
})(typeof globalThis !== 'undefined' ? globalThis : this);
