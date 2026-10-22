#include "ExplanationPanel.h"

#include "Palette.h"

#include <QLabel>
#include <QStringList>
#include <QVBoxLayout>

namespace vdsa {

namespace {

QString caseName(DeletionCase c)
{
    switch (c) {
    case DeletionCase::Leaf: return QObject::tr("CASE 1 · LEAF");
    case DeletionCase::OneChild: return QObject::tr("CASE 2 · ONE CHILD");
    case DeletionCase::TwoChildren: return QObject::tr("CASE 3 · TWO CHILDREN");
    case DeletionCase::None: break;
    }
    return {};
}

} // namespace

ExplanationPanel::ExplanationPanel(QWidget* parent) : QWidget(parent)
{
    stepLabel_ = new QLabel(this);
    stepLabel_->setObjectName(QStringLiteral("ExplainStep"));
    stepLabel_->setFont(palette::monoFont(8.5));
    stepLabel_->setWordWrap(true);

    comparison_ = new QLabel(this);
    comparison_->setObjectName(QStringLiteral("ExplainComparison"));
    comparison_->setFont(palette::monoFont(11, QFont::DemiBold));

    action_ = new QLabel(this);
    action_->setObjectName(QStringLiteral("ExplainAction"));
    action_->setWordWrap(true);
    action_->setFont(palette::uiFont(11.5, QFont::DemiBold));

    reason_ = new QLabel(this);
    reason_->setObjectName(QStringLiteral("ExplainReason"));
    reason_->setWordWrap(true);

    outputLabel_ = new QLabel(tr("OUTPUT SO FAR"), this);
    outputLabel_->setProperty("role", "section");
    output_ = new QLabel(this);
    output_->setObjectName(QStringLiteral("ExplainOutput"));
    output_->setWordWrap(true);
    output_->setFont(palette::monoFont(10));
    output_->setTextInteractionFlags(Qt::TextSelectableByMouse);

    for (QLabel* label : {action_, reason_})
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(stepLabel_);
    layout->addWidget(comparison_);
    layout->addWidget(action_);
    layout->addWidget(reason_);
    layout->addSpacing(4);
    layout->addWidget(outputLabel_);
    layout->addWidget(output_);

    clear();
}

void ExplanationPanel::setEvent(const AlgorithmEvent& event, int step, int count)
{
    QString header = tr("STEP %1 / %2  ·  %3")
                         .arg(step + 1)
                         .arg(count)
                         .arg(QString::fromLatin1(operationName(event.operation)).toUpper());
    if (event.deletionCase != DeletionCase::None)
        header += QStringLiteral("  ·  ") + caseName(event.deletionCase);
    stepLabel_->setText(header);

    const QString comparison = QString::fromStdString(vdsa::comparisonText(event));
    comparison_->setVisible(!comparison.isEmpty());
    comparison_->setText(comparison.isEmpty() ? QString() : tr("compare  %1").arg(comparison));

    action_->setText(QString::fromStdString(event.action));
    reason_->setText(QString::fromStdString(event.reason));

    const bool traversal = isTraversal(event.operation);
    outputLabel_->setVisible(traversal);
    output_->setVisible(traversal);
    if (traversal) {
        QStringList keys;
        for (int key : event.sequence)
            keys << QString::number(key);
        output_->setText(keys.isEmpty() ? QStringLiteral("—") : keys.join(QStringLiteral("  →  ")));
    }
}

void ExplanationPanel::clear()
{
    stepLabel_->setText(tr("NO STEP SELECTED"));
    comparison_->hide();
    action_->setText(tr("Run an operation to see it explained step by step."));
    reason_->setText(tr("Each step shows the comparison made, the action taken and the reason for it."));
    outputLabel_->hide();
    output_->hide();
}

QString ExplanationPanel::actionText() const { return action_->text(); }
QString ExplanationPanel::reasonText() const { return reason_->text(); }
QString ExplanationPanel::comparisonText() const { return comparison_->isVisibleTo(this) ? comparison_->text() : QString(); }

} // namespace vdsa
