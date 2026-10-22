#pragma once

#include "AlgorithmEvent.h"

#include <QWidget>

namespace vdsa {

// Monospaced pseudocode listing with the current line highlighted. The
// listing comes from the core (pseudocodeFor), and lines are matched by
// their stable PseudoLine id, never by text.
class PseudocodePanel : public QWidget {
    Q_OBJECT

public:
    explicit PseudocodePanel(QWidget* parent = nullptr);

    void setOperation(OperationType op);
    void setActiveLine(PseudoLine line);

    OperationType operation() const { return operation_; }
    PseudoLine activeLine() const { return activeLine_; }
    int activeRow() const; // -1 when no line is highlighted
    QRect rowRect(int row) const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override { return {120, sizeHint().height()}; }

signals:
    void activeRowChanged(int row);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    int rowHeight() const;

    OperationType operation_ = OperationType::Insert;
    PseudoLine activeLine_ = PseudoLine::None;
};

} // namespace vdsa
