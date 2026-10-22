#include "Palette.h"

#include <QFontDatabase>
#include <QStringList>

#include <utility>

namespace vdsa::palette {

QString applyTokens(QString qss)
{
    const std::pair<const char*, QColor> tokens[] = {
        {"background", background}, {"surface", surface},       {"surface2", surface2},
        {"elevated", elevated},     {"textPrimary", textPrimary}, {"textSecondary", textSecondary},
        {"textMuted", textMuted},   {"accent", accent},         {"success", success},
        {"error", error},           {"border", border},
    };
    for (const auto& [name, color] : tokens)
        qss.replace(QStringLiteral("{{%1}}").arg(QLatin1String(name)), color.name());
    return qss;
}

QFont uiFont(qreal pointSize, int weight)
{
    static const QString family = [] {
        const QStringList available = QFontDatabase::families();
        for (const char* candidate : {"Inter", "Segoe UI", "SF Pro Text", "Helvetica Neue", "Noto Sans", "Ubuntu"}) {
            if (available.contains(QLatin1String(candidate)))
                return QString::fromLatin1(candidate);
        }
        return QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
    }();
    QFont font(family);
    font.setPointSizeF(pointSize);
    font.setWeight(static_cast<QFont::Weight>(weight));
    font.setStyleHint(QFont::SansSerif);
    return font;
}

QFont monoFont(qreal pointSize, int weight)
{
    static const QString family = [] {
        const QStringList available = QFontDatabase::families();
        for (const char* candidate : {"JetBrains Mono", "Cascadia Mono", "Consolas", "SF Mono", "Menlo",
                                      "DejaVu Sans Mono"}) {
            if (available.contains(QLatin1String(candidate)))
                return QString::fromLatin1(candidate);
        }
        return QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
    }();
    QFont font(family);
    font.setPointSizeF(pointSize);
    font.setWeight(static_cast<QFont::Weight>(weight));
    font.setStyleHint(QFont::TypeWriter);
    return font;
}

} // namespace vdsa::palette
