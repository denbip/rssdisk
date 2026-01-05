#include "mainwindow.h"
#include <QApplication>
#include "../../../libs/basefunc_std.h"

int main(int argc, char *argv[])
{
    basefunc_std::set_path_log("/var/log/rssdisk_pool/");

    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    return a.exec();
}
