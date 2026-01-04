#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "../../client/rssdisk_pool.hpp"
#include "../../../libs/json/json/json.h"
#include "../../../libs/json/json/json_escape.h"
#include "../../../libs/base64.h"
#include "widjets/server_item2.h"
#include <QLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QSettings>
#include <QFileInfo>
#include <QDir>
#include <QtDebug>
#include <QJsonDocument>
#include <QFileDialog>
#include "../../../libs/thread_worker.h"
#include "../../../libs/timer.h"
#include "classes/pieview.h"
#include <QStandardItemModel>
#include "classes/disk_usage.h"

extern disk_usage DISK_USAGE;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    static void set_item_state(const std::uint32_t uuid, bool status);

private slots:
    void on_pushButton_clicked();

    void on_actionAbout_triggered();

    void on_pushButton_3_clicked();

    void on_pushButton_2_clicked();

    void on_lineEdit_returnPressed();

    void on_label_3_clicked();

    void on_pushButton_4_clicked();

    void on_pushButton_5_clicked();

    void on_pushButton_7_clicked();

    void on_pushButton_8_clicked();

    void on_comboBox_2_currentIndexChanged(int index);

    void on_pushButton_10_clicked();

    void timer();

    void on_comboBox_6_currentIndexChanged(int index);

signals:
    void s_timer();

private:
    Ui::MainWindow *ui;
    rssdisk::pool pl { 1, 1, 100 };
    API::base64 b64;

    std::atomic_bool is_running = ATOMIC_VAR_INIT(true);

    QVBoxLayout* l;

    void console2(const std::string& text, const std::string& color = "Lime");
    void console(QString text, QString color = "Lime");

    std::int32_t get_segment_folder(const std::string& filename, int count_segment_folders);
    void open_file(std::uint32_t suuid, std::string filename, bool is_save = false, bool is_all_servers = false);
    void remove_file(std::uint32_t suuid, const std::string& filename);
    void save_file(std::string content, std::string content_sett);

    std::string last_filename;
    std::uint32_t last_server = 0;

    thread_pool::thread_pool POOL { 4 };

    std::unordered_map<std::uint32_t, std::pair<std::string, std::int32_t>> _data_stat;
    std::mutex _lock_ui;

    QAbstractItemModel *model;
    PieView* pie_1;

    static std::mutex lock_all_statuses;
    static std::unordered_map<std::uint32_t, bool> all_statuses;

    bool is_inited = false;
};

#endif // MAINWINDOW_H
