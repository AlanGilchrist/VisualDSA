# Viva Questions and Answers

## BST fundamentals

**1. What is a Binary Search Tree?**
A binary tree in which, for every node, all keys in the left subtree are smaller than the node's key and all keys in the right subtree are larger.

**2. Is it enough to check that each left child is smaller than its parent?**
No. The rule applies to the *whole* subtree. For example, 55 as the right child of 30 inside 50's left subtree satisfies the local check but breaks the BST. Our validator passes an allowed interval (lower, upper) down the tree to catch this.

**3. How does your project handle duplicate keys?**
It rejects them. An insertion that meets an equal key stops with "Key already exists." and the tree is unchanged. Building a tree from a list that contains a duplicate is rejected before anything changes.

**4. Define the height of a tree. What is the height of the sample tree?**
Height is the number of edges on the longest root-to-leaf path. Under this convention an empty tree has height −1 and a single node has height 0. The sample tree `50, 30, 70, 20, 40, 60, 80` has height 2.

**5. What determines the shape of a BST?**
The order in which keys are inserted. The same keys inserted in sorted order produce a chain (a skewed tree) of height n − 1.

## Search

**6. Explain BST search and its complexity.**
Compare the target with the current node. If they are equal, the key is found. If the target is smaller, go left; otherwise go right. Stop at null. Each step moves one level down, so the cost is O(h): O(log n) on average and O(n) in the worst case.

**7. Recursive vs. iterative search: which did you use and why?**
Iterative. It uses O(1) extra space instead of O(h) stack frames, and its loop maps naturally onto the recorded comparison steps.

**8. How does the program prove that a key is absent?**
The search reaches a null pointer. If the key existed, it could only be in that slot. The canvas shows this slot as a red dashed `null` circle.

## Insertion

**9. Where is a new key inserted, and why there?**
At the first empty child pointer on its search path. That is the only position where every ancestor's left/right rule still holds. A new key therefore always becomes a leaf.

**10. What is the best-case cost of insertion?**
O(1): inserting into an empty tree, or into an empty child slot of the root.

## Deletion

**11. What are the three deletion cases?**
- Leaf: clear the parent's pointer.
- One child: link the parent directly to that child.
- Two children: replace the node's key with its inorder successor, then remove the successor.

**12. What is the inorder successor, and how do you find it?**
It is the smallest key larger than the target. For a node with a right subtree, it is the leftmost node of that subtree: step right once, then go left until there is no left child.

**13. Why does the successor always have at most one child?**
It is the leftmost node of a subtree, so it has no left child. Removing it is therefore case 1 or case 2, never case 3 again.

**14. Could you use the inorder predecessor instead?**
Yes. The maximum of the left subtree works symmetrically. We use the successor consistently.

**15. Why do you unlink the successor before copying its key?**
Copying first briefly leaves the same key in the tree twice, which is an invalid BST. Unlinking first gives the same final tree, and every intermediate state shown on screen stays valid. A test checks this.

**16. How do you delete the root without a special case?**
The code holds a pointer to the *owning link*, `std::unique_ptr<BSTNode>*`, rather than to the parent node. For the root, that link is the tree's `root_` member, so all cases are handled the same way.

## Update

**17. Why can't you simply overwrite a key in a BST?**
A key's position depends on its value. Writing 65 into the node holding 40 would put 65 in the left subtree of 50. An overwrite is safe only if the new key lies strictly between the old key's inorder predecessor and successor.

**18. How is your update made atomic?**
It runs three checks first: the old key exists, the new key is absent, and the keys differ. It then deletes the old key and inserts the new one on a cloned working copy. The copy replaces the live tree only after the invariant check passes; otherwise it is discarded. The live tree is never left half-updated.

**19. What does that atomicity cost?**
O(n) extra time and space for the copy. A plain delete followed by insert costs O(h) time and O(1) extra space, but could leave the tree half-changed if the second step failed.

## Traversals

**20. State the three depth-first traversal orders.**
- Inorder: Left, Root, Right.
- Preorder: Root, Left, Right.
- Postorder: Left, Right, Root.

**21. What special property does inorder have for a BST?**
It outputs the keys in ascending order. That is also why our layout, which places nodes by inorder rank, shows the keys sorted from left to right.

**22. Give a practical use of preorder and of postorder.**
- Preorder: re-inserting keys in preorder rebuilds the same tree, which is useful for serialization.
- Postorder: freeing or evaluating children before their parent, as in deleting a tree or evaluating an expression tree.

**23. What is the time and space complexity of a traversal?**
Time is Θ(n), since every node is visited once. The recursion stack uses O(h) space: O(log n) when balanced, O(n) when skewed. Storing the output takes another O(n).

## Complexity

**24. Why is BST search not guaranteed O(log n)?**
A plain BST does not rebalance itself. Sorted insertions create a chain of height n − 1, so search, insert and delete become O(n). Self-balancing trees such as AVL and red-black trees guarantee O(log n).

**25. What is the cost of building a tree by n insertions?**
- O(n log n) on average.
- O(n²) in the worst case, for sorted input: 0 + 1 + … + (n − 1).

**26. Is the animation time the algorithm's running time?**
No. Playback speed only changes how fast the recorded steps are shown. The Complexity panel separates theoretical bounds from counters measured by the C++ code (nodes visited, comparisons, structural changes), and those counters do not depend on speed.

## C++ memory management

**27. How is node memory managed?**
Each node owns its children through `std::unique_ptr`, and the tree owns the root. Every node has exactly one owner, is freed automatically and only once, and there are no manual `delete` calls (RAII).

**28. Why does your destructor not simply rely on recursive `unique_ptr` destruction?**
In a skewed tree with thousands of nodes, recursive destruction can overflow the call stack. `clear()` instead repeatedly rotates the left child up to the root and frees roots that have no left child. This takes O(n) time, O(1) space and allocates nothing. A test destroys a 20,000-node chain safely.

**29. How does one-child deletion avoid a double free?**
`*link = std::move(child)` is a move-assignment. It first releases the child from the doomed node, then destroys the doomed node, whose child pointer is now empty. The child subtree is never freed.

## Architecture, visualization and testing

**30. How is the project structured?**
In four layers:
- A **core** BST engine in pure C++ with no Qt.
- A **visualization** layer: layout, the QPainter canvas, and the playback controller.
- A **presentation** layer: the main window and panels.
- **Tests and documentation.**

The GUI never mutates the tree directly. It calls a tree operation and replays the record it gets back.

**31. How do you ensure animations reflect the real algorithm?**
While each operation runs, it records typed `AlgorithmEvent`s: the compared keys, path, pseudocode line id, metrics and explanation text. It also records pointer-free `TreeSnapshot`s. The UI only replays these, so the visualization cannot show something the algorithm did not do.

**32. Why store snapshots by node id instead of pointers?**
Pointers become invalid when nodes are deleted. Id-based snapshots stay valid forever, which allows stepping backwards, replaying history after the tree has changed, and animating nodes smoothly by id.

**33. How is playback controlled safely?**
A single `QTimer` inside `AnimationController` advances steps. Loading a new operation stops it first, manual stepping pauses it, and the controller stops it on destruction. This avoids overlapping `singleShot` chains, and tests cover pause, resume, the boundaries and cancellation.

**34. How did you test the project?**
With four Qt Test suites (105 results, all passing):
- Insertion, search, traversals and input parsing.
- All deletion cases and update scenarios, including rollback.
- Invariant tests, including 3000 random operations compared against `std::set`.
- Offscreen GUI tests: layout overlap, canvas state, playback, and step-by-step sync between events, pseudocode and explanations.
