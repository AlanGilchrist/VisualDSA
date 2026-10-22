#include "OperationPanel.h"

#include "Validation.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace vdsa {

namespace {

QLabel* description(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setWordWrap(true);
    label->setProperty("role", "secondary");
    return label;
}

QLineEdit* keyEdit(const QString& objectName, const QString& placeholder, QWidget* parent)
{
    auto* edit = new QLineEdit(parent);
    edit->setObjectName(objectName);
    edit->setPlaceholderText(placeholder);
    edit->setAccessibleName(placeholder);
    edit->setClearButtonEnabled(true);
    return edit;
}

QPushButton* button(const QString& text, const char* variant, QWidget* parent)
{
    auto* b = new QPushButton(text, parent);
    b->setProperty("variant", variant);
    b->setCursor(Qt::PointingHandCursor);
    return b;
}

void setInvalid(QLineEdit* edit, bool invalid)
{
    edit->setProperty("invalid", invalid);
    edit->style()->unpolish(edit);
    edit->style()->polish(edit);
}

QString keyRangeHint()
{
    return QObject::tr("Integer key (%1 to %2)").arg(kMinKey).arg(kMaxKey);
}

} // namespace

OperationPanel::OperationPanel(QWidget* parent) : QWidget(parent)
{
    pages_.push_back(buildOverviewPage());
    pages_.push_back(buildSingleKeyPage(
        QStringLiteral("InsertKey"),
        tr("Starting at the root, the key is compared with each node and moves left (smaller) or right "
           "(larger) until an empty position is found. Duplicates are rejected."),
        tr("Insert"), &OperationPanel::insertRequested));
    pages_.push_back(buildSingleKeyPage(
        QStringLiteral("DeleteKey"),
        tr("Try a leaf (20), a node with one child, and a node with two children (30 or 50) to see all "
           "three deletion cases."),
        tr("Delete"), &OperationPanel::deleteRequested));
    pages_.push_back(buildSingleKeyPage(
        QStringLiteral("SearchKey"),
        tr("Each comparison discards one subtree. Search for a present key (60) and an absent one (65)."),
        tr("Search"), &OperationPanel::searchRequested));
    pages_.push_back(buildUpdatePage());
    pages_.push_back(buildTraversalPage());
    pages_.push_back(buildReferencePage());

    error_ = new QLabel(this);
    error_->setObjectName(QStringLiteral("InputError"));
    error_->setWordWrap(true);
    error_->hide();

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    // Only the current page is visible, so hidden pages take no space.
    for (QWidget* page : pages_)
        layout->addWidget(page);
    layout->addWidget(error_);
    setModule(Module::Overview);
}

void OperationPanel::setModule(Module module)
{
    module_ = module;
    for (std::size_t i = 0; i < pages_.size(); ++i)
        pages_[i]->setVisible(i == static_cast<std::size_t>(module));
    clearError();
}

void OperationPanel::showError(const QString& message)
{
    error_->setText(message);
    error_->show();
}

void OperationPanel::clearError()
{
    error_->clear();
    error_->hide();
    for (QLineEdit* edit : findChildren<QLineEdit*>())
        setInvalid(edit, false);
}

QString OperationPanel::errorText() const { return error_->isVisibleTo(this) ? error_->text() : QString(); }

bool OperationPanel::readKey(QLineEdit* edit, int& value, const QString& fieldName)
{
    const KeyParseResult parsed = parseKey(edit->text().toStdString());
    if (!parsed.ok) {
        setInvalid(edit, true);
        QString message = QString::fromStdString(parsed.message);
        if (!fieldName.isEmpty())
            message = fieldName + QStringLiteral(": ") + message;
        showError(message);
        edit->setFocus();
        edit->selectAll();
        return false;
    }
    value = parsed.value;
    return true;
}

QWidget* OperationPanel::buildOverviewPage()
{
    auto* page = new QWidget(this);
    treeInput_ = keyEdit(QStringLiteral("TreeInput"), tr("e.g. 50, 30, 70, 20, 40, 60, 80"), page);

    auto* create = button(tr("Create tree"), "primary", page);
    create->setObjectName(QStringLiteral("CreateTreeButton"));
    auto* sample = button(tr("Load sample"), "secondary", page);
    sample->setObjectName(QStringLiteral("SampleTreeButton"));
    auto* clear = button(tr("Clear"), "secondary", page);
    clear->setObjectName(QStringLiteral("ClearTreeButton"));

    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(6);
    buttons->addWidget(create, 1);
    buttons->addWidget(sample);
    buttons->addWidget(clear);

    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addWidget(description(tr("Enter comma-separated integers. They are inserted in the given order, so "
                                     "the first key becomes the root. Keys must be unique."),
                                  page));
    layout->addWidget(treeInput_);
    layout->addLayout(buttons);

    auto submit = [this] {
        clearError();
        const KeyListParseResult parsed = parseKeyList(treeInput_->text().toStdString());
        if (!parsed.ok) {
            setInvalid(treeInput_, true);
            showError(QString::fromStdString(parsed.message));
            return;
        }
        emit buildRequested(parsed.keys);
    };
    connect(create, &QPushButton::clicked, this, submit);
    connect(treeInput_, &QLineEdit::returnPressed, this, submit);
    connect(sample, &QPushButton::clicked, this, [this] {
        clearError();
        emit sampleRequested();
    });
    connect(clear, &QPushButton::clicked, this, [this] {
        clearError();
        emit clearRequested();
    });
    return page;
}

QWidget* OperationPanel::buildSingleKeyPage(const QString& objectName, const QString& text,
                                            const QString& buttonText, void (OperationPanel::*signal)(int))
{
    auto* page = new QWidget(this);
    auto* edit = keyEdit(objectName, keyRangeHint(), page);
    auto* run = button(buttonText, "primary", page);
    run->setObjectName(objectName + QStringLiteral("Button"));

    auto* row = new QHBoxLayout;
    row->setSpacing(6);
    row->addWidget(edit, 1);
    row->addWidget(run);

    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addWidget(description(text, page));
    layout->addLayout(row);

    auto submit = [this, edit, signal] {
        clearError();
        int key = 0;
        if (!readKey(edit, key, QString()))
            return;
        edit->selectAll();
        emit(this->*signal)(key);
    };
    connect(run, &QPushButton::clicked, this, submit);
    connect(edit, &QLineEdit::returnPressed, this, submit);
    return page;
}

QWidget* OperationPanel::buildUpdatePage()
{
    auto* page = new QWidget(this);
    oldKey_ = keyEdit(QStringLiteral("UpdateOldKey"), tr("Existing key"), page);
    newKey_ = keyEdit(QStringLiteral("UpdateNewKey"), tr("New key"), page);
    auto* run = button(tr("Update"), "primary", page);
    run->setObjectName(QStringLiteral("UpdateButton"));

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(6);
    grid->addWidget(oldKey_, 0, 0);
    grid->addWidget(newKey_, 0, 1);
    grid->addWidget(run, 1, 0, 1, 2);

    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addWidget(description(tr("A key cannot simply be overwritten: its position depends on its value. "
                                     "The update deletes the old key and inserts the new one inside a "
                                     "transaction, so the tree is never left invalid."),
                                  page));
    layout->addLayout(grid);

    auto submit = [this] {
        clearError();
        int oldKey = 0;
        int newKey = 0;
        if (!readKey(oldKey_, oldKey, tr("Existing key")) || !readKey(newKey_, newKey, tr("New key")))
            return;
        emit updateRequested(oldKey, newKey);
    };
    connect(run, &QPushButton::clicked, this, submit);
    connect(oldKey_, &QLineEdit::returnPressed, newKey_, [this] { newKey_->setFocus(); });
    connect(newKey_, &QLineEdit::returnPressed, this, submit);
    return page;
}

QWidget* OperationPanel::buildTraversalPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addWidget(description(tr("Every node is visited exactly once. Only the moment a node is output "
                                     "differs between the three orders."),
                                  page));

    struct Item {
        TraversalOrder order;
        QString name;
        QString rule;
        const char* objectName;
    };
    const Item items[] = {
        {TraversalOrder::Inorder, tr("Inorder"), tr("Left · Root · Right"), "InorderButton"},
        {TraversalOrder::Preorder, tr("Preorder"), tr("Root · Left · Right"), "PreorderButton"},
        {TraversalOrder::Postorder, tr("Postorder"), tr("Left · Right · Root"), "PostorderButton"},
    };
    for (const Item& item : items) {
        auto* b = button(QStringLiteral("%1      %2").arg(item.name, item.rule), "traversal", page);
        b->setObjectName(QString::fromLatin1(item.objectName));
        const TraversalOrder order = item.order;
        connect(b, &QPushButton::clicked, this, [this, order] {
            clearError();
            emit traversalRequested(order);
        });
        layout->addWidget(b);
    }
    return page;
}

QWidget* OperationPanel::buildReferencePage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(description(tr("The workspace shows pseudocode, complexity and terminology for every "
                                     "operation. Choose an operation in the sidebar to return to the canvas."),
                                  page));
    return page;
}

} // namespace vdsa
