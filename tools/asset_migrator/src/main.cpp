// RE1 Asset Migrator - portable Qt GUI for migrating the game's assets.
//
// The tool is deliberately standalone: it is built from tools/asset_migrator and
// is not part of the game build (Game.vcxproj / the root CMakeLists.txt do not
// reference it).

#include "MainWindow.h"

#include <QApplication>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("RE1 Asset Migrator"));
    QApplication::setOrganizationName(QStringLiteral("RE1 Decomp"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    MainWindow window;
    window.show();
    return app.exec();
}
