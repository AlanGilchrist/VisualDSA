// Tree-invariant tests: the validator itself, snapshot consistency, and a
// deterministic randomized comparison against std::set as a reference model.

#include "BinarySearchTree.h"
#include "TreeValidator.h"

#include <QtTest>

#include <numeric>
#include <random>
#include <set>

using namespace vdsa;

namespace {

QList<int> toList(const std::vector<int>& v) { return QList<int>(v.begin(), v.end()); }

std::unique_ptr<BSTNode> node(int key, NodeId id, std::unique_ptr<BSTNode> left = nullptr,
                              std::unique_ptr<BSTNode> right = nullptr)
{
    auto n = std::make_unique<BSTNode>(key, id);
    n->left = std::move(left);
    n->right = std::move(right);
    return n;
}

} // namespace

class TestTreeInvariants : public QObject {
    Q_OBJECT

private slots:
    void validatorAcceptsEmptyTree()
    {
        const ValidationReport r = validateTree(nullptr);
        QVERIFY(r.valid);
        QCOMPARE(r.nodeCount, 0);
        QCOMPARE(r.height, -1);
    }

    void validatorAcceptsValidTree()
    {
        auto root = node(50, 0, node(30, 1, node(20, 2), node(40, 3)), node(70, 4));
        const ValidationReport r = validateTree(root.get());
        QVERIFY(r.valid);
        QCOMPARE(r.nodeCount, 5);
        QCOMPARE(r.height, 2);
    }

    void validatorRejectsWrongChildSide()
    {
        auto root = node(50, 0, node(60, 1)); // 60 on the left of 50
        QVERIFY(!validateTree(root.get()).valid);
    }

    void validatorChecksWholeSubtreeNotJustParent()
    {
        // 55 is a valid right child of 30, but it sits in 50's left subtree.
        auto root = node(50, 0, node(30, 1, nullptr, node(55, 2)), node(70, 3));
        const ValidationReport r = validateTree(root.get());
        QVERIFY(!r.valid);
        QVERIFY(QString::fromStdString(r.error).contains("55"));
    }

    void validatorRejectsDuplicateKeys()
    {
        auto root = node(50, 0, nullptr, node(50, 1));
        QVERIFY(!validateTree(root.get()).valid);
    }

    void validatorRejectsDuplicateIds()
    {
        auto root = node(50, 7, node(30, 7));
        const ValidationReport r = validateTree(root.get());
        QVERIFY(!r.valid);
        QVERIFY(QString::fromStdString(r.error).contains("id"));
    }

    void sizeAndHeightAreCorrect()
    {
        BinarySearchTree tree;
        QCOMPARE(tree.height(), -1);
        for (int key : {50, 30, 70, 20, 40, 60, 80}) // balanced shape, height 2
            tree.insert(key);
        QCOMPARE(tree.size(), std::size_t(7));
        QCOMPARE(tree.height(), 2);
        QCOMPARE(tree.validate().nodeCount, 7);

        BinarySearchTree skewed;
        for (int key = 1; key <= 10; ++key) // sorted input -> skewed, height n - 1
            skewed.insert(key);
        QCOMPARE(skewed.height(), 9);
        QVERIFY(skewed.validate().valid);
    }

    void snapshotMatchesLiveTree()
    {
        BinarySearchTree tree;
        for (int key : {8, 3, 10, 1, 6, 14, 4, 7, 13})
            tree.insert(key);
        const TreeSnapshot s = tree.snapshot();
        QVERIFY(validateSnapshot(s).valid);
        QCOMPARE(s.size(), 9);
        QCOMPARE(s.height(), tree.height());
        QCOMPARE(s.edgeCount(), 8);
        QCOMPARE(toList(s.inorderKeys()), toList(tree.inorder()));
        QCOMPARE(toList(s.preorderKeys()), toList(tree.preorder()));
        QCOMPARE(s.subtreeSize(s.root), 9);
        // pathTo(13) is 8 -> 10 -> 14 -> 13
        QList<int> path;
        for (NodeId id : s.pathTo(s.findKey(13)->id))
            path << s.find(id)->key;
        QCOMPARE(path, (QList<int>{8, 10, 14, 13}));
    }

    void validateSnapshotDetectsInconsistency()
    {
        BinarySearchTree tree;
        for (int key : {2, 1, 3})
            tree.insert(key);
        TreeSnapshot s = tree.snapshot();
        s.nodes.back().parent = kNoNode; // break a parent link
        QVERIFY(!validateSnapshot(s).valid);
    }

    void cloneIsDeepAndPreservesIds()
    {
        BinarySearchTree tree;
        for (int key : {5, 3, 8})
            tree.insert(key);
        BinarySearchTree copy = tree.clone();
        QCOMPARE(copy.snapshot().nodes.size(), tree.snapshot().nodes.size());
        QCOMPARE(copy.snapshot().root, tree.snapshot().root);
        QVERIFY(copy.root() != tree.root()); // distinct nodes
        copy.remove(3);
        QVERIFY(tree.contains(3)); // original unaffected
        QVERIFY(copy.validate().valid && tree.validate().valid);
    }

    void deepSkewedTreeIsDestroyedSafely()
    {
        // Sorted keys produce a single 20 000-node chain. Recursive
        // unique_ptr destruction at that depth risks a stack overflow;
        // clear() and the destructor must release nodes iteratively.
        std::vector<int> keys(20000);
        std::iota(keys.begin(), keys.end(), 0);
        BinarySearchTree tree;
        QVERIFY(tree.rebuild(keys).result.succeeded());
        QCOMPARE(tree.size(), keys.size());
        QCOMPARE(tree.height(), int(keys.size()) - 1);
        tree.clear();
        QVERIFY(tree.empty());
        QVERIFY(!tree.root());

        // Destructor path: let a second deep tree go out of scope.
        {
            BinarySearchTree scoped;
            scoped.rebuild(keys);
        }
    }

    void randomizedOperationsMatchReferenceSet()
    {
        std::mt19937 rng(20241021); // fixed seed: deterministic
        std::uniform_int_distribution<int> keyDist(-200, 200);
        std::uniform_int_distribution<int> opDist(0, 2);
        BinarySearchTree tree;
        std::set<int> reference;

        for (int step = 0; step < 3000; ++step) {
            const int key = keyDist(rng);
            switch (opDist(rng)) {
            case 0: {
                const OperationRecord rec = tree.insert(key);
                QCOMPARE(rec.result.succeeded(), reference.insert(key).second);
                break;
            }
            case 1: {
                const OperationRecord rec = tree.remove(key);
                QCOMPARE(rec.result.succeeded(), reference.erase(key) == 1);
                break;
            }
            case 2: {
                const OperationRecord rec = tree.search(key);
                QCOMPARE(rec.result.code == ResultCode::KeyFound, reference.count(key) == 1);
                break;
            }
            }
            const ValidationReport report = tree.validate();
            QVERIFY2(report.valid, report.error.c_str());
            QCOMPARE(tree.size(), reference.size());
        }
        QCOMPARE(toList(tree.inorder()), toList(std::vector<int>(reference.begin(), reference.end())));
    }
};

QTEST_APPLESS_MAIN(TestTreeInvariants)
#include "tst_treeinvariants.moc"
