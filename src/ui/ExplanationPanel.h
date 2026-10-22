#pragma once

#include "AlgorithmEvent.h"

#include <QWidget>

class QLabel;

namespace vdsa {

// Shows what the current step does and why, generated from the event data:
// step number, operation, comparison, action, reason and traversal output.
class ExplanationPanel : public QWidget {
    Q_OBJECT

public:
    explicit ExplanationPanel(QWidget* parent = nullptr);

    void setEvent(const AlgorithmEvent& event, int step, int count);
    void clear();

    QString actionText() const;
    QString reasonText() const;
    QString comparisonText() const;

private:
    QLabel* stepLabel_;
    QLabel* comparison_;
    QLabel* action_;
    QLabel* reason_;
    QLabel* outputLabel_;
    QLabel* output_;
};

} // namespace vdsa
