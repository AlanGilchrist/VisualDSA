// Insertion, search, traversal and input-validation tests for the core engine.

#include "BinarySearchTree.h"
#include "Pseudocode.h"
#include "Validation.h"

#include <QtTest>

#include <climits>

using namespace vdsa;

namespace {

BinarySearchTree sampleTree()
{
    BinarySearchTree tree;
    for (int key : BinarySearchTree::sampleKeys())
        tree.insert(key);
    return tree;
}

QList<int> toList(const std::vector<int>& v) { return QList<int>(v.begin(), v.end()); }

int keyOf(const TreeSnapshot& s, NodeId id) { return s.find(id) ? s.find(id)->key : INT_MIN; }

} // namespace

class TestBinarySearchTree : public QObject {
    Q_OBJECT

private slots:
    // --- Insertion ---------------------------------------------------------
    void insertIntoEmptyTree()
    {
        BinarySearchTree tree;
        const OperationRecord rec = tree.insert(50);
        QCOMPARE(rec.result.status, ResultStatus::Success);
        QVERIFY(rec.result.treeChanged);
        QCOMPARE(tree.size(), std::size_t(1));
        QVERIFY(tree.root());
        QCOMPARE(tree.root()->key, 50);
        QCOMPARE(tree.height(), 0);
        QVERIFY(tree.validate().valid);
    }

    void insertSmallerGoesLeft()
    {
        BinarySearchTree tree;
        tree.insert(50);
        tree.insert(30);
        QVERIFY(tree.root()->left);
        QCOMPARE(tree.root()->left->key, 30);
        QVERIFY(!tree.root()->right);
    }

    void insertLargerGoesRight()
    {
        BinarySearchTree tree;
        tree.insert(50);
        tree.insert(70);
        QVERIFY(tree.root()->right);
        QCOMPARE(tree.root()->right->key, 70);
        QVERIFY(!tree.root()->left);
    }

    void insertSampleTreeShape()
    {
        const BinarySearchTree tree = sampleTree();
        const BSTNode* root = tree.root();
        QCOMPARE(root->key, 50);
        QCOMPARE(root->left->key, 30);
        QCOMPARE(root->right->key, 70);
        QCOMPARE(root->left->left->key, 20);
        QCOMPARE(root->left->right->key, 40);
        QCOMPARE(root->right->left->key, 60);
        QCOMPARE(root->right->right->key, 80);
        QCOMPARE(tree.size(), std::size_t(7));
        QCOMPARE(tree.height(), 2);
        QVERIFY(tree.validate().valid);
    }

    void insertDuplicateRejectedTreeUnchanged()
    {
        BinarySearchTree tree = sampleTree();
        const TreeSnapshot before = tree.snapshot();
        const OperationRecord rec = tree.insert(40);
        QCOMPARE(rec.result.status, ResultStatus::Rejected);
        QCOMPARE(rec.result.code, ResultCode::DuplicateKey);
        QCOMPARE(QString::fromStdString(rec.result.message), QString("Key already exists."));
        QVERIFY(!rec.result.treeChanged);
        QCOMPARE(tree.size(), std::size_t(7));
        QCOMPARE(toList(tree.snapshot().preorderKeys()), toList(before.preorderKeys()));
        QCOMPARE(rec.events.back().type, EventType::OperationRejected);
        QCOMPARE(rec.events.back().line, PseudoLine::InsDuplicate);
        QVERIFY(tree.validate().valid);
    }

    void insertKeepsInvariantForManyKeys()
    {
        BinarySearchTree tree;
        const int keys[] = {41, 20, 65, 11, 29, 50, 91, 32, 72, 99, -5, 0, 30, 31};
        for (int key : keys) {
            QVERIFY(tree.insert(key).result.succeeded());
            QVERIFY2(tree.validate().valid, tree.validate().error.c_str());
        }
        QCOMPARE(tree.size(), std::size_t(std::size(keys)));
    }

    void insertEventsDescribeThePath()
    {
        BinarySearchTree tree = sampleTree();
        const OperationRecord rec = tree.insert(45); // 50 -> L 30 -> R 40 -> R (empty)
        QCOMPARE(rec.result.status, ResultStatus::Success);

        QList<int> compared;
        QList<EventType> moves;
        for (const AlgorithmEvent& e : rec.events) {
            if (e.type == EventType::KeyCompared)
                compared << *e.comparedKey;
            if (e.type == EventType::MoveLeft || e.type == EventType::MoveRight)
                moves << e.type;
        }
        QCOMPARE(compared, (QList<int>{50, 30, 40}));
        QCOMPARE(moves, (QList<EventType>{EventType::MoveLeft, EventType::MoveRight, EventType::MoveRight}));

        // The last move ends at an empty right slot below 40.
        const AlgorithmEvent* lastMove = nullptr;
        for (const AlgorithmEvent& e : rec.events)
            if (e.type == EventType::MoveRight)
                lastMove = &e;
        QCOMPARE(keyOf(rec.before(), lastMove->pendingParent), 40);
        QCOMPARE(lastMove->pendingSide, Direction::Right);

        QCOMPARE(rec.result.metrics.keyComparisons, 3);
        QCOMPARE(rec.result.metrics.nodesVisited, 3);
        QCOMPARE(rec.result.metrics.structuralChanges, 1);
        QCOMPARE(rec.snapshots.size(), std::size_t(2)); // before + after attach
        QCOMPARE(rec.after().size(), 8);
        QCOMPARE(rec.events.back().type, EventType::OperationCompleted);
    }

    void insertRespectsCapacity()
    {
        BinarySearchTree tree;
        tree.setCapacity(2);
        QVERIFY(tree.insert(1).result.succeeded());
        QVERIFY(tree.insert(2).result.succeeded());
        const OperationRecord rec = tree.insert(3);
        QCOMPARE(rec.result.code, ResultCode::CapacityReached);
        QCOMPARE(tree.size(), std::size_t(2));
    }

    // --- Search ------------------------------------------------------------
    void searchFindsRoot()
    {
        const BinarySearchTree tree = sampleTree();
        const OperationRecord rec = tree.search(50);
        QCOMPARE(rec.result.status, ResultStatus::Success);
        QCOMPARE(rec.result.code, ResultCode::KeyFound);
        QCOMPARE(rec.result.metrics.nodesVisited, 1);
        QCOMPARE(rec.events.back().type, EventType::NodeFound);
        QCOMPARE(keyOf(rec.after(), rec.events.back().nodeId), 50);
    }

    void searchFindsLeaf()
    {
        const BinarySearchTree tree = sampleTree();
        const OperationRecord rec = tree.search(60);
        QCOMPARE(rec.result.code, ResultCode::KeyFound);
        QCOMPARE(rec.result.metrics.nodesVisited, 3);
        QCOMPARE(rec.result.metrics.keyComparisons, 3);
        const AlgorithmEvent& found = rec.events.back();
        QCOMPARE(found.line, PseudoLine::SrchFound);
        QCOMPARE(found.emphasis, Emphasis::Success);
        // The recorded path is root -> 70 -> 60.
        QList<int> pathKeys;
        for (NodeId id : found.path)
            pathKeys << keyOf(rec.after(), id);
        QCOMPARE(pathKeys, (QList<int>{50, 70, 60}));
    }

    void searchFindsDeepNode()
    {
        BinarySearchTree tree = sampleTree();
        for (int key : {45, 43, 44})
            tree.insert(key);
        const OperationRecord rec = tree.search(44); // 50,30,40,45,43,44
        QCOMPARE(rec.result.code, ResultCode::KeyFound);
        QCOMPARE(rec.result.metrics.nodesVisited, 6);
    }

    void searchMissingKey()
    {
        const BinarySearchTree tree = sampleTree();
        const OperationRecord rec = tree.search(65); // 50 -> 70 -> 60 -> right(empty)
        QCOMPARE(rec.result.status, ResultStatus::NotFound);
        QCOMPARE(QString::fromStdString(rec.result.message), QString("Key not found."));
        const AlgorithmEvent& last = rec.events.back();
        QCOMPARE(last.type, EventType::NodeNotFound);
        QCOMPARE(keyOf(rec.after(), last.pendingParent), 60);
        QCOMPARE(last.pendingSide, Direction::Right);
        QCOMPARE(rec.result.metrics.nodesVisited, 3);
    }

    void searchEmptyTree()
    {
        const BinarySearchTree tree;
        const OperationRecord rec = tree.search(10);
        QCOMPARE(rec.result.status, ResultStatus::NotFound);
        QCOMPARE(rec.events.size(), std::size_t(1));
        QCOMPARE(rec.result.metrics.nodesVisited, 0);
    }

    void searchDoesNotModifyTree()
    {
        BinarySearchTree tree = sampleTree();
        const auto before = tree.snapshot().preorderKeys();
        tree.search(40);
        tree.search(41);
        QCOMPARE(toList(tree.snapshot().preorderKeys()), toList(before));
    }

    // --- Traversals -----------------------------------------------------------
    void inorderSample() { QCOMPARE(toList(sampleTree().inorder()), (QList<int>{20, 30, 40, 50, 60, 70, 80})); }
    void preorderSample() { QCOMPARE(toList(sampleTree().preorder()), (QList<int>{50, 30, 20, 40, 70, 60, 80})); }
    void postorderSample() { QCOMPARE(toList(sampleTree().postorder()), (QList<int>{20, 40, 30, 60, 80, 70, 50})); }

    void traversalsOnEmptyTree()
    {
        const BinarySearchTree tree;
        for (TraversalOrder order : {TraversalOrder::Inorder, TraversalOrder::Preorder, TraversalOrder::Postorder}) {
            const OperationRecord rec = tree.traverse(order);
            QCOMPARE(rec.result.status, ResultStatus::Success);
            QVERIFY(rec.result.sequence.empty());
            QCOMPARE(rec.result.metrics.traversalVisits, 0);
        }
    }

    void traversalsOnSingleNode()
    {
        BinarySearchTree tree;
        tree.insert(7);
        QCOMPARE(toList(tree.inorder()), QList<int>{7});
        QCOMPARE(toList(tree.preorder()), QList<int>{7});
        QCOMPARE(toList(tree.postorder()), QList<int>{7});
    }

    void traversalEventsEmitInOrder()
    {
        const BinarySearchTree tree = sampleTree();
        const OperationRecord rec = tree.traverse(TraversalOrder::Postorder);
        QList<int> visited;
        for (const AlgorithmEvent& e : rec.events) {
            if (e.type == EventType::TraversalNodeVisited) {
                visited << e.key;
                // The progressive sequence ends with the key just visited.
                QCOMPARE(e.sequence.back(), e.key);
                QCOMPARE(e.completed.size(), e.sequence.size());
                QCOMPARE(e.line, PseudoLine::TrvVisit);
            }
        }
        QCOMPARE(visited, (QList<int>{20, 40, 30, 60, 80, 70, 50}));
        QCOMPARE(rec.result.metrics.traversalVisits, 7);
        QCOMPARE(rec.result.metrics.nodesVisited, 7);
    }

    void inorderIsSortedForArbitraryTree()
    {
        BinarySearchTree tree;
        for (int key : {13, 7, 21, 3, 9, 15, 30, 1, 8, 25})
            tree.insert(key);
        std::vector<int> expected = tree.inorder();
        std::vector<int> sorted = expected;
        std::sort(sorted.begin(), sorted.end());
        QCOMPARE(toList(expected), toList(sorted));
    }

    // --- Pseudocode metadata ------------------------------------------------
    void everyEventLineExistsInListing()
    {
        BinarySearchTree tree = sampleTree();
        std::vector<OperationRecord> records;
        records.push_back(tree.insert(45));
        records.push_back(tree.insert(45));
        records.push_back(tree.search(60));
        records.push_back(tree.search(61));
        records.push_back(tree.traverse(TraversalOrder::Inorder));
        records.push_back(tree.traverse(TraversalOrder::Preorder));
        records.push_back(tree.traverse(TraversalOrder::Postorder));
        records.push_back(tree.remove(20));
        records.push_back(tree.remove(30));
        records.push_back(tree.remove(50));
        records.push_back(tree.remove(999));
        records.push_back(tree.update(40, 90));
        records.push_back(tree.update(60, 80));
        records.push_back(tree.rebuild({1, 2, 3}));
        records.push_back(tree.clearTree());
        for (const OperationRecord& rec : records) {
            for (const AlgorithmEvent& e : rec.events) {
                QVERIFY2(pseudocodeRow(rec.result.operation, e.line) >= 0,
                         qPrintable(QString("op %1 has an event whose line is not in its listing")
                                        .arg(operationName(rec.result.operation))));
                QVERIFY(e.snapshotIndex >= 0 && e.snapshotIndex < int(rec.snapshots.size()));
                QVERIFY(!e.action.empty());
            }
        }
    }

    // --- Input validation ---------------------------------------------------
    void parseKeyValid()
    {
        QCOMPARE(parseKey("42").value, 42);
        QCOMPARE(parseKey("  -17 ").value, -17);
        QCOMPARE(parseKey("+5").value, 5);
        QVERIFY(parseKey("0").ok);
    }

    void parseKeyErrors_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<int>("error");
        QTest::newRow("empty") << "" << int(InputError::Empty);
        QTest::newRow("spaces") << "   " << int(InputError::Empty);
        QTest::newRow("letters") << "abc" << int(InputError::InvalidInteger);
        QTest::newRow("decimal") << "4.5" << int(InputError::InvalidInteger);
        QTest::newRow("sign only") << "-" << int(InputError::InvalidInteger);
        QTest::newRow("inner space") << "4 5" << int(InputError::InvalidInteger);
        QTest::newRow("overflow") << "2147483648" << int(InputError::Overflow);
        QTest::newRow("huge") << "99999999999999999999999" << int(InputError::Overflow);
        QTest::newRow("negative overflow") << "-2147483649" << int(InputError::Overflow);
        QTest::newRow("out of range") << "10000" << int(InputError::OutOfRange);
        QTest::newRow("int max") << "2147483647" << int(InputError::OutOfRange);
    }

    void parseKeyErrors()
    {
        QFETCH(QString, text);
        QFETCH(int, error);
        const KeyParseResult r = parseKey(text.toStdString());
        QVERIFY(!r.ok);
        QCOMPARE(int(r.error), error);
        QVERIFY(!r.message.empty());
    }

    void parseKeyListValid()
    {
        const KeyListParseResult r = parseKeyList("50, 30,70 20  40,");
        QVERIFY(r.ok);
        QCOMPARE(toList(r.keys), (QList<int>{50, 30, 70, 20, 40}));
    }

    void parseKeyListErrors()
    {
        QCOMPARE(parseKeyList("").error, InputError::Empty);
        QCOMPARE(parseKeyList("1,,2").error, InputError::EmptyEntry);
        QCOMPARE(parseKeyList("1, x").error, InputError::InvalidInteger);
        QCOMPARE(parseKeyList("1, 30, 1").error, InputError::DuplicateKeys);
        QCOMPARE(parseKeyList("1, 99999999999").error, InputError::Overflow);
        QCOMPARE(parseKeyList("1 2 3", 2).error, InputError::TooManyKeys);
        QVERIFY(parseKeyList("1, 30, 1").keys.empty());
    }
};

QTEST_APPLESS_MAIN(TestBinarySearchTree)
#include "tst_binarysearchtree.moc"
