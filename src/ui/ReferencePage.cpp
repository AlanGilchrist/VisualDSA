#include "ReferencePage.h"

#include "Palette.h"
#include "Pseudocode.h"

namespace vdsa {

namespace {

QString pseudocodeBlock(OperationType op)
{
    QString html = QStringLiteral("<pre>");
    for (const PseudoLineSpec& line : pseudocodeFor(op)) {
        html += QString(line.indent * 2, QLatin1Char(' ')) + QString::fromUtf8(line.text).toHtmlEscaped()
              + QLatin1Char('\n');
    }
    return html + QStringLiteral("</pre>");
}

QString complexityRow(OperationType op)
{
    const ComplexityInfo& c = complexityFor(op);
    return QStringLiteral("<tr><td>%1</td><td class='m'>%2</td><td class='m'>%3</td><td class='m'>%4</td>"
                          "<td class='m'>%5</td></tr>")
        .arg(QString::fromLatin1(operationName(op)), QString::fromUtf8(c.best), QString::fromUtf8(c.average),
             QString::fromUtf8(c.worst), QString::fromUtf8(c.space).toHtmlEscaped());
}

} // namespace

ReferencePage::ReferencePage(QWidget* parent) : QTextBrowser(parent)
{
    setObjectName(QStringLiteral("Reference"));
    setOpenExternalLinks(false);
    document()->setDocumentMargin(28);
    setHtml(buildHtml());
}

QString ReferencePage::buildHtml()
{
    const QString css = QStringLiteral(
        "body { color:%1; font-size:10.5pt; }"
        "h1 { font-size:17pt; font-weight:500; margin:0 0 4px 0; }"
        "h2 { font-size:12.5pt; font-weight:600; color:%1; margin:22px 0 6px 0; }"
        "p, li { color:%2; line-height:140%; }"
        ".label { color:%3; font-size:8pt; font-weight:600; letter-spacing:1px; }"
        "pre { font-family:'%4'; font-size:9.5pt; color:%1; background:%5; padding:10px; }"
        "table { border-collapse:collapse; }"
        "th { color:%3; font-size:8.5pt; font-weight:600; text-align:left; padding:6px 14px 6px 0; "
        "border-bottom:1px solid %6; }"
        "td { color:%2; padding:5px 14px 5px 0; border-bottom:1px solid %6; }"
        "td.m { font-family:'%4'; color:%1; }"
        ".accent { color:%7; } .ok { color:%8; } .bad { color:%9; }")
                            .arg(palette::textPrimary.name(), palette::textSecondary.name(), palette::textMuted.name(),
                                 palette::monoFont().family(), palette::surface.name(), palette::border.name(),
                                 palette::accent.name(), palette::success.name(), palette::error.name());

    QString html = QStringLiteral("<html><head><style>%1</style></head><body>").arg(css);
    html += QStringLiteral(
        "<p class='label'>ALGORITHM REFERENCE</p>"
        "<h1>Binary Search Tree</h1>"
        "<p>A binary tree in which, for every node, all keys in its <b>left subtree are smaller</b> and all keys "
        "in its <b>right subtree are larger</b>. This project stores <b>unique integer keys</b>; inserting a "
        "duplicate is rejected and leaves the tree unchanged.</p>"
        "<p><b>n</b> = number of nodes. <b>h</b> = height = number of edges on the longest root-to-leaf path "
        "(empty tree: −1, single node: 0). A balanced-shaped tree has h ≈ log₂ n; inserting sorted keys produces "
        "a skewed tree with h = n − 1. This tree does <b>not</b> rebalance itself, so O(log n) is an average, "
        "never a guarantee.</p>");

    html += QStringLiteral("<h2>Complexity</h2><table width='100%'><tr><th>Operation</th><th>Best</th>"
                           "<th>Average</th><th>Worst</th><th>Auxiliary space</th></tr>");
    for (OperationType op : {OperationType::Search, OperationType::Insert, OperationType::Delete,
                             OperationType::Update, OperationType::InorderTraversal,
                             OperationType::PreorderTraversal, OperationType::PostorderTraversal}) {
        html += complexityRow(op);
    }
    html += QStringLiteral("</table><p>Search, insertion, deletion and update all walk a single root-to-leaf "
                           "path, so they are O(h). Traversals touch every node once, so they are O(n) on any "
                           "shape.</p>");

    struct Section {
        OperationType op;
        const char* title;
        const char* text;
    };
    const Section sections[] = {
        {OperationType::Search, "Search",
         "Compare the target with the current node. Equal: found. Smaller: continue in the left subtree. "
         "Larger: continue in the right subtree. Reaching a null pointer proves the key is absent."},
        {OperationType::Insert, "Insertion",
         "Follow the search path for the new key. The first empty child pointer on that path is the only "
         "position where the key keeps every ancestor's ordering rule, so the new leaf is attached there."},
        {OperationType::Delete, "Deletion — three cases",
         "<b>Case 1, leaf:</b> clear the parent's pointer. <b>Case 2, one child:</b> link the parent directly to "
         "that child. <b>Case 3, two children:</b> find the inorder successor (leftmost node of the right "
         "subtree), unlink it — it never has a left child, so this is case 1 or 2 — then write its key into the "
         "target node. The successor is larger than everything on the left and smaller than everything else "
         "on the right, so the order is preserved."},
        {OperationType::Update, "Update",
         "A key's position depends on its value, so overwriting it in place can break the ordering. The update "
         "checks that the old key exists and the new key does not, then deletes the old key and inserts the "
         "new one on a working copy. The copy replaces the tree only after it passes the invariant check; "
         "otherwise it is discarded (rollback)."},
        {OperationType::InorderTraversal, "Inorder traversal  (Left · Root · Right)",
         "Outputs the keys of a BST in ascending order."},
        {OperationType::PreorderTraversal, "Preorder traversal  (Root · Left · Right)",
         "Outputs each node before its subtrees. Re-inserting keys in preorder rebuilds the same tree."},
        {OperationType::PostorderTraversal, "Postorder traversal  (Left · Right · Root)",
         "Outputs each node after both subtrees — the order used to free a tree bottom-up."},
    };
    for (const Section& s : sections) {
        html += QStringLiteral("<h2>%1</h2><p>%2</p>").arg(QString::fromUtf8(s.title), QString::fromUtf8(s.text));
        html += pseudocodeBlock(s.op);
    }

    html += QStringLiteral(
        "<h2>Reading the canvas</h2><ul>"
        "<li><span class='accent'>Amber</span> — the node being examined, and the path from the root to it.</li>"
        "<li><span class='ok'>Green</span> — a found or inserted node, or a node already output by a traversal "
        "(with its output position as a small number).</li>"
        "<li><span class='bad'>Red</span> — a node being removed, or a search that ended at a null pointer.</li>"
        "<li>Dashed ring — a second node involved in the step (next node, only child, successor, parent).</li>"
        "<li>Dashed circle — an empty child position (where a key would be inserted, or where a search ended).</li>"
        "<li>Nodes are placed by inorder rank, so reading the tree left to right gives the keys in sorted "
        "order.</li></ul>");
    html += QStringLiteral("</body></html>");
    return html;
}

} // namespace vdsa
