#ifndef SERVER_ITEM2_H
#define SERVER_ITEM2_H

#include <QFrame>
#include <QWidget>
#include <QPainter>
#include <QPixmap>
#include <iostream>
#include <functional>
#include <QMessageBox>
#include <math.h>
#include <mutex>
#include <unordered_map>
#include "../../../libs/json/json/json.h"
#include "../../../libs/basefunc_std.h"
#include "../mainwindow.h"
#include <QProgressBar>

namespace Ui {
class server_item2;
}

class server_item2 : public QFrame
{
    Q_OBJECT

public:
    explicit server_item2(QWidget *parent, const std::string& ip, std::int32_t group, const std::uint32_t id,
                          bool is_work, std::function<void(std::uint32_t, const std::string&)> read_callback_,
                          std::function<void(std::uint32_t, const std::string&)> remove_callback_,
                          std::function<void(const std::string&, const std::string&)> console_callback_);
    ~server_item2();

    void add_disk(const std::string& name, const std::string& fn = "", int curr_size = 0, int max_size = 0, int group = 0);
    void set_file_preview(const std::string& name);
    std::uint32_t get_suuid() const { return suuid; }
    void set_data(const std::string& data);
    bool get_is_write_selected() const;

private slots:
    void on_label_2_clicked();
    void on_label_3_clicked();
    void on_label_4_clicked();

private:
    struct l_size
    {
        QLabel* l = nullptr;
        int sz = 0;
        int m_sz = 0;
        int group = -1;
    };

    Ui::server_item2 *ui;
    std::string name_to_preveue;
    std::uint32_t suuid = 0;
    bool is_running_check = false;
    std::function<void(std::uint32_t, const std::string&)> read_callback;
    std::function<void(std::uint32_t, const std::string&)> remove_callback;
    std::function<void(const std::string&, const std::string&)> console_callback;

    void set_color_pgbar(QProgressBar* p, const QString& col);

    std::mutex lock;
    std::unordered_map<std::string, l_size> label_sizes;

    void set_status_color(Qt::GlobalColor col);

    struct srv_item
    {
        std::string ip;
        bool is_connected = false;
    };

    std::vector<srv_item> srvs_connected_to;
};

#endif // SERVER_ITEM2_H
