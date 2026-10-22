#pragma once

class QApplication;

namespace vdsa {

// Applies the VisualDSA look: Fusion style, fonts, base palette and the
// token-substituted stylesheet from resources/styles/theme.qss.
void applyTheme(QApplication& app);

} // namespace vdsa
