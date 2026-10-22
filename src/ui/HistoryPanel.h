#pragma once

#include "OperationResult.h"

#include <QWidget>

#include <memory>
#include <vector>

class QListWidget;

namespace vdsa {

// Lightweight in-memory list of recent operations. Selecting an entry
// replays its recorded steps; records are self-contained snapshots, so old
// entries stay valid after the live tree changes.
class HistoryPanel : public QWidget {
    Q_OBJECT

public:
    static constexpr int kMaxEntries = 20;

    explicit HistoryPanel(QWidget* parent = nullptr);

    void addRecord(std::shared_ptr<const OperationRecord> record);
    int entryCount() const { return static_cast<int>(records_.size()); }
    std::shared_ptr<const OperationRecord> recordAt(int row) const;

    static QString describe(const OperationResult& result);

signals:
    void replayRequested(std::shared_ptr<const vdsa::OperationRecord> record);

private:
    QListWidget* list_;
    std::vector<std::shared_ptr<const OperationRecord>> records_; // newest first, parallel to list rows
};

} // namespace vdsa
