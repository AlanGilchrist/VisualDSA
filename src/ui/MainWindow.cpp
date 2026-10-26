#include "MainWindow.h"

#include "AnimationController.h"
#include "ComplexityPanel.h"
#include "ExplanationPanel.h"
#include "HistoryPanel.h"
#include "Icons.h"
#include "Palette.h"
#include "PlaybackPanel.h"
#include "PseudocodePanel.h"
#include "TreeCanvas.h"
#include "Validation.h"

#include <QButtonGroup>
#include <QCloseEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <stdexcept>

namespace vdsa {

namespace {

struct ModuleInfo {
    Module module;
    const char* name;
    const char* group;
    const char* description;
};

const ModuleInfo kModules[] = {
    {Module::Overview, "Overview", "WORKSPACE",
     "Build a tree from your own keys or load the sample tree, then choose an operation."},
    {Module::Insert, "Insert", "OPERATIONS",
     "Follow the search path from the root and attach the new key at the first empty position."},
    {Module::Delete, "Delete", "OPERATIONS",
     "Locate the key, identify which of the three deletion cases applies, and restructure the tree."},
    {Module::Search, "Search", "OPERATIONS",
     "Compare with each node and discard one subtree per step until the key is found or a null pointer is "
     "reached."},
    {Module::Traversals, "Depth-first (DFS)", "TRAVERSALS",
     "Visit every node depth-first in inorder, preorder or postorder and watch the output sequence form."},
    {Module::Bfs, "Breadth-first (BFS)", "TRAVERSALS",
     "Visit every node level by level with a queue: the level-order traversal."},
};

const ModuleInfo& infoFor(Module module) { return kModules[static_cast<int>(module)]; }

OperationType defaultOperation(Module module)
{
    switch (module) {
    case Module::Delete: return OperationType::Delete;
    case Module::Search: return OperationType::Search;
    case Module::Traversals: return OperationType::InorderTraversal;
    case Module::Bfs: return OperationType::LevelOrderTraversal;
    case Module::Overview: return OperationType::Build;
    case Module::Insert: break;
    }
    return OperationType::Insert;
}

QLabel* sectionLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setProperty("role", "section");
    return label;
}

QFrame* hairline(QWidget* parent)
{
    auto* line = new QFrame(parent);
    line->setObjectName(QStringLiteral("Hairline"));
    line->setFixedHeight(1);
    return line;
}

void repolish(QWidget* w)
{
    w->style()->unpolish(w);
    w->style()->polish(w);
}

const char* toneFor(ResultStatus status)
{
    switch (status) {
    case ResultStatus::Success: return "success";
    case ResultStatus::NotFound:
    case ResultStatus::Rejected: break;
    }
    return "error";
}

QString progressText(const OperationResult& r)
{
    switch (r.operation) {
    case OperationType::Insert: return QObject::tr("Inserting %1…").arg(r.key);
    case OperationType::Delete: return QObject::tr("Deleting %1…").arg(r.key);
    case OperationType::Search: return QObject::tr("Searching for %1…").arg(r.key);
    case OperationType::InorderTraversal: return QObject::tr("Inorder traversal…");
    case OperationType::PreorderTraversal: return QObject::tr("Preorder traversal…");
    case OperationType::PostorderTraversal: return QObject::tr("Postorder traversal…");
    case OperationType::LevelOrderTraversal: return QObject::tr("Level-order traversal (BFS)…");
    case OperationType::Build: return QObject::tr("Building tree…");
    case OperationType::Clear: return QObject::tr("Clearing tree…");
    }
    return {};
}

std::vector<int> preorderOf(const std::vector<int>& insertionOrder)
{
    BinarySearchTree tree;
    tree.rebuild(insertionOrder);
    return tree.snapshot().preorderKeys();
}

} // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle(tr("VisualDSA — Binary Search Tree Visualizer"));
    setMinimumSize(1120, 720);
    resize(1440, 900);
    tree_.setCapacity(kMaxTreeSize);

    controller_ = new AnimationController(this);

    auto* root = new QWidget(this);
    root->setObjectName(QStringLiteral("Root"));
    auto* body = new QHBoxLayout;
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(0);
    body->addWidget(buildSidebar());
    body->addWidget(buildWorkspace(), 1);
    body->addWidget(buildRightPanel());

    playback_ = new PlaybackPanel(root);

    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(buildTopBar());
    layout->addLayout(body, 1);
    layout->addWidget(playback_);
    setCentralWidget(root);

    connectSignals();
    installShortcuts();

    // Start with the deterministic demonstration tree.
    auto initial = std::make_shared<const OperationRecord>(tree_.rebuild(BinarySearchTree::sampleKeys()));
    history_->addRecord(initial);
    canvas_->showTree(tree_.snapshot());
    refreshStats(tree_.snapshot());
    refreshNodeInfo();
    selectModule(Module::Overview);
    setStatus(tr("Sample tree loaded: 50, 30, 70, 20, 40, 60, 80."), "neutral");
}

MainWindow::~MainWindow()
{
    controller_->pause(); // stop timers before child widgets are destroyed
}

QString MainWindow::statusText() const { return status_->text(); }

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

QWidget* MainWindow::buildTopBar()
{
    auto* bar = new QFrame(this);
    bar->setObjectName(QStringLiteral("TopBar"));
    bar->setFixedHeight(56);

    auto* wordmark = new QLabel(QStringLiteral("Visual<span style='color:%1'>DSA</span>").arg(palette::accent.name()), bar);
    wordmark->setObjectName(QStringLiteral("Wordmark"));
    wordmark->setTextFormat(Qt::RichText);
    auto* divider = new QFrame(bar);
    divider->setObjectName(QStringLiteral("VerticalRule"));
    divider->setFixedSize(1, 18);
    auto* subtitle = new QLabel(tr("Binary Search Tree · Operations & Traversal Visualizer"), bar);
    subtitle->setProperty("role", "secondary");

    auto* reset = new QPushButton(tr("Reset"), bar);
    reset->setToolTip(tr("Restore the sample tree 50, 30, 70, 20, 40, 60, 80"));
    auto* help = new QPushButton(tr("Help"), bar);
    help->setToolTip(tr("Controls and keyboard shortcuts (F1)"));
    auto* about = new QPushButton(tr("About"), bar);
    for (QPushButton* b : {reset, help, about}) {
        b->setProperty("variant", "ghost");
        b->setCursor(Qt::PointingHandCursor);
    }

    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(22, 0, 16, 0);
    layout->setSpacing(14);
    layout->addWidget(wordmark);
    layout->addWidget(divider);
    layout->addWidget(subtitle);
    layout->addStretch();
    layout->addWidget(reset);
    layout->addWidget(help);
    layout->addWidget(about);

    connect(reset, &QPushButton::clicked, this, &MainWindow::resetToSample);
    connect(help, &QPushButton::clicked, this, &MainWindow::showHelp);
    connect(about, &QPushButton::clicked, this, &MainWindow::showAbout);
    return bar;
}

QWidget* MainWindow::buildSidebar()
{
    auto* sidebar = new QFrame(this);
    sidebar->setObjectName(QStringLiteral("Sidebar"));
    sidebar->setFixedWidth(208);

    auto* layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(0, 18, 0, 16);
    layout->setSpacing(2);

    navigation_ = new QButtonGroup(this);
    navigation_->setExclusive(true);
    QString currentGroup;
    for (const ModuleInfo& info : kModules) {
        if (currentGroup != QLatin1String(info.group)) {
            currentGroup = QString::fromLatin1(info.group);
            if (layout->count() > 0)
                layout->addSpacing(14);
            auto* label = sectionLabel(currentGroup, sidebar);
            label->setContentsMargins(20, 0, 0, 6);
            layout->addWidget(label);
        }
        auto* button = new QPushButton(tr(info.name), sidebar);
        QString objectName = QStringLiteral("Nav");
        for (const QChar c : QString::fromLatin1(info.name))
            if (c.isLetter())
                objectName += c;
        button->setObjectName(objectName); // e.g. NavBreadthfirstBFS
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        navigation_->addButton(button, static_cast<int>(info.module));
        layout->addWidget(button);
    }
    layout->addStretch();

    auto* footer = new QLabel(tr("C++17 · Qt 6 Widgets\nKeys %1 to %2 · up to %3 nodes")
                                  .arg(kMinKey)
                                  .arg(kMaxKey)
                                  .arg(kMaxTreeSize),
                              sidebar);
    footer->setProperty("role", "note");
    footer->setContentsMargins(20, 0, 12, 0);
    layout->addWidget(footer);

    connect(navigation_, &QButtonGroup::idClicked, this,
            [this](int id) { selectModule(static_cast<Module>(id)); });
    return sidebar;
}

QWidget* MainWindow::buildWorkspace()
{
    auto* workspace = new QWidget(this);
    workspace->setObjectName(QStringLiteral("Workspace"));

    moduleTitle_ = new QLabel(workspace);
    moduleTitle_->setObjectName(QStringLiteral("ModuleTitle"));
    moduleDescription_ = new QLabel(workspace);
    moduleDescription_->setProperty("role", "secondary");
    moduleDescription_->setWordWrap(true);

    // Stat strip: root, size, height, active node, status.
    auto stat = [workspace](const QString& name, QLabel*& value) {
        auto* box = new QWidget(workspace);
        auto* l = new QHBoxLayout(box);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(7);
        l->addWidget(sectionLabel(name, box));
        value = new QLabel(box);
        value->setObjectName(QStringLiteral("StatValue"));
        value->setFont(palette::monoFont(10, QFont::Medium));
        l->addWidget(value);
        return box;
    };
    auto* stats = new QHBoxLayout;
    stats->setSpacing(22);
    stats->addWidget(stat(tr("ROOT"), rootValue_));
    stats->addWidget(stat(tr("NODES"), sizeValue_));
    stats->addWidget(stat(tr("HEIGHT"), heightValue_));
    stats->addWidget(stat(tr("NODE"), nodeInfo_));
    nodeInfo_->setFont(palette::monoFont(9.5));
    stats->addStretch();
    status_ = new QLabel(workspace);
    status_->setObjectName(QStringLiteral("Status"));
    stats->addWidget(status_);

    // Canvas with a zoom toolbar overlaid in its top-right corner.
    auto* canvasFrame = new QFrame(workspace);
    canvasFrame->setObjectName(QStringLiteral("CanvasFrame"));
    canvas_ = new TreeCanvas(canvasFrame);

    auto* toolbar = new QFrame(canvasFrame);
    toolbar->setObjectName(QStringLiteral("CanvasToolbar"));
    auto* tl = new QHBoxLayout(toolbar);
    tl->setContentsMargins(4, 4, 4, 4);
    tl->setSpacing(2);
    struct Tool {
        icons::Glyph glyph;
        QString tip;
        void (TreeCanvas::*slot)();
    };
    const Tool tools[] = {
        {icons::Glyph::ZoomOut, tr("Zoom out (Ctrl+-)"), &TreeCanvas::zoomOut},
        {icons::Glyph::ZoomIn, tr("Zoom in (Ctrl+=)"), &TreeCanvas::zoomIn},
        {icons::Glyph::OneToOne, tr("Actual size (100%)"), &TreeCanvas::resetZoom},
    };
    for (const Tool& tool : tools) {
        auto* b = new QToolButton(toolbar);
        b->setIcon(icons::make(tool.glyph, palette::textSecondary, 16));
        b->setToolTip(tool.tip);
        b->setAccessibleName(tool.tip);
        b->setFixedSize(28, 26);
        connect(b, &QToolButton::clicked, canvas_, tool.slot);
        tl->addWidget(b);
    }
    auto* fit = new QToolButton(toolbar);
    fit->setObjectName(QStringLiteral("FitButton"));
    fit->setIcon(icons::make(icons::Glyph::Fit, palette::textPrimary, 16));
    fit->setText(tr("Fit"));
    fit->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    fit->setToolTip(tr("Fit the tree to the canvas (Ctrl+0, or double-click empty space)"));
    fit->setFixedHeight(26);
    connect(fit, &QToolButton::clicked, canvas_, &TreeCanvas::fitToView);
    tl->addWidget(fit);

    auto* overlay = new QGridLayout(canvasFrame);
    overlay->setContentsMargins(1, 1, 1, 1);
    overlay->addWidget(canvas_, 0, 0);
    overlay->addWidget(toolbar, 0, 0, Qt::AlignTop | Qt::AlignRight);

    auto* layout = new QVBoxLayout(workspace);
    layout->setContentsMargins(28, 22, 28, 18);
    layout->setSpacing(0);
    layout->addWidget(moduleTitle_);
    layout->addSpacing(4);
    layout->addWidget(moduleDescription_);
    layout->addSpacing(16);
    layout->addLayout(stats);
    layout->addSpacing(12);
    layout->addWidget(canvasFrame, 1);
    return workspace;
}

QWidget* MainWindow::buildRightPanel()
{
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("RightPanel"));
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFixedWidth(384);

    auto* content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("RightPanelContent"));
    operations_ = new OperationPanel(content);
    pseudocode_ = new PseudocodePanel(content);
    explanation_ = new ExplanationPanel(content);
    complexity_ = new ComplexityPanel(content);
    history_ = new HistoryPanel(content);

    pseudocodeScroll_ = new QScrollArea(content);
    pseudocodeScroll_->setObjectName(QStringLiteral("PseudocodeScroll"));
    pseudocodeScroll_->setWidget(pseudocode_);
    pseudocodeScroll_->setWidgetResizable(true);
    pseudocodeScroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    pseudocodeTitle_ = sectionLabel(tr("PSEUDOCODE"), content);

    auto* layout = new QVBoxLayout(content);
    layout->setContentsMargins(22, 20, 22, 24);
    layout->setSpacing(0);
    auto addSection = [&](QLabel* title, QWidget* body, bool last = false) {
        layout->addWidget(title);
        layout->addSpacing(10);
        layout->addWidget(body);
        if (!last) {
            layout->addSpacing(18);
            layout->addWidget(hairline(content));
            layout->addSpacing(16);
        }
    };
    addSection(sectionLabel(tr("OPERATION"), content), operations_);
    addSection(sectionLabel(tr("CURRENT STEP"), content), explanation_);
    addSection(pseudocodeTitle_, pseudocodeScroll_);
    addSection(sectionLabel(tr("COMPLEXITY"), content), complexity_);
    addSection(sectionLabel(tr("HISTORY"), content), history_, true);
    layout->addStretch();

    scroll->setWidget(content);
    return scroll;
}

void MainWindow::connectSignals()
{
    connect(operations_, &OperationPanel::insertRequested, this, &MainWindow::insertKey);
    connect(operations_, &OperationPanel::deleteRequested, this, &MainWindow::deleteKey);
    connect(operations_, &OperationPanel::searchRequested, this, &MainWindow::searchKey);
    connect(operations_, &OperationPanel::traversalRequested, this, &MainWindow::runTraversal);
    connect(operations_, &OperationPanel::buildRequested, this, &MainWindow::buildTree);
    connect(operations_, &OperationPanel::clearRequested, this, &MainWindow::clearTree);
    connect(operations_, &OperationPanel::sampleRequested, this, &MainWindow::loadSampleTree);

    connect(playback_, &PlaybackPanel::previousRequested, controller_, &AnimationController::previous);
    connect(playback_, &PlaybackPanel::nextRequested, controller_, &AnimationController::next);
    connect(playback_, &PlaybackPanel::playPauseRequested, controller_, &AnimationController::togglePlayPause);
    connect(playback_, &PlaybackPanel::restartRequested, controller_, &AnimationController::restart);
    connect(playback_, &PlaybackPanel::seekRequested, controller_, &AnimationController::jumpTo);
    connect(playback_, &PlaybackPanel::speedRequested, controller_, &AnimationController::setSpeed);

    connect(controller_, &AnimationController::recordChanged, this, &MainWindow::onRecordChanged);
    connect(controller_, &AnimationController::stepChanged, this, &MainWindow::onStepChanged);
    connect(controller_, &AnimationController::playingChanged, this, [this](bool playing) {
        playback_->setState(controller_->currentStep(), controller_->stepCount(), playing);
    });
    connect(controller_, &AnimationController::speedChanged, playback_, &PlaybackPanel::setSpeed);

    connect(pseudocode_, &PseudocodePanel::activeRowChanged, this, [this](int row) {
        if (row >= 0) {
            const QRect r = pseudocode_->rowRect(row);
            pseudocodeScroll_->ensureVisible(r.center().x(), r.center().y(), 0, r.height());
        }
    });
    connect(canvas_, &TreeCanvas::nodeSelected, this, [this](NodeId) { refreshNodeInfo(); });
    connect(history_, &HistoryPanel::replayRequested, this,
            [this](std::shared_ptr<const OperationRecord> record) { present(std::move(record), true); });
}

void MainWindow::installShortcuts()
{
    auto add = [this](const QKeySequence& keys, QWidget* scope, Qt::ShortcutContext context, auto slot) {
        auto* shortcut = new QShortcut(keys, scope);
        shortcut->setContext(context);
        connect(shortcut, &QShortcut::activated, this, slot);
    };
    add(QKeySequence(Qt::CTRL | Qt::Key_Space), this, Qt::WindowShortcut, [this] { controller_->togglePlayPause(); });
    add(QKeySequence(Qt::CTRL | Qt::Key_Right), this, Qt::WindowShortcut, [this] { controller_->next(); });
    add(QKeySequence(Qt::CTRL | Qt::Key_Left), this, Qt::WindowShortcut, [this] { controller_->previous(); });
    add(QKeySequence(Qt::CTRL | Qt::Key_0), this, Qt::WindowShortcut, [this] { canvas_->fitToView(); });
    add(QKeySequence(Qt::CTRL | Qt::Key_Equal), this, Qt::WindowShortcut, [this] { canvas_->zoomIn(); });
    add(QKeySequence(Qt::CTRL | Qt::Key_Minus), this, Qt::WindowShortcut, [this] { canvas_->zoomOut(); });
    add(QKeySequence(Qt::Key_F1), this, Qt::WindowShortcut, [this] { showHelp(); });
    // Bare keys only while the canvas has focus, so typing in inputs is unaffected.
    add(QKeySequence(Qt::Key_Space), canvas_, Qt::WidgetShortcut, [this] { controller_->togglePlayPause(); });
    add(QKeySequence(Qt::Key_Right), canvas_, Qt::WidgetShortcut, [this] { controller_->next(); });
    add(QKeySequence(Qt::Key_Left), canvas_, Qt::WidgetShortcut, [this] { controller_->previous(); });
}

// ---------------------------------------------------------------------------
// Module selection
// ---------------------------------------------------------------------------

void MainWindow::selectModule(Module module)
{
    module_ = module;
    const ModuleInfo& info = infoFor(module);
    if (QAbstractButton* button = navigation_->button(static_cast<int>(module)))
        button->setChecked(true);
    moduleTitle_->setText(tr(info.name));
    moduleDescription_->setText(tr(info.description));
    operations_->setModule(module);

    // With nothing recorded yet, preview the selected operation's pseudocode.
    if (!controller_->hasRecord()) {
        pseudocode_->setOperation(defaultOperation(module));
        complexity_->setOperation(defaultOperation(module));
        pseudocodeTitle_->setText(tr("PSEUDOCODE  ·  %1")
                                      .arg(QString::fromLatin1(operationName(defaultOperation(module))).toUpper()));
        fitPseudocodeHeight();
    }
}

// ---------------------------------------------------------------------------
// Operations — every one goes through the tree's public API
// ---------------------------------------------------------------------------

void MainWindow::insertKey(int key)
{
    runOperation([this, key] { return tree_.insert(key); });
}

void MainWindow::deleteKey(int key)
{
    runOperation([this, key] { return tree_.remove(key); });
}

void MainWindow::searchKey(int key)
{
    runOperation([this, key] { return tree_.search(key); });
}

void MainWindow::runTraversal(TraversalOrder order)
{
    runOperation([this, order] { return tree_.traverse(order); });
}

void MainWindow::buildTree(const std::vector<int>& keys)
{
    if (!confirmReplace(tr("Creating a tree from your keys"), keys))
        return;
    runOperation([this, keys] { return tree_.rebuild(keys); });
}

void MainWindow::clearTree()
{
    if (!confirmReplace(tr("Clearing"), {}))
        return;
    runOperation([this] { return tree_.clearTree(); });
}

void MainWindow::loadSampleTree()
{
    if (!confirmReplace(tr("Loading the sample tree"), BinarySearchTree::sampleKeys()))
        return;
    runOperation([this] { return tree_.rebuild(BinarySearchTree::sampleKeys()); });
}

void MainWindow::resetToSample()
{
    if (!confirmReplace(tr("Reset"), BinarySearchTree::sampleKeys()))
        return;
    runOperation([this] { return tree_.rebuild(BinarySearchTree::sampleKeys()); });
    canvas_->fitToView();
}

void MainWindow::runOperation(const std::function<OperationRecord()>& operation)
{
    std::shared_ptr<const OperationRecord> record;
    try {
        record = std::make_shared<const OperationRecord>(operation());
    } catch (const std::logic_error& e) {
        // Only raised when an internal invariant check fails, i.e. a bug.
        setStatus(tr("Internal error: operation aborted."), "error");
        QMessageBox::critical(this, tr("Internal error"),
                              tr("An internal consistency check failed:\n\n%1").arg(QString::fromUtf8(e.what())));
        return;
    }
    history_->addRecord(record);
    present(std::move(record), false);
}

void MainWindow::present(std::shared_ptr<const OperationRecord> record, bool replay)
{
    replaying_ = replay;
    controller_->load(std::move(record), true); // cancels any playback in progress
}

bool MainWindow::confirmReplace(const QString& action, const std::vector<int>& newKeys)
{
    if (!confirmations_ || tree_.empty())
        return true;
    if (tree_.snapshot().preorderKeys() == preorderOf(newKeys))
        return true; // the result would be identical; nothing is lost
    QMessageBox box(QMessageBox::Question, tr("Replace the current tree?"),
                    tr("%1 will replace the current tree (%2 nodes). Continue?").arg(action).arg(tree_.size()),
                    QMessageBox::Yes | QMessageBox::Cancel, this);
    box.setDefaultButton(QMessageBox::Cancel);
    return box.exec() == QMessageBox::Yes;
}

// ---------------------------------------------------------------------------
// Playback synchronisation
// ---------------------------------------------------------------------------

int MainWindow::transitionDuration() const
{
    return std::min(460, static_cast<int>(controller_->stepIntervalMs() * 0.6));
}

void MainWindow::fitPseudocodeHeight()
{
    pseudocode_->adjustSize();
    pseudocodeScroll_->setFixedHeight(std::min(pseudocode_->sizeHint().height() + 2, 300));
}

void MainWindow::onRecordChanged()
{
    const auto record = controller_->record();
    if (!record)
        return;
    const OperationType op = record->result.operation;
    pseudocode_->setOperation(op);
    complexity_->setOperation(op);
    pseudocodeTitle_->setText(tr("PSEUDOCODE  ·  %1").arg(QString::fromLatin1(operationName(op)).toUpper()));
    fitPseudocodeHeight();
}

void MainWindow::onStepChanged(int step, int count)
{
    const AlgorithmEvent* event = controller_->currentEvent();
    const TreeSnapshot* snapshot = controller_->currentSnapshot();
    if (!event || !snapshot) {
        canvas_->showTree(tree_.snapshot(), transitionDuration());
        explanation_->clear();
        pseudocode_->setActiveLine(PseudoLine::None);
        complexity_->setMeasurements(nullptr, static_cast<int>(tree_.size()), tree_.height());
        playback_->setState(0, 0, false);
        refreshStats(tree_.snapshot());
        refreshNodeInfo();
        return;
    }

    canvas_->showStep(*snapshot, *event, transitionDuration());
    pseudocode_->setActiveLine(event->line);
    explanation_->setEvent(*event, *snapshot, step, count);
    complexity_->setMeasurements(&event->metrics, snapshot->size(), snapshot->height());
    playback_->setState(step, count, controller_->isPlaying());
    refreshStats(*snapshot);
    refreshNodeInfo();

    const OperationResult& result = controller_->record()->result;
    const QString prefix = replaying_ ? tr("Replay · ") : QString();
    if (step == count - 1)
        setStatus(prefix + QString::fromStdString(result.message), toneFor(result.status));
    else
        setStatus(prefix + progressText(result), "active");
}

void MainWindow::refreshStats(const TreeSnapshot& snapshot)
{
    const SnapshotNode* root = snapshot.find(snapshot.root);
    rootValue_->setText(root ? QString::number(root->key) : QStringLiteral("null"));
    sizeValue_->setText(QString::number(snapshot.size()));
    heightValue_->setText(snapshot.empty() ? QStringLiteral("—") : QString::number(snapshot.height()));
}

void MainWindow::refreshNodeInfo()
{
    const TreeSnapshot& snapshot = canvas_->snapshot();
    NodeId id = canvas_->selectedNode();
    QString prefix = tr("selected");
    if (id == kNoNode) {
        id = canvas_->highlightedNode();
        prefix = tr("active");
    }
    if (id == kNoNode) {
        nodeInfo_->setText(tr("— click a node to inspect"));
        return;
    }
    const SnapshotNode* node = snapshot.find(id);
    if (!node) {
        nodeInfo_->setText(tr("%1 node removed").arg(prefix));
        return;
    }
    const int children = (node->left != kNoNode) + (node->right != kNoNode);
    const SnapshotNode* parent = snapshot.find(node->parent);
    QString text = tr("%1 %2 · depth %3 · ").arg(prefix).arg(node->key).arg(node->depth);
    text += children == 0 ? tr("leaf") : (children == 1 ? tr("1 child") : tr("2 children"));
    text += parent ? tr(" · parent %1").arg(parent->key) : tr(" · root");
    nodeInfo_->setText(text);
}

void MainWindow::setStatus(const QString& text, const char* tone)
{
    status_->setText(text);
    status_->setProperty("tone", tone);
    repolish(status_);
}

// ---------------------------------------------------------------------------
// Dialogs and lifetime
// ---------------------------------------------------------------------------

void MainWindow::showHelp()
{
    QMessageBox::information(
        this, tr("Using VisualDSA"),
        tr("<p><b>Run an operation</b><br>Pick an operation in the sidebar, enter a key in the right panel and "
           "press Enter or the button. The algorithm runs immediately on the C++ tree; the canvas then replays "
           "every recorded step.</p>"
           "<p><b>Playback</b><br>Previous / Play-Pause / Next / Restart, a speed selector (0.5× – 4×) and a "
           "timeline you can drag.</p>"
           "<p><b>Shortcuts</b><br>Ctrl+Space play/pause · Ctrl+Left / Ctrl+Right step · Ctrl+0 fit · "
           "Ctrl+= / Ctrl+- zoom · F1 help. With the canvas focused: Space, Left, Right.</p>"
           "<p><b>Canvas</b><br>Scroll to zoom, drag to pan, double-click empty space to fit, click a node to "
           "inspect it.</p>"
           "<p><b>History</b><br>Click a history entry to replay that operation's recorded steps.</p>"));
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("About VisualDSA"),
                       tr("<p><b>VisualDSA %1</b><br>Interactive Binary Search Tree Operations and Traversal "
                          "Visualizer.</p><p>A Data Structures and Algorithms mini project in C++17 with Qt 6 "
                          "Widgets. Every animation is a replay of events recorded while the real BST algorithm "
                          "ran.</p>")
                           .arg(QStringLiteral("1.0.0")));
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    controller_->pause();
    QMainWindow::closeEvent(event);
}

} // namespace vdsa
