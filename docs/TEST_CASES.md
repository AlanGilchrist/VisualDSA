# Test Cases and Results

## Execution record

| Item | Value |
|---|---|
| Date executed | 2026-10-21 |
| Platform | Windows 11 (x64) |
| Compiler | MSVC 19.44 (Visual Studio 2022 Build Tools), Release, `/W4 /permissive-` |
| Qt | 6.8.3 (msvc2022_64) |
| Command | `ctest --test-dir build --output-on-failure` |
| Result | **4 / 4 test executables passed: 105 QtTest results, 0 failed, 0 skipped** |

| Executable | QtTest results* | Status |
|---|---|---|
| `tst_binarysearchtree` | 38 | Passed |
| `tst_treeoperations` | 31 | Passed |
| `tst_treeinvariants` | 14 | Passed |
| `tst_visualization` (offscreen GUI) | 22 | Passed |

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

## C. Traversals (`tst_binarysearchtree`)

| ID | Test | Expected | Status |
|---|---|---|---|
| T1 | `inorderSample` | 20, 30, 40, 50, 60, 70, 80 | Pass |
| T2 | `preorderSample` | 50, 30, 20, 40, 70, 60, 80 | Pass |
| T3 | `postorderSample` | 20, 40, 30, 60, 80, 70, 50 | Pass |
| T4 | `traversalsOnEmptyTree` | all three return an empty sequence | Pass |
| T5 | `traversalsOnSingleNode` | all three return [7] | Pass |
| T6 | `traversalEventsEmitInOrder` | output events in postorder; progressive sequence correct; 7 visits | Pass |
| T7 | `inorderIsSortedForArbitraryTree` | inorder output is sorted | Pass |
| T8 | `everyEventLineExistsInListing` | every event of 15 different operations maps to a line in its pseudocode listing and a valid snapshot | Pass |

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

## F. Update (`tst_treeoperations`)

| ID | Test | Input | Expected | Status |
|---|---|---|---|---|
| U1 | `updateValidKey` | 40 → 45 | inorder 20, 30, 45, 50, 60, 70, 80; committed | Pass |
| U2 | `updateMovesKeyToOtherBranch` | 20 → 65 | 65 becomes the right child of 60 | Pass |
| U3 | `updateRootWithTwoChildren` | 50 → 10 | inorder 10, 20, 30, 40, 60, 70, 80 | Pass |
| U4 | `updateSingleNodeTree` | 5 → 6 | root 6 | Pass |
| U5 | `updateMissingOldKeyRejected` | 41 → 42 | Rejected / KeyMissing; no transaction started | Pass |
| U6 | `updateConflictingNewKeyRejected` | 40 → 60 | Rejected / NewKeyExists; tree unchanged | Pass |
| U7 | `updateSameKeyIsNoOp` | 30 → 30 | NoChange / SameKey | Pass |
| U8 | `updateOnEmptyTreeRejected` | 1 → 2 | KeyMissing | Pass |
| U9 | `updateIntermediateStatesAreTentativeAndValid` | 30 → 75 | intermediate events tentative; every snapshot valid; nested lines mapped to update pseudocode | Pass |
| U10 | `failedUpdatePreservesOriginalTree` | three failing updates | ids, keys and links identical | Pass |
| U11 | `updateSequenceKeepsInvariant` | 6 chained updates | valid, size 7 throughout | Pass |

## G. Recording and tree management (`tst_treeoperations`)

| ID | Test | Expected | Status |
|---|---|---|---|
| R1 | `snapshotsAreIndependentOfLiveTree` | a recorded delete still shows the old tree after clear and rebuild | Pass |
| R2 | `metricsAreCumulativeAndMonotonic` | counters never decrease across events | Pass |
| R3 | `rebuildRejectsDuplicatesWithoutChangingTree` | [5, 6, 5] rejected; tree unchanged | Pass |
| R4 | `rebuildReplacesTreeAndKeepsIdsUnique` | new ids never collide with old ones | Pass |
| R5 | `clearTreeEmptiesEverything` | before 7, after 0 | Pass |
| R6 | `moveAssignmentTransfersOwnership` | moved tree valid, size 7 | Pass |

## H. Tree invariants (`tst_treeinvariants`)

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
| N12 | `randomizedOperationsMatchReferenceSet` | 3000 seeded random operations agree with `std::set`; valid after each | Pass |

## I. Visualization and GUI (`tst_visualization`, offscreen platform)

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
| G16 | `invalidInputDoesNotTouchTree` | 5 invalid inputs show errors and change nothing; `45` then inserts | Pass |
| G17 | `duplicateTreeInputIsRejected` | `10, 20, 10` rejected with the uniqueness message; tree unchanged | Pass |
| G18 | `resetRestoresSampleTree` | after changes and a clear, Reset gives preorder 50, 30, 20, 40, 70, 60, 80 | Pass |
| G19 | `historyReplayShowsRecordedState` | a replayed insert shows 8 nodes while the live tree is empty | Pass |
| G20 | `closingWhilePlayingIsSafe` | deleting the window mid-playback does not crash | Pass |

## J. Manual checks

The real window was launched and captured at each step of the demonstration script with `tools/capture_screenshots` (see [`screenshots/`](../screenshots)). These captures were inspected visually at 1440×900: layout, highlight colours, comparison badges, ghost slots, the working-copy banner and the traversal output strip. Two layout defects found this way were fixed:

- The right panel overflowed when a long step header appeared.
- The operation inputs had unused vertical space above them.

Behaviour at other window sizes was not exhaustively tested. The minimum window size is 1120×720.
