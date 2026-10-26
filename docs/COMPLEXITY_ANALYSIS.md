# Complexity Analysis

Notation:

- **n**: the number of nodes.
- **h**: the height of the tree, counted in edges.
- **w**: the width of the tree, the largest number of nodes on one level.

A plain BST does **not** rebalance itself:

- For a balanced-shaped tree, h ≈ ⌊log₂ n⌋.
- For a skewed tree, h = n − 1. Inserting keys in sorted order produces one.

Every bound below that depends on the tree's shape is O(h). "O(log n)" is the *average* over random insertion orders, never a guarantee.

## Summary

| Operation | Best | Average | Worst | Auxiliary space (algorithm) |
|---|---|---|---|---|
| Search | O(1) | O(log n) | O(n) | O(1), iterative |
| Insertion | O(1) | O(log n) | O(n) | O(1), iterative |
| Deletion | O(1) | O(log n) | O(n) | O(1), iterative |
| Inorder / preorder / postorder (DFS) | O(n) | O(n) | O(n) | O(h) recursion stack + O(n) stored output |
| Level order (BFS) | O(n) | O(n) | O(n) | O(w) queue + O(n) stored output |
| Build from n keys | O(n log n) | O(n log n) | O(n²) | O(n) for the nodes |
| Clear | O(n) | O(n) | O(n) | O(1) (rotation-based) |

## Search

- **Best case:** the key is at the root, so one comparison.
- **Average and worst case:** every comparison moves one level down, so at most h + 1 nodes are examined, giving O(h). For random insertion orders the expected height is O(log n). For sorted insertion orders, h = n − 1.
- **Space:** the loop keeps only one pointer, so O(1). A recursive version would use O(h) stack.

## Insertion

Insertion is a search for the key followed by one pointer assignment: O(h) + O(1) = O(h). The best case is O(1): an empty tree, or a slot directly under the root.

## Deletion

Deletion has three parts:

1. Locating the node: O(h).
2. Case 3 only: walking to the inorder successor. This continues downward from the target, so the total path is still at most h + 1 nodes.
3. Re-linking: O(1). Case 3 performs two writes: unlinking the successor and copying its key.

The total is O(h).

## Depth-first traversals

Each node is entered once and output once, so the time is Θ(n) for every shape. The recursion depth equals the height:

- O(log n) for a balanced-shaped tree.
- O(n) for a skewed tree.

Storing the output takes another O(n).

## Breadth-first traversal (level order)

Each node is enqueued once and dequeued once, so the time is also Θ(n) for every shape. The extra space is the queue, which holds at most about one level at a time: O(w).

- **Full or balanced-shaped tree:** the last level holds about n/2 nodes, so the queue needs O(n). The sample tree peaks at 4 waiting nodes, its four leaves.
- **Skewed tree:** every level has one node, so the queue never holds more than 1: O(1).

This is the mirror image of the depth-first traversals. Their stack grows with the height, so they are cheap on wide, shallow trees and expensive on deep, skewed ones. BFS is cheap on deep, narrow trees and expensive on wide ones.

## Building a tree

Inserting n keys one at a time costs Σ O(hᵢ):

- **Random order:** O(n log n).
- **Sorted order:** 0 + 1 + … + (n − 1) = O(n²).

## Measured counters vs. theory

The Complexity panel shows two kinds of numbers:

- **Theoretical** bounds, from the table above.
- **Measured** counters, recorded by the C++ code as it runs:

| Counter | Meaning |
|---|---|
| Nodes visited | Nodes the algorithm stepped onto, including the successor search |
| Key comparisons | Three-way key comparisons, one per node on a search path |
| Traversal outputs | Keys appended to a traversal's output |
| Structural changes | Pointer re-links and key writes on the tree |

Examples on the sample tree, all verified by the automated tests:

| Operation | Nodes visited | Comparisons | Structural changes |
|---|---|---|---|
| Search 60 | 3 | 3 | 0 |
| Insert 45 | 3 | 3 | 1 |
| Delete 30 (two children) | 3 | 2 | 2 |
| Postorder traversal | 7 | 0 | 0, plus 7 outputs |
| Level-order traversal (BFS) | 7 | 0 | 0, plus 7 outputs |

For delete 30: nodes 50 and 30 are reached by comparison, and 40 is reached during the successor search. Both are counted as visited, but only the first two involved key comparisons.

Animation duration is **not** time complexity. Playback speed changes how fast steps are shown and never changes the counters.

## Recording overhead (not part of the algorithm)

To make playback possible, each structural change stores a snapshot, which costs O(n), and each event stores the active path, which costs O(h). This overhead exists only to support visualization. It is bounded by keeping trees at 64 nodes or fewer in the GUI, and turned off entirely when a whole tree is built at once.
