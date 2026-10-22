// Deletion and update tests, plus checks on the recorded event stream.

#include "BinarySearchTree.h"

#include <QtTest>

#include <algorithm>

using namespace vdsa;

namespace {

BinarySearchTree treeOf(std::initializer_list<int> keys)
{
    BinarySearchTree tree;
    for (int key : keys)
        tree.insert(key);
    return tree;
}

BinarySearchTree sampleTree()
{
    BinarySearchTree tree;
    for (int key : BinarySearchTree::sampleKeys())
        tree.insert(key);
    return tree;
}

QList<int> toList(const std::vector<int>& v) { return QList<int>(v.begin(), v.end()); }

QList<int> preorderOf(const BinarySearchTree& tree) { return toList(tree.snapshot().preorderKeys()); }

bool hasEvent(const OperationRecord& rec, EventType type)
{
    return std::any_of(rec.events.begin(), rec.events.end(), [type](const AlgorithmEvent& e) { return e.type == type; });
}

const AlgorithmEvent& firstEvent(const OperationRecord& rec, EventType type)
{
    return *std::find_if(rec.events.begin(), rec.events.end(), [type](const AlgorithmEvent& e) { return e.type == type; });
}

} // namespace

class TestTreeOperations : public QObject {
    Q_OBJECT

private slots:
    // --- Deletion ------------------------------------------------------------
    void deleteLeaf()
    {
        BinarySearchTree tree = sampleTree();
        const OperationRecord rec = tree.remove(20);
        QCOMPARE(rec.result.status, ResultStatus::Success);
        QCOMPARE(rec.result.deletionCase, DeletionCase::Leaf);
        QCOMPARE(preorderOf(tree), (QList<int>{50, 30, 40, 70, 60, 80}));
        QVERIFY(!tree.root()->left->left);
        QVERIFY(tree.validate().valid);
        QCOMPARE(rec.result.metrics.structuralChanges, 1);
        QCOMPARE(firstEvent(rec, EventType::DeletionCaseIdentified).line, PseudoLine::DelCaseLeaf);
    }

    void deleteNodeWithOnlyLeftChild()
    {
        BinarySearchTree tree = treeOf({50, 30, 70, 20}); // 30 has only a left child (20)
        const OperationRecord rec = tree.remove(30);
        QCOMPARE(rec.result.deletionCase, DeletionCase::OneChild);
        QCOMPARE(tree.root()->left->key, 20);
        QCOMPARE(preorderOf(tree), (QList<int>{50, 20, 70}));
        QVERIFY(tree.validate().valid);
        const AlgorithmEvent& c = firstEvent(rec, EventType::DeletionCaseIdentified);
        QCOMPARE(c.secondaryRole, SecondaryRole::OnlyChild);
        QCOMPARE(rec.before().find(c.secondaryId)->key, 20);
    }

    void deleteNodeWithOnlyRightChild()
    {
        BinarySearchTree tree = treeOf({50, 30, 70, 80}); // 70 has only a right child (80)
        const OperationRecord rec = tree.remove(70);
        QCOMPARE(rec.result.deletionCase, DeletionCase::OneChild);
        QCOMPARE(tree.root()->right->key, 80);
        QCOMPARE(preorderOf(tree), (QList<int>{50, 30, 80}));
        QVERIFY(tree.validate().valid);
    }

    void deleteNodeWithTwoChildren()
    {
        BinarySearchTree tree = sampleTree();
        const OperationRecord rec = tree.remove(30); // successor is 40
        QCOMPARE(rec.result.deletionCase, DeletionCase::TwoChildren);
        QCOMPARE(tree.root()->left->key, 40);
        QCOMPARE(preorderOf(tree), (QList<int>{50, 40, 20, 70, 60, 80}));
        QVERIFY(tree.validate().valid);

        const AlgorithmEvent& succ = firstEvent(rec, EventType::SuccessorFound);
        QCOMPARE(succ.key, 40);
        QVERIFY(hasEvent(rec, EventType::SuccessorSearchStarted));
        QVERIFY(hasEvent(rec, EventType::KeyUpdated));
        QCOMPARE(rec.result.metrics.structuralChanges, 2); // key copy + unlink
        QCOMPARE(rec.result.metrics.keyComparisons, 2);    // 50, 30
        QCOMPARE(rec.result.metrics.nodesVisited, 3);      // 50, 30 + successor 40
    }

    void deleteTwoChildrenWalksLeftToSuccessor()
    {
        // Successor of 50 is 55: right subtree 70 -> 60 -> 55, and 55 has a right child 57.
        BinarySearchTree tree = treeOf({50, 30, 70, 60, 80, 55, 57});
        const OperationRecord rec = tree.remove(50);
        QCOMPARE(tree.root()->key, 55);
        QCOMPARE(tree.root()->right->left->left->key, 57); // 57 moved into 55's old slot
        QVERIFY(tree.validate().valid);

        QList<int> successorPath;
        for (const AlgorithmEvent& e : rec.events)
            if (e.type == EventType::SuccessorSearchStarted || e.type == EventType::NodeVisited)
                successorPath << e.key;
        QCOMPARE(successorPath, (QList<int>{70, 60, 55}));
        const AlgorithmEvent& removed = firstEvent(rec, EventType::NodeRemoved);
        QCOMPARE(removed.key, 55);
        QCOMPARE(removed.secondaryRole, SecondaryRole::OnlyChild);
        QCOMPARE(firstEvent(rec, EventType::KeyUpdated).key, 55);
    }

    void deleteTwoChildrenNeverShowsInvalidTree()
    {
        // The successor is unlinked before its key is written into the target,
        // so every recorded intermediate state is a valid BST.
        BinarySearchTree tree = sampleTree();
        const OperationRecord rec = tree.remove(50);
        QCOMPARE(rec.snapshots.size(), std::size_t(3)); // before, after unlink, after key write
        for (const TreeSnapshot& s : rec.snapshots)
            QVERIFY2(validateSnapshot(s).valid, validateSnapshot(s).error.c_str());
        const auto removedAt = std::find_if(rec.events.begin(), rec.events.end(),
                                            [](const AlgorithmEvent& e) { return e.type == EventType::NodeRemoved; });
        const auto copiedAt = std::find_if(rec.events.begin(), rec.events.end(),
                                           [](const AlgorithmEvent& e) { return e.type == EventType::KeyUpdated; });
        QVERIFY(removedAt < copiedAt);
    }

    void deleteRoot()
    {
        BinarySearchTree tree = sampleTree();
        QVERIFY(tree.remove(50).result.succeeded());
        QCOMPARE(tree.root()->key, 60);
        QCOMPARE(toList(tree.inorder()), (QList<int>{20, 30, 40, 60, 70, 80}));
        QVERIFY(tree.validate().valid);
    }

    void deleteRootWithOneChild()
    {
        BinarySearchTree tree = treeOf({10, 20, 30});
        QVERIFY(tree.remove(10).result.succeeded());
        QCOMPARE(tree.root()->key, 20);
        QCOMPARE(tree.height(), 1);
    }

    void deleteOnlyNode()
    {
        BinarySearchTree tree = treeOf({42});
        const OperationRecord rec = tree.remove(42);
        QCOMPARE(rec.result.deletionCase, DeletionCase::Leaf);
        QVERIFY(tree.empty());
        QVERIFY(!tree.root());
        QCOMPARE(tree.height(), -1);
        QVERIFY(rec.after().empty());
        QVERIFY(tree.validate().valid);
    }

    void deleteMissingKeyLeavesTreeUnchanged()
    {
        BinarySearchTree tree = sampleTree();
        const QList<int> before = preorderOf(tree);
        const OperationRecord rec = tree.remove(65);
        QCOMPARE(rec.result.status, ResultStatus::NotFound);
        QVERIFY(!rec.result.treeChanged);
        QCOMPARE(preorderOf(tree), before);
        QCOMPARE(rec.snapshots.size(), std::size_t(1));
        QCOMPARE(rec.events.back().type, EventType::NodeNotFound);
    }

    void deleteFromEmptyTree()
    {
        BinarySearchTree tree;
        QCOMPARE(tree.remove(1).result.status, ResultStatus::NotFound);
        QVERIFY(tree.empty());
    }

    void deleteEveryKeyInEveryOrder()
    {
        // Delete all keys of the sample tree in several orders, validating each step.
        const std::vector<std::vector<int>> orders = {
            {50, 30, 70, 20, 40, 60, 80}, {20, 40, 60, 80, 30, 70, 50}, {80, 70, 60, 50, 40, 30, 20},
            {40, 50, 20, 70, 30, 80, 60}};
        for (const auto& order : orders) {
            BinarySearchTree tree = sampleTree();
            std::size_t expected = 7;
            for (int key : order) {
                QVERIFY(tree.remove(key).result.succeeded());
                QCOMPARE(tree.size(), --expected);
                QVERIFY2(tree.validate().valid, tree.validate().error.c_str());
                QVERIFY(!tree.contains(key));
            }
            QVERIFY(tree.empty());
        }
    }

    // --- Update --------------------------------------------------------------
    void updateValidKey()
    {
        BinarySearchTree tree = sampleTree();
        const OperationRecord rec = tree.update(40, 45);
        QCOMPARE(rec.result.status, ResultStatus::Success);
        QVERIFY(!tree.contains(40));
        QVERIFY(tree.contains(45));
        QCOMPARE(tree.size(), std::size_t(7));
        QCOMPARE(toList(tree.inorder()), (QList<int>{20, 30, 45, 50, 60, 70, 80}));
        QVERIFY(tree.validate().valid);
        QCOMPARE(rec.events.back().type, EventType::TransactionCommitted);
    }

    void updateMovesKeyToOtherBranch()
    {
        BinarySearchTree tree = sampleTree();
        QVERIFY(tree.update(20, 65).result.succeeded());
        QVERIFY(!tree.contains(20));
        // 65 belongs in the right subtree: 50 -> 70 -> 60 -> right.
        QCOMPARE(tree.root()->right->left->right->key, 65);
        QCOMPARE(toList(tree.inorder()), (QList<int>{30, 40, 50, 60, 65, 70, 80}));
        QVERIFY(tree.validate().valid);
    }

    void updateRootWithTwoChildren()
    {
        BinarySearchTree tree = sampleTree();
        QVERIFY(tree.update(50, 10).result.succeeded());
        QCOMPARE(toList(tree.inorder()), (QList<int>{10, 20, 30, 40, 60, 70, 80}));
        QVERIFY(tree.validate().valid);
    }

    void updateSingleNodeTree()
    {
        BinarySearchTree tree = treeOf({5});
        QVERIFY(tree.update(5, 6).result.succeeded());
        QCOMPARE(tree.root()->key, 6);
        QCOMPARE(tree.size(), std::size_t(1));
    }

    void updateMissingOldKeyRejected()
    {
        BinarySearchTree tree = sampleTree();
        const QList<int> before = preorderOf(tree);
        const OperationRecord rec = tree.update(41, 42);
        QCOMPARE(rec.result.status, ResultStatus::Rejected);
        QCOMPARE(rec.result.code, ResultCode::KeyMissing);
        QCOMPARE(preorderOf(tree), before);
        QVERIFY(!hasEvent(rec, EventType::TransactionStarted));
    }

    void updateConflictingNewKeyRejected()
    {
        BinarySearchTree tree = sampleTree();
        const QList<int> before = preorderOf(tree);
        const OperationRecord rec = tree.update(40, 60);
        QCOMPARE(rec.result.status, ResultStatus::Rejected);
        QCOMPARE(rec.result.code, ResultCode::NewKeyExists);
        QCOMPARE(QString::fromStdString(rec.result.message), QString("Update rejected: the new key already exists."));
        QCOMPARE(preorderOf(tree), before);
        QVERIFY(!rec.result.treeChanged);
    }

    void updateSameKeyIsNoOp()
    {
        BinarySearchTree tree = sampleTree();
        const QList<int> before = preorderOf(tree);
        const OperationRecord rec = tree.update(30, 30);
        QCOMPARE(rec.result.status, ResultStatus::NoChange);
        QCOMPARE(rec.result.code, ResultCode::SameKey);
        QCOMPARE(preorderOf(tree), before);
    }

    void updateOnEmptyTreeRejected()
    {
        BinarySearchTree tree;
        QCOMPARE(tree.update(1, 2).result.code, ResultCode::KeyMissing);
        QVERIFY(tree.empty());
    }

    void updateIntermediateStatesAreTentativeAndValid()
    {
        BinarySearchTree tree = sampleTree();
        const OperationRecord rec = tree.update(30, 75);
        QVERIFY(rec.result.succeeded());
        bool sawTentative = false;
        for (const AlgorithmEvent& e : rec.events) {
            if (e.type == EventType::TransactionStarted || e.type == EventType::NodeRemoved
                || e.type == EventType::NodeInserted) {
                QVERIFY(e.tentative);
                sawTentative = true;
            }
        }
        QVERIFY(sawTentative);
        QVERIFY(!rec.events.back().tentative);
        // Between the removal and the reinsertion the working copy is still a
        // valid BST (it simply lacks the key), so no invalid tree is ever shown.
        for (const TreeSnapshot& s : rec.snapshots)
            QVERIFY(validateSnapshot(s).valid);
        // Nested deletion/insertion steps are mapped to the update pseudocode.
        QVERIFY(hasEvent(rec, EventType::DeletionCaseIdentified));
        QCOMPARE(firstEvent(rec, EventType::DeletionCaseIdentified).line, PseudoLine::UpdDelete);
        QCOMPARE(firstEvent(rec, EventType::NodeInserted).line, PseudoLine::UpdInsert);
    }

    void failedUpdatePreservesOriginalTree()
    {
        // Rejections happen before the working copy is committed; the live
        // tree must be byte-for-byte identical (keys, shape and node ids).
        BinarySearchTree tree = sampleTree();
        const TreeSnapshot before = tree.snapshot();
        tree.update(40, 70);
        tree.update(41, 1);
        tree.update(40, 40);
        const TreeSnapshot after = tree.snapshot();
        QCOMPARE(after.nodes.size(), before.nodes.size());
        for (std::size_t i = 0; i < before.nodes.size(); ++i) {
            QCOMPARE(after.nodes[i].id, before.nodes[i].id);
            QCOMPARE(after.nodes[i].key, before.nodes[i].key);
            QCOMPARE(after.nodes[i].left, before.nodes[i].left);
            QCOMPARE(after.nodes[i].right, before.nodes[i].right);
        }
    }

    void updateSequenceKeepsInvariant()
    {
        BinarySearchTree tree = sampleTree();
        const std::pair<int, int> updates[] = {{50, 55}, {20, 99}, {99, -3}, {80, 81}, {55, 50}, {60, 35}};
        for (const auto& [from, to] : updates) {
            QVERIFY(tree.update(from, to).result.succeeded());
            QVERIFY2(tree.validate().valid, tree.validate().error.c_str());
            QCOMPARE(tree.size(), std::size_t(7));
        }
    }

    // --- Recording and snapshots ---------------------------------------------
    void snapshotsAreIndependentOfLiveTree()
    {
        BinarySearchTree tree = sampleTree();
        const OperationRecord rec = tree.remove(50);
        const QList<int> recordedBefore = toList(rec.before().preorderKeys());
        tree.clearTree();
        tree.rebuild({1, 2, 3});
        // The recording still describes the tree as it was.
        QCOMPARE(recordedBefore, (QList<int>{50, 30, 20, 40, 70, 60, 80}));
        QCOMPARE(toList(rec.before().preorderKeys()), recordedBefore);
        QCOMPARE(toList(rec.after().inorderKeys()), (QList<int>{20, 30, 40, 60, 70, 80}));
    }

    void metricsAreCumulativeAndMonotonic()
    {
        BinarySearchTree tree = sampleTree();
        const OperationRecord rec = tree.update(20, 85);
        OperationMetrics prev;
        for (const AlgorithmEvent& e : rec.events) {
            QVERIFY(e.metrics.nodesVisited >= prev.nodesVisited);
            QVERIFY(e.metrics.keyComparisons >= prev.keyComparisons);
            QVERIFY(e.metrics.structuralChanges >= prev.structuralChanges);
            prev = e.metrics;
        }
        QCOMPARE(rec.result.metrics.structuralChanges, 2); // unlink leaf + attach new node
    }

    void rebuildRejectsDuplicatesWithoutChangingTree()
    {
        BinarySearchTree tree = sampleTree();
        const QList<int> before = preorderOf(tree);
        const OperationRecord rec = tree.rebuild({5, 6, 5});
        QCOMPARE(rec.result.code, ResultCode::DuplicateKey);
        QCOMPARE(preorderOf(tree), before);
    }

    void rebuildReplacesTreeAndKeepsIdsUnique()
    {
        BinarySearchTree tree = sampleTree();
        const TreeSnapshot old = tree.snapshot();
        QVERIFY(tree.rebuild({3, 1, 2}).result.succeeded());
        QCOMPARE(preorderOf(tree), (QList<int>{3, 1, 2}));
        for (const SnapshotNode& n : tree.snapshot().nodes)
            QVERIFY(!old.find(n.id)); // fresh ids never collide with the previous tree's
    }

    void clearTreeEmptiesEverything()
    {
        BinarySearchTree tree = sampleTree();
        const OperationRecord rec = tree.clearTree();
        QVERIFY(tree.empty());
        QCOMPARE(rec.before().size(), 7);
        QVERIFY(rec.after().empty());
    }

    void moveAssignmentTransfersOwnership()
    {
        BinarySearchTree a = sampleTree();
        BinarySearchTree b = treeOf({1});
        b = std::move(a);
        QCOMPARE(b.size(), std::size_t(7));
        QVERIFY(b.validate().valid);
    }
};

QTEST_APPLESS_MAIN(TestTreeOperations)
#include "tst_treeoperations.moc"
