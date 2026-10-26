# Test Cases and Results

## Execution record

| Item | Value |
|---|---|
| Date executed | 2026-10-26 |
| Platform | Windows 11 (x64) |
| Compiler | MSVC 19.44 (Visual Studio 2022 Build Tools), Release, `/W4 /permissive-` |
| Qt | 6.8.3 (msvc2022_64) |
| Command | `ctest --test-dir build --output-on-failure` |
| Result | **4 / 4 test executables passed: 99 QtTest results, 0 failed, 0 skipped** |

| Executable | QtTest results* | Status |
|---|---|---|
| `tst_binarysearchtree` | 42 | Passed |
| `tst_treeoperations` | 20 | Passed |
| `tst_treeinvariants` | 14 | Passed |
| `tst_visualization` (offscreen GUI) | 23 | Passed |

\* QtTest counts `initTestCase`/`cleanupTestCase` and each data row of data-driven tests as separate results.

The build produced no compiler warnings at `/W4`. These results were produced by actually running the suites. Re-run the command above to reproduce them on your machine, and update this table if your environment differs.

The sample tree used throughout is built by inserting `50, 30, 70, 20, 40, 60, 80`.

---

## A. Insertion (`tst_binarysearchtree`)

| ID | Test | Input | Expected result | Actual | Status |
|---|---|---|---|---|---|
| I1 | `insertIntoEmptyTree` | insert 50 into empty tree | root = 50, size 1, height 0, valid | as expected | Pass |
| I2 | `insertSmallerGoesLeft` | 50, then 30 | 30 is the left child | as expected | Pass |
| I3 | `insertLargerGoesRight` | 50, then 70 | 70 is the right child | as expected | Pass |
| I4 | `insertSampleTreeShape` | sample keys | shape matches spec; size 7; height 2 | as expected | Pass |
| I5 | `insertDuplicateRejectedTreeUnchanged` | insert 40 into sample | Rejected / DuplicateKey, "Key already exists.", tree identical | as expected | Pass |
| I6 | `insertKeepsInvariantForManyKeys` | 14 mixed keys incl. negatives | valid after every insert | as expected | Pass |
| I7 | `insertEventsDescribeThePath` | insert 45 into sample | compares 50, 30, 40; moves L, R, R; empty right slot of 40; 3 comparisons, 1 structural change | as expected | Pass |
| I8 | `insertRespectsCapacity` | capacity 2, insert 3 keys | third rejected (CapacityReached) | as expected | Pass |

## B. Search (`tst_binarysearchtree`)

| ID | Test | Input | Expected | Actual | Status |
|---|---|---|---|---|---|
| S1 | `searchFindsRoot` | 50 | found, 1 node visited | as expected | Pass |
| S2 | `searchFindsLeaf` | 60 | found, path 50→70→60, 3 comparisons | as expected | Pass |
| S3 | `searchFindsDeepNode` | 44 (after adding 45, 43, 44) | found after 6 nodes | as expected | Pass |
| S4 | `searchMissingKey` | 65 | NotFound; ended at right slot of 60 | as expected | Pass |
| S5 | `searchEmptyTree` | 10 on empty tree | NotFound, 1 event, 0 visits | as expected | Pass |
| S6 | `searchDoesNotModifyTree` | 40, 41 | tree unchanged | as expected | Pass |

## C. Traversals: depth-first and breadth-first (`tst_binarysearchtree`)

| ID | Test | Expected | Status |
|---|---|---|---|
| T1 | `inorderSample` | 20, 30, 40, 50, 60, 70, 80 | Pass |
| T2 | `preorderSample` | 50, 30, 20, 40, 70, 60, 80 | Pass |
| T3 | `postorderSample` | 20, 40, 30, 60, 80, 70, 50 | Pass |
| T4 | `traversalsOnEmptyTree` | all four orders return an empty sequence | Pass |
| T5 | `traversalsOnSingleNode` | all four orders return [7] | Pass |
| T6 | `traversalEventsEmitInOrder` | output events in postorder; progressive sequence correct; 7 visits | Pass |
| T7 | `inorderIsSortedForArbitraryTree` | inorder output is sorted | Pass |
| T8 | `everyEventLineExistsInListing` | every event of 15 different operations (including BFS on a full and an empty tree) maps to a line in its pseudocode listing and a valid snapshot | Pass |
| T9 | `levelOrderSample` | BFS outputs 50, 30, 70, 20, 40, 60, 80 | Pass |
| T10 | `levelOrderOfSkewedTree` | chain 1..5 outputs 1, 2, 3, 4, 5; the queue never holds more than 1 node | Pass |
| T11 | `levelOrderEmptyTree` | a single event on the `if root = null` line; empty output | Pass |
| T12 | `levelOrderQueueIsFirstInFirstOut` | each dequeued node was at the front; each enqueued child joins the back and is a child of the node just dequeued; depths never decrease; 7 dequeues, 6 enqueues, widest queue 4, empty at the end; final reason `[50] [30, 70] [20, 40, 60, 80]` | Pass |

## D. Input validation (`tst_binarysearchtree`)

| ID | Test | Input | Expected | Status |
|---|---|---|---|---|
| V1 | `parseKeyValid` | `42`, ` -17 `, `+5`, `0` | parsed values | Pass |
| V2 | `parseKeyErrors` (11 rows) | `""`, `"   "` | Empty | Pass |
| | | `abc`, `4.5`, `-`, `4 5` | InvalidInteger | Pass |
| | | `2147483648`, `999…9` (23 digits), `-2147483649` | Overflow | Pass |
| | | `10000`, `2147483647` | OutOfRange | Pass |
| V3 | `parseKeyListValid` | `50, 30,70 20  40,` | [50, 30, 70, 20, 40] | Pass |
| V4 | `parseKeyListErrors` | empty; `1,,2`; `1, x`; `1, 30, 1`; overflow; too many | correct error for each; no keys returned on error | Pass |

## E. Deletion (`tst_treeoperations`)

| ID | Test | Input | Expected | Status |
|---|---|---|---|---|
| D1 | `deleteLeaf` | delete 20 | case Leaf; preorder 50, 30, 40, 70, 60, 80 | Pass |
| D2 | `deleteNodeWithOnlyLeftChild` | tree 50, 30, 70, 20; delete 30 | case OneChild; 20 replaces 30 | Pass |
| D3 | `deleteNodeWithOnlyRightChild` | tree 50, 30, 70, 80; delete 70 | case OneChild; 80 replaces 70 | Pass |
| D4 | `deleteNodeWithTwoChildren` | delete 30 | successor 40; preorder 50, 40, 20, 70, 60, 80; 2 structural changes, 2 comparisons, 3 nodes visited | Pass |
| D5 | `deleteTwoChildrenWalksLeftToSuccessor` | tree 50, 30, 70, 60, 80, 55, 57; delete 50 | successor path 70→60→55; 57 takes 55's place | Pass |
| D6 | `deleteTwoChildrenNeverShowsInvalidTree` | delete 50 | 3 snapshots, all valid BSTs; unlink happens before key copy | Pass |
| D7 | `deleteRoot` | delete 50 | new root key 60 | Pass |
| D8 | `deleteRootWithOneChild` | tree 10, 20, 30; delete 10 | root 20, height 1 | Pass |
| D9 | `deleteOnlyNode` | tree 42; delete 42 | empty tree, height −1 | Pass |
| D10 | `deleteMissingKeyLeavesTreeUnchanged` | delete 65 | NotFound, tree unchanged | Pass |
| D11 | `deleteFromEmptyTree` | delete 1 | NotFound | Pass |
| D12 | `deleteEveryKeyInEveryOrder` | 4 orders × 7 deletions | valid after every step, empty at the end | Pass |

## F. Recording and tree management (`tst_treeoperations`)

| ID | Test | Expected | Status |
|---|---|---|---|
| R1 | `snapshotsAreIndependentOfLiveTree` | a recorded delete still shows the old tree after clear and rebuild | Pass |
| R2 | `metricsAreCumulativeAndMonotonic` | counters never decrease across the events of delete 50; 2 structural changes | Pass |
| R3 | `rebuildRejectsDuplicatesWithoutChangingTree` | [5, 6, 5] rejected; tree unchanged | Pass |
| R4 | `rebuildReplacesTreeAndKeepsIdsUnique` | new ids never collide with old ones | Pass |
| R5 | `clearTreeEmptiesEverything` | before 7, after 0 | Pass |
| R6 | `moveAssignmentTransfersOwnership` | moved tree valid, size 7 | Pass |

## G. Tree invariants (`tst_treeinvariants`)

| ID | Test | Expected | Status |
|---|---|---|---|
| N1 | `validatorAcceptsEmptyTree` | valid, count 0, height −1 | Pass |
| N2 | `validatorAcceptsValidTree` | valid, count 5, height 2 | Pass |
| N3 | `validatorRejectsWrongChildSide` | 60 left of 50 rejected | Pass |
| N4 | `validatorChecksWholeSubtreeNotJustParent` | 55 under 30 in 50's left subtree rejected | Pass |
| N5 | `validatorRejectsDuplicateKeys` | rejected | Pass |
| N6 | `validatorRejectsDuplicateIds` | rejected (duplicate ownership/identity) | Pass |
| N7 | `sizeAndHeightAreCorrect` | balanced height 2; sorted 1..10 height 9 | Pass |
| N8 | `snapshotMatchesLiveTree` | snapshot traversals, height, edges and paths match | Pass |
| N9 | `validateSnapshotDetectsInconsistency` | broken parent link detected | Pass |
| N10 | `cloneIsDeepAndPreservesIds` | changing the clone leaves the original intact | Pass |
| N11 | `deepSkewedTreeIsDestroyedSafely` | 20,000-node chain cleared and destroyed without stack overflow | Pass |
| N12 | `randomizedOperationsMatchReferenceSet` | 3000 seeded random inserts, deletes and searches agree with `std::set`; valid after each | Pass |

## H. Visualization and GUI (`tst_visualization`, offscreen platform)

| ID | Test | Expected | Status |
|---|---|---|---|
| G1 | `layoutHasNoOverlapAndFollowsInorder` | no two nodes closer than 2r; x order = key order; children on the correct side, below their parent | Pass |
| G2 | `layoutOfEmptyTree` | no positions | Pass |
| G3 | `canvasRendersSampleTree` | 7 nodes, 6 edges, renders | Pass |
| G4 | `canvasRendersEmptyTree` | 0 nodes, renders | Pass |
| G5 | `canvasHighlightsEventNode` | highlight equals event node at every step of search 60 | Pass |
| G6 | `canvasAnimatesStructuralChange` | transition runs, then 8 nodes | Pass |
| G7 | `canvasZoomIsClampedAndFitRestoresAutoFit` | zoom clamped to [0.25, 3.0]; Fit re-enables auto-fit | Pass |
| G8 | `canvasClickSelectsNode` | clicking node 40 selects it and emits a signal | Pass |
| G9 | `controllerBoundaries` | previous/next/jump clamp at both ends; no-op without a record | Pass |
| G10 | `controllerPauseAndResume` | no progress while paused; step works while paused; resume continues | Pass |
| G11 | `controllerReachesConsistentEnd` | finished emitted once; at end, not playing; play restarts; reset returns to step 0 | Pass |
| G12 | `controllerLoadCancelsPreviousPlayback` | loading a new record stops the old one | Pass |
| G13 | `controllerSpeedChangesInterval` | 0.5× → 2000 ms, 2× → 500 ms | Pass |
| G14 | `mainWindowStartsWithSampleTree` | 7 nodes, 1 history entry | Pass |
| G15 | `eventPseudocodeAndExplanationStayInSync` | at every step of delete 30: pseudocode line, explanation text, canvas highlight, node count and measured counter match the event | Pass |
| G16 | `levelOrderStepsStayInSync` | BFS from the BFS module: 22 steps; at every step the pseudocode line, explanation and canvas queue match the event; the tree is unchanged | Pass |
| G17 | `invalidInputDoesNotTouchTree` | 5 invalid inputs show errors and change nothing; `45` then inserts | Pass |
| G18 | `duplicateTreeInputIsRejected` | `10, 20, 10` rejected with the uniqueness message; tree unchanged | Pass |
| G19 | `resetRestoresSampleTree` | after an insert, a delete, a BFS and a clear, Reset gives preorder 50, 30, 20, 40, 70, 60, 80 | Pass |
| G20 | `historyReplayShowsRecordedState` | a replayed insert shows 8 nodes while the live tree is empty | Pass |
| G21 | `closingWhilePlayingIsSafe` | deleting the window mid-playback does not crash | Pass |

## I. Manual checks

The real window was launched and captured at each step of the demonstration script with `tools/capture_screenshots` (see [`screenshots/`](../screenshots)). These captures were inspected visually at 1440×900: layout, highlight colours, comparison badges, ghost slots, the BFS queue rings and QUEUE strip, and the traversal output strip. Two layout defects found this way were fixed:

- The right panel overflowed when a long step header appeared.
- The operation inputs had unused vertical space above them.

Behaviour at other window sizes was not exhaustively tested. The minimum window size is 1120×720.

## J. Web version

The browser version in `web/` has its own suite, ported from the cases above: `node --test web/tests/core.test.js`, with **34 tests, all passing** (Node.js 24). It covers insertion, search, all deletion cases, the depth-first and breadth-first traversals (including the FIFO queue checks of T12), snapshots, the validator, 2000 seeded random operations and input parsing.

The page itself was driven in headless Chrome at 1440×900, 1100×800 and phone width (390×844). The checks covered every module, BFS stepping (the queue matches the desktop app at every step), the absence of horizontal scrolling and an empty browser console.
