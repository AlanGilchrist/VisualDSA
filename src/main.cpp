#include "MainWindow.h"
#include "Theme.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("VisualDSA"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    vdsa::applyTheme(app);

    vdsa::MainWindow window;
    window.show();
    return QApplication::exec();
}
