#pragma once

#include "AlgorithmEvent.h"

#include <QWidget>

#include <vector>

class QLabel;
class QLineEdit;

namespace vdsa {

enum class Module { Overview, Insert, Delete, Search, Update, Traversals, Reference };

// Input controls for the selected module. Input is validated here (with
// inline error messages) before any request reaches the tree.
class OperationPanel : public QWidget {
    Q_OBJECT

public:
    explicit OperationPanel(QWidget* parent = nullptr);

    void setModule(Module module);
    Module module() const { return module_; }
    void showError(const QString& message);
    void clearError();
    QString errorText() const;

signals:
    void insertRequested(int key);
    void deleteRequested(int key);
    void searchRequested(int key);
    void updateRequested(int oldKey, int newKey);
    void traversalRequested(vdsa::TraversalOrder order);
    void buildRequested(const std::vector<int>& keys);
    void clearRequested();
    void sampleRequested();

private:
    QWidget* buildOverviewPage();
    QWidget* buildSingleKeyPage(const QString& objectName, const QString& description, const QString& buttonText,
                                void (OperationPanel::*signal)(int));
    QWidget* buildUpdatePage();
    QWidget* buildTraversalPage();
    QWidget* buildReferencePage();
    bool readKey(QLineEdit* edit, int& value, const QString& fieldName);

    std::vector<QWidget*> pages_; // indexed by Module
    QLabel* error_;
    QLineEdit* treeInput_ = nullptr;
    QLineEdit* oldKey_ = nullptr;
    QLineEdit* newKey_ = nullptr;
    Module module_ = Module::Overview;
};

} // namespace vdsa
