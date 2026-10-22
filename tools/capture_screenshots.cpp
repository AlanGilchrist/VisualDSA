// Regenerates the PNG screenshots in screenshots/ by driving the real
// MainWindow through a fixed demonstration script and grabbing the window.
//
//   capture_screenshots <output-directory>
//
// Every state shown comes from a real operation on the real tree; the tool
// only chooses which recorded step to display.

#include "AnimationController.h"
#include "MainWindow.h"
#include "Theme.h"
#include "TreeCanvas.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QTextStream>

#include <functional>

using namespace vdsa;

namespace {

void settle(int ms)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < ms)
        QApplication::processEvents(QEventLoop::AllEvents, 20);
}

// Shows the first step of the current record that matches `predicate`
// (or the last step if none does), waits for the transition, then saves.
void capture(MainWindow& window, const QDir& dir, const QString& name,
             const std::function<bool(const AlgorithmEvent&)>& predicate)
{
    AnimationController* controller = window.controller();
    controller->pause();
    int target = controller->stepCount() - 1;
    if (predicate && controller->record()) {
        const auto& events = controller->record()->events;
        for (int i = 0; i < static_cast<int>(events.size()); ++i) {
            if (predicate(events[static_cast<std::size_t>(i)])) {
                target = i;
                break;
            }
        }
    }
    controller->jumpTo(target);
    settle(700);
    const QString path = dir.filePath(name);
    window.grab().save(path);
    QTextStream(stdout) << "saved " << path << '\n';
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    applyTheme(app);
    const QDir dir(argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("screenshots"));
    QDir().mkpath(dir.absolutePath());

    MainWindow window;
    window.setConfirmationsEnabled(false);
    window.resize(1440, 900);
    window.show();
    settle(800);

    auto is = [](EventType type) { return [type](const AlgorithmEvent& e) { return e.type == type; }; };

    window.selectModule(Module::Overview);
    settle(300);
    window.grab().save(dir.filePath(QStringLiteral("01-overview.png")));

    window.selectModule(Module::Insert);
    window.insertKey(45);
    capture(window, dir, QStringLiteral("02-insert-empty-slot.png"), [](const AlgorithmEvent& e) {
        return e.type == EventType::MoveRight && e.pendingParent != kNoNode;
    });
    capture(window, dir, QStringLiteral("03-insert-completed.png"), is(EventType::OperationCompleted));

    window.resetToSample();
    window.selectModule(Module::Search);
    window.searchKey(60);
    capture(window, dir, QStringLiteral("04-search-compare.png"), [](const AlgorithmEvent& e) {
        return e.type == EventType::KeyCompared && e.comparedKey == 70;
    });
    capture(window, dir, QStringLiteral("05-search-found.png"), is(EventType::NodeFound));
    window.searchKey(65);
    capture(window, dir, QStringLiteral("06-search-not-found.png"), is(EventType::NodeNotFound));

    window.selectModule(Module::Delete);
    window.deleteKey(50);
    capture(window, dir, QStringLiteral("07-delete-two-children-case.png"), is(EventType::DeletionCaseIdentified));
    capture(window, dir, QStringLiteral("08-delete-successor-found.png"), is(EventType::SuccessorFound));
    capture(window, dir, QStringLiteral("09-delete-completed.png"), is(EventType::OperationCompleted));

    window.resetToSample();
    window.selectModule(Module::Update);
    window.updateKey(40, 65);
    capture(window, dir, QStringLiteral("10-update-transaction.png"), is(EventType::TransactionStarted));
    capture(window, dir, QStringLiteral("11-update-reinsert.png"), [](const AlgorithmEvent& e) {
        return e.line == PseudoLine::UpdInsert && e.pendingParent != kNoNode;
    });
    capture(window, dir, QStringLiteral("12-update-committed.png"), is(EventType::TransactionCommitted));

    window.resetToSample();
    window.selectModule(Module::Traversals);
    window.runTraversal(TraversalOrder::Inorder);
    capture(window, dir, QStringLiteral("13-inorder-midway.png"),
            [](const AlgorithmEvent& e) { return e.type == EventType::TraversalNodeVisited && e.sequence.size() == 4; });
    window.runTraversal(TraversalOrder::Postorder);
    capture(window, dir, QStringLiteral("14-postorder-midway.png"),
            [](const AlgorithmEvent& e) { return e.type == EventType::TraversalNodeVisited && e.sequence.size() == 4; });

    window.selectModule(Module::Reference);
    settle(300);
    window.grab().save(dir.filePath(QStringLiteral("15-algorithm-reference.png")));
    return 0;
}
