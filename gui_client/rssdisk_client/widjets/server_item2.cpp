#include "server_item2.h"
#include "ui_server_item2.h"
#include <QStyleFactory>

server_item2::server_item2(QWidget *parent, const std::string &ip, std::int32_t group, const uint32_t id, bool is_work,
                           std::function<void(uint32_t, const std::string &)> read_callback_,
                           std::function<void(std::uint32_t, const std::string&)> remove_callback_,
                           std::function<void(const std::string&, const std::string&)> console_callback_) :
    QFrame(parent),
    ui(new Ui::server_item2),
    read_callback(read_callback_),
    remove_callback(remove_callback_),
    console_callback(console_callback_)
{
    ui->setupUi(this);

    //setMinimumSize(260, 60);
    setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Maximum );

    ui->label->setText(QString::fromStdString(ip + " (g." + std::to_string(group) + ")"));

    if (!is_work) set_status_color(Qt::red);
    else set_status_color(Qt::green);

    setObjectName(QString("server_item_") + QString::number(id));
    setStyleSheet(QString("#server_item_") + QString::number(id) + " { background-color: #303030; border: 0px solid black; border-radius: 3px; } .QLabel { color: #e0e0e0; }");
    ui->label_3->setVisible(false);
    ui->label_4->setVisible(false);

    QSizePolicy sp_retain = ui->label_5->sizePolicy();
    sp_retain.setRetainSizeWhenHidden(true);
    ui->label_5->setSizePolicy(sp_retain);

    suuid = id;
    name_to_preveue = "";
}

server_item2::~server_item2()
{
    delete ui;
}

bool server_item2::get_is_write_selected() const
{
    return ui->checkBox->isChecked();
}

void server_item2::set_status_color(Qt::GlobalColor col)
{
    int w = ui->label_2->width() - 2;
    int h = ui->label_2->height() - 2;

    QPixmap pixmap(w + 2, h + 2);
    pixmap.fill(QColor("transparent"));

    QPainter painter(&pixmap);

    painter.setBrush(QBrush(col));

    painter.setPen(Qt::NoPen);
    QRect r(QPoint(0, 0), QSize(w, h));
    r.moveCenter(QPoint(w / 2, h / 2));
    painter.drawEllipse(r);

    ui->label_2->setPixmap(pixmap);

    MainWindow::set_item_state(suuid, col == Qt::green);
}

void server_item2::add_disk(const std::string &name, const std::string &fn, int curr_size, int max_size, int group)
{
    QFont f("Arial", 10);

    QLabel* l = new QLabel();
    l->setFont(f);
    l->setText(QString::fromStdString(name));
    ui->verticalLayout_3->addWidget(l);

    if (!fn.empty())
    {
        l_size l_sz;
        l_sz.l = l;
        l_sz.group = group;

        std::lock_guard<std::mutex> _{ lock };
        label_sizes[fn] = l_sz;
    }
}

void server_item2::set_is_running_on_check(bool b)
{
    is_running_check = b;
    if (!is_running_check)
    {
        ui->label_5->setVisible(true);
    }
}

void server_item2::running_on_check()
{
    if (!is_running_check) return;
    bool to_set { ui->label_5->isVisible() };
    ui->label_5->setVisible(!to_set);
}

void server_item2::set_data(const std::string& data)
{
    if (!data.empty())
    {
        try
        {
            Json::Value root;
            bool parsingSuccessful = Json::Reader().parse(data, root);
            if (parsingSuccessful)
            {
                if (root.isMember("sub"))
                {
                    const Json::Value& array = root["sub"];
                    for (unsigned i = 0; i < array.size(); ++i)
                    {
                        std::string subfolder_name = array[i].get("n", "").asString();

                        const Json::Value& f = array[i]["f"];
                        for (unsigned i = 0; i < f.size(); ++i)
                        {
                            std::string folder_name = f[i].get("n", "").asString();
                            std::string folder_path = f[i].get("p", "").asString();

                            int s_mb = f[i].get("s_mb", 0).asInt();
                            int max_s_mb = f[i].get("max_s_mb", 0).asInt();

                            std::lock_guard<std::mutex> _{ lock };
                            auto f = label_sizes.find(subfolder_name + folder_name);
                            if (f != label_sizes.end())
                            {
                                l_size& l_sz = f->second;

                                if (l_sz.sz != s_mb || l_sz.m_sz != max_s_mb)
                                {
                                    l_sz.sz = s_mb;
                                    l_sz.m_sz = max_s_mb;

                                    DISK_USAGE.set_size(suuid, l_sz.group, subfolder_name, folder_name, s_mb, max_s_mb);

                                    std::string sz;
                                    if (max_s_mb != 0)
                                    {
                                        double perc = double(s_mb * 100) / double(max_s_mb);
                                        sz += basefunc_std::to_string_double(perc, 1) + "% ";
                                    }
                                    sz += "(max " + std::to_string(max_s_mb) + "Mb)";

                                    l_sz.l->setText(QString::fromStdString(" - " + sz + " " + folder_name + " (" + folder_path + ")"));
                                }
                            }

                        }
                    }
                }

                srvs_connected_to.clear();

                if (root.isMember("srvs"))
                {
                    bool is_connected { true };
                    for (const auto& it : root["srvs"])
                    {
                        srv_item item;

                        item.ip = it.get("ip", "").asString();
                        int c = it.get("c", 0).asInt();
                        if (c == 0) is_connected = false;

                        item.is_connected = (c != 0);

                        srvs_connected_to.push_back(std::move(item));
                    }

                    if (!is_connected)
                    {
                        set_status_color(Qt::gray);
                    }
                    else
                    {
                        set_status_color(Qt::green);
                    }
                }
            }
            else
            {
                std::cout << "server_item2::set_data " << data << std::endl;
            }
        }
        catch(std::exception& ex)
        {
            std::cout << "server_item2::set_data " << ex.what() << std::endl;
        }
    }
}

void server_item2::set_file_preview(const std::string& name)
{
    if (name.empty())
    {
        ui->label_3->setVisible(false);
        ui->label_4->setVisible(false);
    }
    else
    {
        ui->label_3->setVisible(true);
        ui->label_4->setVisible(true);
    }

    name_to_preveue = name;
}

void server_item2::on_label_2_clicked()
{
    for (const auto& it : srvs_connected_to)
    {
        if (it.is_connected) console_callback("Connection established to " + it.ip, "Lime");
        else console_callback("Connection not established to " + it.ip, "Red");
    }
}

void server_item2::on_label_3_clicked()
{
    if (!name_to_preveue.empty())
    {
        read_callback(suuid, name_to_preveue);
    }
}

void server_item2::on_label_4_clicked()
{
    if (QMessageBox::question(nullptr, "Warning", "Remove file?", QMessageBox::Yes|QMessageBox::No) != QMessageBox::Yes) return;

    remove_callback(suuid, name_to_preveue);
}
