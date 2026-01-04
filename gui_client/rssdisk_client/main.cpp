#include "mainwindow.h"
#include <QApplication>
#include "../../../libs/basefunc_std.h"
#include "classes/disk_usage.h"

disk_usage DISK_USAGE;

int main(int argc, char *argv[])
{
    basefunc_std::set_path_log("/var/log/rssdisk_pool/");

    QApplication a(argc, argv);
    MainWindow w;
    w.show();

    return a.exec();
}
