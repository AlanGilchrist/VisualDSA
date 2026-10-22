# Algorithm Documentation

This document describes the algorithms implemented in `src/core/BinarySearchTree.cpp`. The pseudocode below matches the listings shown in the application, which come from `src/core/Pseudocode.cpp`.

## 1. Data structure

```cpp
struct BSTNode {
    int key;
    NodeId id;                        // stable identifier for visualization
    std::unique_ptr<BSTNode> left;    // each node owns its children
    std::unique_ptr<BSTNode> right;
};
```

- **Ownership.** `BinarySearchTree` owns the root through a `std::unique_ptr`, and every node owns its children. Each node therefore has exactly one owner and is freed exactly once. No node is deleted manually.
- **Stable ids.** Ids are assigned from a counter and never reused within a tree, including across `rebuild`. The visualizer follows nodes by id, so a node that moves keeps its identity on screen.
- **Duplicate policy.** Keys are unique. Inserting an existing key is rejected and the tree is left unchanged.

### BST invariant

For every node `x`, every key in `x.left`'s subtree is **less than** `x.key`, and every key in `x.right`'s subtree is **greater than** `x.key`.

`validateTree` in `TreeValidator.h` checks this rule for whole subtrees, not just for parent/child pairs. It carries an open interval `(lower, upper)` down the tree and also detects duplicate keys, duplicate ids and nodes reachable twice. It runs after every successful structural change, and a failure throws `std::logic_error`, because a failure can only mean a programming error.

### Height convention

Height is the number of **edges** on the longest root-to-leaf path:

- empty tree: −1
- single node: 0
- sample tree: 2

## 2. Search

```
search(key):
  node ← root
  while node ≠ null:
    compare key with node.key
    if key = node.key: return FOUND
    if key < node.key: node ← node.left
    else: node ← node.right
  return NOT FOUND
```

The search is iterative. Each three-way comparison either finds the key or discards an entire subtree. Reaching `null` proves the key is absent, because the key could only have been in that slot.

**Recorded events:** `OperationStarted`, then a `KeyCompared` and a `MoveLeft`/`MoveRight` pair per node, then `NodeFound` or `NodeNotFound`. The not-found event records the empty slot (`pendingParent`, `pendingSide`).

## 3. Insertion

```
insert(key):
  node ← root;  parent ← null
  while node ≠ null:
    compare key with node.key
    if key = node.key: reject duplicate
    if key < node.key: go left
    else: go right
  attach Node(key) under parent
  verify invariant; return success
```

The new key follows exactly the path a search for it would take. The first empty pointer on that path is the only position where every ancestor's ordering rule still holds. A new key always becomes a **leaf**.

Edge cases:

- **Empty tree:** the new node becomes the root.
- **Duplicate key:** rejected; the tree is unchanged.
- **Capacity reached:** the GUI limits the tree to 64 nodes so it stays readable. Insertion beyond that is rejected.

## 4. Deletion

```
delete(key):
  node ← search(key), tracking parent
  if node = null: report NOT FOUND
  if node has no children:
    parent.link ← null
  else if node has one child:
    parent.link ← that child
  else:  // two children
    succ ← leftmost of node.right
    unlink succ (← succ.right)
    node.key ← succ.key
  verify BST invariant
```

The implementation keeps a pointer to the **owning link**, a `std::unique_ptr<BSTNode>*`, instead of a parent pointer. That makes the root and non-root cases identical.

### Case 1: leaf

`link.reset()` frees the node. Nothing else moves.

### Case 2: one child

`*link = std::move(child)`. A `unique_ptr` move-assignment first releases the child from the doomed node and only then destroys that node, so the child's subtree survives intact and moves up one level. This is correct because every key in the child's subtree lies on the same side of the deleted node's parent as the deleted node itself.

### Case 3: two children

1. Step into the right subtree.
2. Go left until there is no left child. This node is the **inorder successor**: the smallest key greater than the target.
3. Unlink the successor. It has no left child, so this is case 1 or case 2.
4. Write the successor's key into the target node.

**Why the successor works.** It is larger than every key in the target's left subtree, because those keys are all smaller than the target, and the successor is larger than the target. It is also smaller than every other key in the right subtree, because it is that subtree's minimum. So it can sit in the target's position without breaking the order.

**Why unlink before copying.** The classic description copies the key first. That briefly leaves the successor's key in the tree twice, an invalid intermediate state the visualizer would display. Unlinking first gives the same final tree, and every intermediate state stays a valid BST. A test (`deleteTwoChildrenNeverShowsInvalidTree`) checks this.

Missing keys are reported as `NotFound` and the tree is unchanged.

## 5. Update

```
update(old, new):
  node ← search(old)
  if node = null: reject
  if new = old: no change
  if search(new) found: reject
  copy ← tree  // transaction
  delete(old) on copy
  insert(new) on copy
  if copy is valid: tree ← copy
  else: discard copy (rollback)
```

A key's position in a BST depends on its value, so overwriting it in place is not generally safe. In the sample tree, writing 65 into the node that holds 40 would place 65 in the *left* subtree of 50, which violates the ordering. An in-place write is only safe when the new key still lies strictly between the old key's inorder predecessor and successor. The application computes this bound and explains it at the "begin transaction" step.

The update is **logically atomic**:

- The checks happen first. If the old key is missing, the new key already exists, or the keys are identical, nothing changes.
- The deletion and insertion run on a deep **working copy**, made by `clone()`, which preserves node ids.
- The copy replaces the live tree only after `validate()` passes. Otherwise it is discarded and the original is untouched (rollback).
- Every event during this phase is marked `tentative`. The canvas shows a "WORKING COPY · NOT YET COMMITTED" badge, so the viewer never sees an uncommitted state presented as the real tree.

This differs from changing an array element: an array position is independent of the value stored there, but a BST position is *determined by* the value.

## 6. Traversals

All three are recursive, with recursion depth equal to the height *h*.

```
inorder(node):            preorder(node):           postorder(node):
  if node = null: return    if node = null: return    if node = null: return
  inorder(node.left)        output(node.key)          postorder(node.left)
  output(node.key)          preorder(node.left)       postorder(node.right)
  inorder(node.right)       preorder(node.right)      output(node.key)
```

The visualizer records two kinds of steps:

- `NodeVisited` when the recursion **enters** a node, highlighting the pseudocode line that made the call.
- `TraversalNodeVisited` when the node is **output**.

Separating the two makes the difference between the orders visible: every order enters the nodes in the same sequence, but outputs them at different moments.

For the sample tree:

| Order | Output |
|---|---|
| Inorder | 20, 30, 40, 50, 60, 70, 80 (always ascending for a BST) |
| Preorder | 50, 30, 20, 40, 70, 60, 80 |
| Postorder | 20, 40, 30, 60, 80, 70, 50 |

## 7. Freeing the tree

Destroying a long chain of `unique_ptr`s recursively can overflow the call stack on a skewed tree. `clear()` frees nodes iteratively:

- If the root has a left child, rotate that child up to the root.
- Otherwise, free the root and promote its right child.

This takes O(n) time and O(1) extra space and allocates nothing. The destructor uses it, and a test destroys a 20,000-node chain safely.

## 8. Event recording

Each operation creates a `Recorder` that collects:

- **Events** (`AlgorithmEvent`): the event type, pseudocode line id, primary and secondary node ids and their roles, the compared keys, an empty slot if there is one, the active path, traversal output so far, cumulative metrics, and the action/reason text.
- **Snapshots** (`TreeSnapshot`): pointer-free copies of the tree structure. A new snapshot is taken only when the structure changes, and each event stores the index of its snapshot.

The result is an `OperationRecord` that never points into the live tree. It can be replayed forwards and backwards at any time, including from the history list after the tree has changed. Bulk construction (`rebuild`) runs the same insertion code with recording turned off.
