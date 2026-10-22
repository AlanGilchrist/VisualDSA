#include "HistoryPanel.h"

#include "Palette.h"

#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

namespace vdsa {

HistoryPanel::HistoryPanel(QWidget* parent) : QWidget(parent)
{
    list_ = new QListWidget(this);
    list_->setObjectName(QStringLiteral("History"));
    list_->setAccessibleName(tr("Operation history"));
    list_->setSelectionMode(QAbstractItemView::SingleSelection);
    list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list_->setUniformItemSizes(true);
    list_->setFixedHeight(168);

    auto* hint = new QLabel(tr("Click an entry to replay its recorded steps."), this);
    hint->setProperty("role", "note");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    layout->addWidget(list_);
    layout->addWidget(hint);

    connect(list_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (auto record = recordAt(list_->row(item)))
            emit replayRequested(record);
    });
}

QString HistoryPanel::describe(const OperationResult& result)
{
    switch (result.operation) {
    case OperationType::Insert: return tr("Insert %1").arg(result.key);
    case OperationType::Delete: return tr("Delete %1").arg(result.key);
    case OperationType::Search: return tr("Search %1").arg(result.key);
    case OperationType::Update:
        return tr("Update %1 → %2").arg(result.key).arg(result.secondaryKey ? *result.secondaryKey : result.key);
    case OperationType::InorderTraversal: return tr("Inorder traversal");
    case OperationType::PreorderTraversal: return tr("Preorder traversal");
    case OperationType::PostorderTraversal: return tr("Postorder traversal");
    case OperationType::Build: return tr("Build tree");
    case OperationType::Clear: return tr("Clear tree");
    }
    return {};
}

void HistoryPanel::addRecord(std::shared_ptr<const OperationRecord> record)
{
    const OperationResult& result = record->result;
    QColor tone = palette::textSecondary;
    QString status = tr("ok");
    switch (result.status) {
    case ResultStatus::Success: tone = palette::success; status = tr("ok"); break;
    case ResultStatus::NotFound: tone = palette::error; status = tr("not found"); break;
    case ResultStatus::NoChange: tone = palette::textSecondary; status = tr("no change"); break;
    case ResultStatus::Rejected: tone = palette::error; status = tr("rejected"); break;
    case ResultStatus::Failed: tone = palette::error; status = tr("failed"); break;
    }

    auto* item = new QListWidgetItem(QStringLiteral("%1   ·   %2").arg(describe(result), status));
    item->setForeground(tone);
    item->setToolTip(QString::fromStdString(result.message));
    list_->insertItem(0, item);
    records_.insert(records_.begin(), std::move(record));

    while (static_cast<int>(records_.size()) > kMaxEntries) {
        delete list_->takeItem(list_->count() - 1);
        records_.pop_back();
    }
}

std::shared_ptr<const OperationRecord> HistoryPanel::recordAt(int row) const
{
    if (row < 0 || row >= static_cast<int>(records_.size()))
        return nullptr;
    return records_[static_cast<std::size_t>(row)];
}

} // namespace vdsa
