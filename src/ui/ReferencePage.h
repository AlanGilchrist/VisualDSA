#pragma once

#include <QTextBrowser>

namespace vdsa {

// "Algorithm Reference" workspace page. Its pseudocode and complexity tables
// are generated from the same core metadata the live panels use, so the
// reference can never disagree with what the algorithms actually run.
class ReferencePage : public QTextBrowser {
    Q_OBJECT

public:
    explicit ReferencePage(QWidget* parent = nullptr);

    static QString buildHtml();
};

} // namespace vdsa
