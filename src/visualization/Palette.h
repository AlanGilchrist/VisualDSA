#pragma once

#include <QColor>
#include <QFont>
#include <QString>

// Design tokens shared by the canvas (QPainter) and the stylesheet (QSS).
namespace vdsa::palette {

inline const QColor background{0x10, 0x10, 0x10};
inline const QColor surface{0x18, 0x18, 0x18};
inline const QColor surface2{0x20, 0x20, 0x20};
inline const QColor elevated{0x27, 0x27, 0x27};
inline const QColor textPrimary{0xF2, 0xF0, 0xEB};
inline const QColor textSecondary{0xA5, 0xA2, 0x9B};
inline const QColor textMuted{0x77, 0x74, 0x6E};
inline const QColor accent{0xE5, 0xA4, 0x5C};
inline const QColor success{0x78, 0xB9, 0x9A};
inline const QColor error{0xD9, 0x6C, 0x69};
inline const QColor border{0x34, 0x34, 0x34};

// Derived node colours (tinted fills so text stays readable).
inline const QColor nodeFill{0x1D, 0x1D, 0x1C};
inline const QColor nodeBorder{0x4A, 0x48, 0x44};
inline const QColor accentFill{0x3A, 0x2B, 0x1A};
inline const QColor successFill{0x1B, 0x2C, 0x24};
inline const QColor errorFill{0x35, 0x1D, 0x1C};
inline const QColor edge{0x3E, 0x3C, 0x38};

// Returns `qss` with {{token}} placeholders replaced by the colours above.
QString applyTokens(QString qss);

QFont uiFont(qreal pointSize = 10.0, int weight = QFont::Normal);
QFont monoFont(qreal pointSize = 9.5, int weight = QFont::Normal);

} // namespace vdsa::palette
