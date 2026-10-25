#include "MainWindow.h"

#include "DcAssetsTab.h"
#include "PcAssetsTab.h"

#include <QTabWidget>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(tr("RE1 Asset Migrator"));
    auto* tabs = new QTabWidget(this);
    tabs->addTab(new PcAssetsTab(tabs), tr("PC Assets"));
    tabs->addTab(new DcAssetsTab(tabs), tr("Director's Cut"));
    setCentralWidget(tabs);
    resize(760, 620);
}
