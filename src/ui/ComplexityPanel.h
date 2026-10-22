#pragma once

#include "AlgorithmEvent.h"

#include <QWidget>

class QLabel;

namespace vdsa {

// Theoretical complexity for the selected operation, kept visually separate
// from the counters actually measured during the recorded run.
class ComplexityPanel : public QWidget {
    Q_OBJECT

public:
    explicit ComplexityPanel(QWidget* parent = nullptr);

    void setOperation(OperationType op);
    // Measured counters up to the current step, and the size/height of the
    // tree shown at that step. Pass nullptr when nothing has run yet.
    void setMeasurements(const OperationMetrics* metrics, int treeSize, int treeHeight);

    QString measuredValue(const QString& key) const;

private:
    QLabel* best_;
    QLabel* average_;
    QLabel* worst_;
    QLabel* space_;
    QLabel* note_;
    QLabel* visited_;
    QLabel* comparisons_;
    QLabel* traversal_;
    QLabel* changes_;
    QLabel* size_;
    QLabel* height_;
};

} // namespace vdsa
