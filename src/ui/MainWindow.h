#pragma once

#include "BinarySearchTree.h"
#include "OperationPanel.h"

#include <QMainWindow>

#include <functional>
#include <memory>
#include <vector>

class QButtonGroup;
class QLabel;
class QScrollArea;

namespace vdsa {

class AnimationController;
class ComplexityPanel;
class ExplanationPanel;
class HistoryPanel;
class PlaybackPanel;
class PseudocodePanel;
class TreeCanvas;

// Owns the live BinarySearchTree. User actions are validated by the
// OperationPanel, executed here through the tree's public operations, and
// the resulting OperationRecord is handed to the AnimationController for
// playback. Nothing in the UI mutates the tree in any other way.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // Accessors used by tests.
    const BinarySearchTree& tree() const { return tree_; }
    AnimationController* controller() const { return controller_; }
    TreeCanvas* canvas() const { return canvas_; }
    PseudocodePanel* pseudocode() const { return pseudocode_; }
    ExplanationPanel* explanation() const { return explanation_; }
    ComplexityPanel* complexity() const { return complexity_; }
    OperationPanel* operations() const { return operations_; }
    HistoryPanel* history() const { return history_; }
    Module currentModule() const { return module_; }
    QString statusText() const;
    // Disables modal confirmations (used by automated tests).
    void setConfirmationsEnabled(bool enabled) { confirmations_ = enabled; }

public slots:
    void selectModule(vdsa::Module module);
    void insertKey(int key);
    void deleteKey(int key);
    void searchKey(int key);
    void runTraversal(vdsa::TraversalOrder order);
    void buildTree(const std::vector<int>& keys);
    void clearTree();
    void loadSampleTree();
    void resetToSample();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    QWidget* buildTopBar();
    QWidget* buildSidebar();
    QWidget* buildWorkspace();
    QWidget* buildRightPanel();
    void connectSignals();
    void installShortcuts();

    void runOperation(const std::function<OperationRecord()>& operation);
    void present(std::shared_ptr<const OperationRecord> record, bool replay);
    void onRecordChanged();
    void onStepChanged(int step, int count);
    void refreshStats(const TreeSnapshot& snapshot);
    void refreshNodeInfo();
    void setStatus(const QString& text, const char* tone);
    bool confirmReplace(const QString& action, const std::vector<int>& newKeys);
    void showHelp();
    void showAbout();
    int transitionDuration() const;
    void fitPseudocodeHeight();

    BinarySearchTree tree_;
    AnimationController* controller_ = nullptr;

    QButtonGroup* navigation_ = nullptr;
    QLabel* moduleTitle_ = nullptr;
    QLabel* moduleDescription_ = nullptr;
    QLabel* rootValue_ = nullptr;
    QLabel* sizeValue_ = nullptr;
    QLabel* heightValue_ = nullptr;
    QLabel* nodeInfo_ = nullptr;
    QLabel* status_ = nullptr;
    QLabel* pseudocodeTitle_ = nullptr;
    QScrollArea* pseudocodeScroll_ = nullptr;

    TreeCanvas* canvas_ = nullptr;
    OperationPanel* operations_ = nullptr;
    PlaybackPanel* playback_ = nullptr;
    PseudocodePanel* pseudocode_ = nullptr;
    ExplanationPanel* explanation_ = nullptr;
    ComplexityPanel* complexity_ = nullptr;
    HistoryPanel* history_ = nullptr;

    Module module_ = Module::Overview;
    bool confirmations_ = true;
    bool replaying_ = false;
};

} // namespace vdsa
