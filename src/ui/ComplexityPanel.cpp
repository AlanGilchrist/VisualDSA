#include "ComplexityPanel.h"

#include "Palette.h"
#include "Pseudocode.h"

#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace vdsa {

namespace {

QLabel* valueLabel(QWidget* parent, const char* objectName)
{
    auto* label = new QLabel(parent);
    label->setObjectName(QString::fromLatin1(objectName));
    label->setFont(palette::monoFont(10, QFont::Medium));
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    return label;
}

QLabel* keyLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setProperty("role", "secondary");
    return label;
}

QLabel* subheading(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setProperty("role", "subsection");
    return label;
}

} // namespace

ComplexityPanel::ComplexityPanel(QWidget* parent) : QWidget(parent)
{
    best_ = valueLabel(this, "ComplexityBest");
    average_ = valueLabel(this, "ComplexityAverage");
    worst_ = valueLabel(this, "ComplexityWorst");
    space_ = valueLabel(this, "ComplexitySpace");
    space_->setWordWrap(true);
    note_ = new QLabel(this);
    note_->setWordWrap(true);
    note_->setProperty("role", "note");

    visited_ = valueLabel(this, "MeasuredVisited");
    comparisons_ = valueLabel(this, "MeasuredComparisons");
    traversal_ = valueLabel(this, "MeasuredTraversal");
    changes_ = valueLabel(this, "MeasuredChanges");
    size_ = valueLabel(this, "MeasuredSize");
    height_ = valueLabel(this, "MeasuredHeight");

    auto* theory = new QGridLayout;
    theory->setHorizontalSpacing(12);
    theory->setVerticalSpacing(5);
    theory->addWidget(keyLabel(tr("Best case"), this), 0, 0);
    theory->addWidget(best_, 0, 1);
    theory->addWidget(keyLabel(tr("Average case"), this), 1, 0);
    theory->addWidget(average_, 1, 1);
    theory->addWidget(keyLabel(tr("Worst case"), this), 2, 0);
    theory->addWidget(worst_, 2, 1);
    theory->addWidget(keyLabel(tr("Auxiliary space"), this), 3, 0, Qt::AlignTop);
    theory->addWidget(space_, 3, 1);
    theory->setColumnStretch(1, 1);

    auto* measured = new QGridLayout;
    measured->setHorizontalSpacing(12);
    measured->setVerticalSpacing(5);
    const std::pair<QString, QLabel*> rows[] = {
        {tr("Nodes visited"), visited_},          {tr("Key comparisons"), comparisons_},
        {tr("Traversal outputs"), traversal_},    {tr("Structural changes"), changes_},
        {tr("Tree size  n"), size_},              {tr("Tree height  h"), height_},
    };
    int row = 0;
    for (const auto& [name, label] : rows) {
        measured->addWidget(keyLabel(name, this), row, 0);
        measured->addWidget(label, row, 1);
        ++row;
    }
    measured->setColumnStretch(1, 1);

    auto* footnote = new QLabel(tr("Counted by the C++ algorithm up to the current step. Playback speed does "
                                   "not affect these values; height counts edges (single node = 0)."),
                                this);
    footnote->setWordWrap(true);
    footnote->setProperty("role", "note");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    layout->addWidget(subheading(tr("Theoretical · unbalanced BST"), this));
    layout->addLayout(theory);
    layout->addWidget(note_);
    layout->addSpacing(6);
    layout->addWidget(subheading(tr("Measured · this run"), this));
    layout->addLayout(measured);
    layout->addWidget(footnote);

    setOperation(OperationType::Insert);
    setMeasurements(nullptr, 0, -1);
}

void ComplexityPanel::setOperation(OperationType op)
{
    const ComplexityInfo& info = complexityFor(op);
    best_->setText(QString::fromUtf8(info.best));
    average_->setText(QString::fromUtf8(info.average));
    worst_->setText(QString::fromUtf8(info.worst));
    space_->setText(QString::fromUtf8(info.space));
    note_->setText(QString::fromUtf8(info.note));
}

void ComplexityPanel::setMeasurements(const OperationMetrics* metrics, int treeSize, int treeHeight)
{
    const QString dash = QStringLiteral("—");
    visited_->setText(metrics ? QString::number(metrics->nodesVisited) : dash);
    comparisons_->setText(metrics ? QString::number(metrics->keyComparisons) : dash);
    traversal_->setText(metrics ? QString::number(metrics->traversalVisits) : dash);
    changes_->setText(metrics ? QString::number(metrics->structuralChanges) : dash);
    size_->setText(QString::number(treeSize));
    height_->setText(treeHeight >= 0 ? QString::number(treeHeight) : tr("— (empty)"));
}

QString ComplexityPanel::measuredValue(const QString& key) const
{
    if (key == QLatin1String("visited")) return visited_->text();
    if (key == QLatin1String("comparisons")) return comparisons_->text();
    if (key == QLatin1String("traversal")) return traversal_->text();
    if (key == QLatin1String("changes")) return changes_->text();
    if (key == QLatin1String("size")) return size_->text();
    if (key == QLatin1String("height")) return height_->text();
    return {};
}

} // namespace vdsa
