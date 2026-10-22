#include "Theme.h"

#include "Palette.h"

#include <QApplication>
#include <QFile>
#include <QPalette>
#include <QStyleFactory>

// The resource file is compiled into this static library, so it must be
// registered explicitly (outside any namespace).
static void initThemeResources() { Q_INIT_RESOURCE(resources); }

namespace vdsa {

void applyTheme(QApplication& app)
{
    initThemeResources();
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QApplication::setFont(palette::uiFont(10));

    // Base palette for anything the stylesheet does not cover (dialogs, menus).
    QPalette pal;
    pal.setColor(QPalette::Window, palette::surface);
    pal.setColor(QPalette::WindowText, palette::textPrimary);
    pal.setColor(QPalette::Base, palette::background);
    pal.setColor(QPalette::AlternateBase, palette::surface2);
    pal.setColor(QPalette::Text, palette::textPrimary);
    pal.setColor(QPalette::Button, palette::elevated);
    pal.setColor(QPalette::ButtonText, palette::textPrimary);
    pal.setColor(QPalette::Highlight, palette::accent);
    pal.setColor(QPalette::HighlightedText, palette::background);
    pal.setColor(QPalette::ToolTipBase, palette::elevated);
    pal.setColor(QPalette::ToolTipText, palette::textPrimary);
    pal.setColor(QPalette::PlaceholderText, palette::textMuted);
    QApplication::setPalette(pal);

    QFile theme(QStringLiteral(":/styles/theme.qss"));
    if (theme.open(QIODevice::ReadOnly | QIODevice::Text))
        app.setStyleSheet(palette::applyTokens(QString::fromUtf8(theme.readAll())));
}

} // namespace vdsa
