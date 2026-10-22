// Visualization and GUI tests: layout, canvas state, playback control and
// synchronisation between events, pseudocode and explanations. Runs on the
// "offscreen" Qt platform (set by CTest).

#include "AnimationController.h"
#include "ComplexityPanel.h"
#include "ExplanationPanel.h"
#include "HistoryPanel.h"
#include "MainWindow.h"
#include "OperationPanel.h"
#include "PseudocodePanel.h"
#include "TreeCanvas.h"
#include "TreeLayout.h"

#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest>

using namespace vdsa;

namespace {

BinarySearchTree sampleTree()
{
    BinarySearchTree tree;
    tree.rebuild(BinarySearchTree::sampleKeys());
    return tree;
}

std::shared_ptr<const OperationRecord> share(OperationRecord rec)
{
    return std::make_shared<const OperationRecord>(std::move(rec));
}

} // namespace

class TestVisualization : public QObject {
    Q_OBJECT

private slots:
    // --- Layout ----------------------------------------------------------------
    void layoutHasNoOverlapAndFollowsInorder()
    {
        BinarySearchTree tree;
        for (int key : {50, 30, 70, 20, 40, 60, 80, 10, 25, 35, 45, 5, 1, 90, 85, 95, 99})
            tree.insert(key);
        const TreeSnapshot s = tree.snapshot();
        const TreeLayout layout = TreeLayout::compute(s);
        const qreal minGap = 2 * layout.metrics().nodeRadius;

        for (const SnapshotNode& a : s.nodes)
            for (const SnapshotNode& b : s.nodes)
                if (a.id != b.id)
                    QVERIFY(QLineF(layout.position(a.id), layout.position(b.id)).length() >= minGap);

        // Left-to-right order on screen equals ascending key order.
        qreal lastX = -1e9;
        for (NodeId id : s.inorderIds()) {
            QVERIFY(layout.position(id).x() > lastX);
            lastX = layout.position(id).x();
        }
        // Children are below their parent, on the correct side.
        for (const SnapshotNode& n : s.nodes) {
            if (n.left != kNoNode)
                QVERIFY(layout.position(n.left).x() < layout.position(n.id).x());
            if (n.right != kNoNode)
                QVERIFY(layout.position(n.right).x() > layout.position(n.id).x());
            if (n.parent != kNoNode)
                QVERIFY(layout.position(n.id).y() > layout.position(n.parent).y());
        }
    }

    void layoutOfEmptyTree()
    {
        const TreeLayout layout = TreeLayout::compute(TreeSnapshot{});
        QVERIFY(layout.positions().empty());
        QVERIFY(layout.bounds().isNull());
    }

    // --- Canvas ----------------------------------------------------------------
    void canvasRendersSampleTree()
    {
        TreeCanvas canvas;
        canvas.resize(900, 600);
        canvas.showTree(sampleTree().snapshot());
        QCOMPARE(canvas.nodeCount(), 7);
        QCOMPARE(canvas.edgeCount(), 6);
        QCOMPARE(canvas.highlightedNode(), kNoNode);
        const QImage image = canvas.grab().toImage();
        QVERIFY(!image.isNull());
    }

    void canvasRendersEmptyTree()
    {
        TreeCanvas canvas;
        canvas.resize(600, 400);
        canvas.showTree(TreeSnapshot{});
        QCOMPARE(canvas.nodeCount(), 0);
        QCOMPARE(canvas.edgeCount(), 0);
        QVERIFY(!canvas.grab().isNull());
    }

    void canvasHighlightsEventNode()
    {
        const BinarySearchTree tree = sampleTree();
        const OperationRecord rec = tree.search(60);
        TreeCanvas canvas;
        canvas.resize(900, 600);
        for (const AlgorithmEvent& e : rec.events) {
            canvas.showStep(rec.snapshotFor(e), e, 0);
            QCOMPARE(canvas.highlightedNode(), e.nodeId);
            QVERIFY(!canvas.grab().isNull()); // every step paints without error
        }
        QCOMPARE(rec.after().find(canvas.highlightedNode())->key, 60);
    }

    void canvasAnimatesStructuralChange()
    {
        BinarySearchTree tree = sampleTree();
        const TreeSnapshot before = tree.snapshot();
        tree.insert(45);
        TreeCanvas canvas;
        canvas.resize(900, 600);
        canvas.showTree(before);
        canvas.showTree(tree.snapshot(), 200);
        QVERIFY(canvas.isAnimating());
        QTRY_VERIFY_WITH_TIMEOUT(!canvas.isAnimating(), 2000);
        QCOMPARE(canvas.nodeCount(), 8);
    }

    void canvasZoomIsClampedAndFitRestoresAutoFit()
    {
        TreeCanvas canvas;
        canvas.resize(900, 600);
        canvas.showTree(sampleTree().snapshot());
        for (int i = 0; i < 40; ++i)
            canvas.zoomIn();
        QCOMPARE(canvas.zoom(), TreeCanvas::kMaxZoom);
        QVERIFY(!canvas.isAutoFit());
        for (int i = 0; i < 40; ++i)
            canvas.zoomOut();
        QCOMPARE(canvas.zoom(), TreeCanvas::kMinZoom);
        canvas.fitToView();
        QVERIFY(canvas.isAutoFit());
    }

    void canvasClickSelectsNode()
    {
        const TreeSnapshot s = sampleTree().snapshot();
        TreeCanvas canvas;
        canvas.resize(900, 600);
        canvas.show();
        QVERIFY(QTest::qWaitForWindowExposed(&canvas));
        canvas.showTree(s);
        QSignalSpy spy(&canvas, &TreeCanvas::nodeSelected);
        const NodeId target = s.findKey(40)->id;
        QTest::mouseClick(&canvas, Qt::LeftButton, {}, canvas.nodeScreenPosition(target).toPoint());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(canvas.selectedNode(), target);
    }

    // --- Animation controller -------------------------------------------------------
    void controllerBoundaries()
    {
        AnimationController controller;
        QCOMPARE(controller.stepCount(), 0);
        QVERIFY(!controller.currentEvent());
        controller.next(); // no record: must be a harmless no-op
        controller.previous();

        const auto rec = share(sampleTree().search(80));
        controller.load(rec, false);
        QCOMPARE(controller.currentStep(), 0);
        QVERIFY(!controller.isPlaying());
        controller.previous();
        QCOMPARE(controller.currentStep(), 0);
        controller.jumpTo(10000);
        QCOMPARE(controller.currentStep(), controller.stepCount() - 1);
        QVERIFY(controller.atEnd());
        controller.next();
        QCOMPARE(controller.currentStep(), controller.stepCount() - 1);
        controller.jumpTo(-5);
        QCOMPARE(controller.currentStep(), 0);
    }

    void controllerPauseAndResume()
    {
        AnimationController controller;
        controller.setSpeed(4.0);
        controller.load(share(sampleTree().traverse(TraversalOrder::Inorder)), true);
        QVERIFY(controller.isPlaying());
        QTRY_VERIFY_WITH_TIMEOUT(controller.currentStep() >= 1, 2000);

        controller.pause();
        QVERIFY(!controller.isPlaying());
        const int paused = controller.currentStep();
        QTest::qWait(controller.stepIntervalMs() * 3);
        QCOMPARE(controller.currentStep(), paused); // no progress while paused

        controller.next(); // stepping works while paused
        QCOMPARE(controller.currentStep(), paused + 1);
        controller.previous();
        QCOMPARE(controller.currentStep(), paused);

        controller.play(); // resume from the current step
        QTRY_VERIFY_WITH_TIMEOUT(controller.currentStep() > paused, 2000);
    }

    void controllerReachesConsistentEnd()
    {
        AnimationController controller;
        controller.setSpeed(8.0);
        QSignalSpy finished(&controller, &AnimationController::finished);
        controller.load(share(sampleTree().search(20)), true);
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
        QVERIFY(controller.atEnd());
        QVERIFY(!controller.isPlaying());
        QCOMPARE(controller.currentEvent()->type, EventType::NodeFound);

        controller.play(); // playing a finished timeline restarts it
        QCOMPARE(controller.currentStep(), 0);
        QVERIFY(controller.isPlaying());
        controller.reset();
        QCOMPARE(controller.currentStep(), 0);
        QVERIFY(!controller.isPlaying());
    }

    void controllerLoadCancelsPreviousPlayback()
    {
        AnimationController controller;
        controller.setSpeed(8.0);
        const auto first = share(sampleTree().traverse(TraversalOrder::Postorder));
        const auto second = share(sampleTree().search(50)); // 2 steps
        controller.load(first, true);
        QTRY_VERIFY_WITH_TIMEOUT(controller.currentStep() >= 2, 2000);
        controller.load(second, true);
        QVERIFY(controller.record() == second);
        QCOMPARE(controller.currentStep(), 0);
        QTest::qWait(controller.stepIntervalMs() * 6);
        QCOMPARE(controller.stepCount(), int(second->events.size()));
        QVERIFY(controller.currentStep() < controller.stepCount());
        QVERIFY(controller.atEnd());
        QVERIFY(!controller.isPlaying());
    }

    void controllerSpeedChangesInterval()
    {
        AnimationController controller;
        controller.setSpeed(0.5);
        QCOMPARE(controller.stepIntervalMs(), 2000);
        controller.setSpeed(2.0);
        QCOMPARE(controller.stepIntervalMs(), 500);
    }

    // --- Main window integration ------------------------------------------------------
    void mainWindowStartsWithSampleTree()
    {
        MainWindow window;
        QCOMPARE(window.tree().size(), std::size_t(7));
        QCOMPARE(window.canvas()->nodeCount(), 7);
        QCOMPARE(window.history()->entryCount(), 1);
    }

    void eventPseudocodeAndExplanationStayInSync()
    {
        MainWindow window;
        window.setConfirmationsEnabled(false);
        window.controller()->setSpeed(8.0);
        window.selectModule(Module::Delete);
        window.deleteKey(30); // two-children case: richest event stream

        AnimationController* controller = window.controller();
        QVERIFY(controller->stepCount() > 5);
        QCOMPARE(window.pseudocode()->operation(), OperationType::Delete);
        for (int step = 0; step < controller->stepCount(); ++step) {
            controller->jumpTo(step);
            const AlgorithmEvent* e = controller->currentEvent();
            QCOMPARE(window.pseudocode()->activeLine(), e->line);
            QVERIFY(window.pseudocode()->activeRow() >= 0);
            QCOMPARE(window.explanation()->actionText(), QString::fromStdString(e->action));
            QCOMPARE(window.canvas()->highlightedNode(), e->nodeId);
            QCOMPARE(window.canvas()->nodeCount(), controller->record()->snapshotFor(*e).size());
            QCOMPARE(window.complexity()->measuredValue("visited"), QString::number(e->metrics.nodesVisited));
        }
        QCOMPARE(window.statusText(), QString("Deletion completed."));
        QCOMPARE(window.canvas()->nodeCount(), 6);
    }

    void invalidInputDoesNotTouchTree()
    {
        MainWindow window;
        window.setConfirmationsEnabled(false);
        window.selectModule(Module::Insert);
        auto* edit = window.findChild<QLineEdit*>("InsertKey");
        auto* run = window.findChild<QPushButton*>("InsertKeyButton");
        QVERIFY(edit && run);

        const int historyBefore = window.history()->entryCount();
        for (const char* bad : {"", "abc", "4.5", "99999999999", "10000"}) {
            edit->setText(QString::fromLatin1(bad));
            run->click();
            QVERIFY2(!window.operations()->errorText().isEmpty(), bad);
            QCOMPARE(window.tree().size(), std::size_t(7));
            QCOMPARE(window.history()->entryCount(), historyBefore);
        }
        edit->setText("45");
        run->click();
        QVERIFY(window.operations()->errorText().isEmpty());
        QCOMPARE(window.tree().size(), std::size_t(8));
    }

    void duplicateTreeInputIsRejected()
    {
        MainWindow window;
        window.setConfirmationsEnabled(false);
        window.selectModule(Module::Overview);
        auto* edit = window.findChild<QLineEdit*>("TreeInput");
        auto* create = window.findChild<QPushButton*>("CreateTreeButton");
        QVERIFY(edit && create);
        const auto before = window.tree().snapshot().preorderKeys();
        edit->setText("10, 20, 10");
        create->click();
        QVERIFY(window.operations()->errorText().contains("unique"));
        QCOMPARE(window.tree().snapshot().preorderKeys(), before);

        edit->setText("8, 4, 12");
        create->click();
        QCOMPARE(window.tree().size(), std::size_t(3));
    }

    void resetRestoresSampleTree()
    {
        MainWindow window;
        window.setConfirmationsEnabled(false);
        window.insertKey(45);
        window.deleteKey(70);
        window.updateKey(20, 1);
        window.clearTree();
        QVERIFY(window.tree().empty());
        window.resetToSample();
        QCOMPARE(window.tree().snapshot().preorderKeys(), (std::vector<int>{50, 30, 20, 40, 70, 60, 80}));
    }

    void historyReplayShowsRecordedState()
    {
        MainWindow window;
        window.setConfirmationsEnabled(false);
        window.insertKey(45);
        window.clearTree();
        // Replay the insertion recorded before the tree was cleared.
        const auto insertRecord = window.history()->recordAt(1);
        QCOMPARE(insertRecord->result.operation, OperationType::Insert);
        window.controller()->load(insertRecord, false);
        window.controller()->jumpTo(window.controller()->stepCount() - 1);
        QCOMPARE(window.canvas()->nodeCount(), 8); // as recorded, independent of the live (empty) tree
        QVERIFY(window.tree().empty());
    }

    void closingWhilePlayingIsSafe()
    {
        auto* window = new MainWindow;
        window->show();
        window->controller()->setSpeed(8.0);
        window->runTraversal(TraversalOrder::Inorder);
        QVERIFY(window->controller()->isPlaying());
        QTest::qWait(50);
        delete window; // timers and animations must stop cleanly
        QTest::qWait(300);
    }
};

QTEST_MAIN(TestVisualization)
#include "tst_visualization.moc"
