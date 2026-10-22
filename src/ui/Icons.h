#pragma once

#include <QColor>
#include <QIcon>

namespace vdsa::icons {

enum class Glyph { Play, Pause, Previous, Next, Restart, ZoomIn, ZoomOut, Fit, OneToOne };

// Simple vector icons painted with QPainter, so the UI needs no image files
// and never relies on emoji or font glyphs.
QIcon make(Glyph glyph, const QColor& color, int size = 18);

} // namespace vdsa::icons
