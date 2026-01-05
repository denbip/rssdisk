#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QScreen>

std::mutex MainWindow::lock_all_statuses;
std::unordered_map<std::uint32_t, bool> MainWindow::all_statuses;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(this, SIGNAL(s_timer()), this, SLOT(timer()));

    pl.init("/etc/general/rssdisk.json");

    std::this_thread::sleep_for(std::chrono::seconds(1));

    ui->scrollArea->setWidgetResizable(true);

    auto *new_widget = new QWidget( ui->scrollArea );
    l = new QVBoxLayout(new_widget);
    new_widget->setLayout( l );
    ui->scrollArea->setWidget( new_widget );

    QPalette p = ui->textEdit->palette();
    p.setColor(QPalette::Base, QColor(48, 48, 48));
    p.setColor(QPalette::Text, QColor(224, 224, 224));
    ui->textEdit->setPalette(p);

    setPalette(p);

    p.setColor(QPalette::Base, QColor(255, 255, 255));
    p.setColor(QPalette::Text, QColor(0, 0, 0));
    ui->lineEdit->setPalette(p);
    ui->lineEdit_3->setPalette(p);

    ui->comboBox_2->addItem("Image");
    ui->comboBox_2->addItem("Json");
    ui->comboBox_2->addItem("Text");
    ui->comboBox_2->addItem("jdb");
    ui->comboBox_2->addItem("ajdb");

    ui->label_3->setVisible(false);

    ui->textEdit_3->setVisible(false);

    std::unordered_set<std::string> available_groups;
    try
    {
        std::ifstream t("/etc/general/rssdisk.json");
        if (!t) throw std::runtime_error("Failed to open file configuration");
        std::string c((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

        Json::Value root;
        bool parsingSuccessful = Json::Reader().parse(c, root);
        if (!parsingSuccessful) throw std::runtime_error("Failed to parse configuration");

        Json::Value::Members m = root.getMemberNames();
        for (int i = 0; i < m.size(); ++i)
        {
            std::string networks = root[m[i]]["replication"]["networks"].toString();

            basefunc_std::replaceAll(networks, "[", "");
            basefunc_std::replaceAll(networks, "]", "");

            available_groups.emplace(networks);
        }

    }
    catch(std::exception& ex)
    {
        console(QString("cant_load_settings ") + QString(ex.what()), "Red");
    }

    ui->comboBox_6->addItem("");
    for (const std::string& it : available_groups)
    {
        ui->comboBox_6->addItem(QString::fromStdString(it));
    }

    QSettings settings { QCoreApplication::applicationDirPath() + "/settings.ini", QSettings::IniFormat };

    QFileInfo check_file { QCoreApplication::applicationDirPath() + "/settings.ini" };
    if (check_file.exists() && check_file.isFile())
    {
        ui->comboBox_6->setCurrentIndex(settings.value("selected_group", 0).toInt());
    }
    else
    {
        settings.setValue("templates/names", "");
        settings.sync();
    }

    is_inited = true;

    //if (!available_groups.empty()) ui->comboBox_5->setCurrentIndex(0);

    on_pushButton_clicked();

    POOL.push_back([this]()
    {
        int d = 10;
        while (is_running)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            ++d;
            if (d >= 10)
            {
                d = 0;
                std::lock_guard<std::mutex> _{ _lock_ui };
                _data_stat.clear();

                std::vector<rssdisk::client::read_responce> ok_read, ok_ping;
                {
                    rssdisk::pool::guard g = pl.get();
                    if (g.get() == nullptr)
                    {
                        console("[error] cant get pool guard", "Red");
                        return;
                    }

                    ok_read = g->get_servers_statuses();
                    ok_ping = g->get_servers_ping();
                }

                for (const rssdisk::client::read_responce& it : ok_read)
                {
                    _data_stat.insert( { it.ip_address_readed, { it.data, -1 } } );
                }

                for (const rssdisk::client::read_responce& it : ok_ping)
                {
                    auto f = _data_stat.find(it.ip_address_readed);
                    if (f != _data_stat.end())
                    {
                        if (!it.data.empty()) basefunc_std::stoi(it.data, f->second.second);
                    }
                }
            }

            emit s_timer();
        }
    });
}

MainWindow::~MainWindow()
{
    is_running = false;
    pl.stop();
    delete ui;
}

void MainWindow::console2(const std::string& text, const std::string& color)
{
    console(QString::fromStdString(text), QString::fromStdString(color));
}

void MainWindow::console(QString text, QString color)
{
    if (!color.isEmpty())
    {
        bool finded_b { false };

        int f = text.indexOf("[");
        if (f != -1)
        {
            int f2 = text.indexOf("]");
            if (f2 != -1 && f2 > f)
            {
                text.replace(f + 1, f2 - f - 1, "<font color=\"" + color + "\">" + text.mid(f + 1, f2 - f - 1) + "</font>");
                finded_b = true;
            }
        }

        if (!finded_b)
        {
            text = "<font color=\"" + color + "\">" + text + "</font>";
        }
    }

    ui->textEdit->append(text);
}

void MainWindow::on_pushButton_clicked()
{
    std::set<std::int32_t> groups;
    if (ui->comboBox_6->currentIndex() > 0)
    {
        basefunc_std::get_set_from_string(ui->comboBox_6->currentText().toStdString(), groups);
    }

    {
        rssdisk::pool::guard g = pl.get();
        if (g.get() == nullptr)
        {
            console("[error] cant get pool guard", "Red");
            return;
        }

        console(QString("Refreshing status ") + QString::fromStdString(date_time::current_date_time().get_date_time()), "#82d8a7");

        std::vector<rssdisk::client::read_responce> ok_read = g->get_servers_statuses();

        //remove all items
        {
            QLayoutItem* item;
            while ( ( item = l->takeAt( 0 ) ) != nullptr )
            {
                delete item->widget();
                delete item;
            }

            std::map<std::uint32_t, rssdisk::client::read_responce> sorted;
            for (rssdisk::client::read_responce& d : ok_read)
            {
                if (groups.empty() || groups.count(d.group) != 0)
                {
                    sorted.insert( { d.ip_address_readed, std::move(d) } );
                }
            }

            for (const auto& s_d : sorted)
            {
                const rssdisk::client::read_responce& d = s_d.second;

                QString sts { "ok" };
                QString color { "#f25c5c" };
                bool is_ok { false };
                switch (d.status)
                {
                    case rssdisk::client::read_res::timeout: sts = "timeout"; break;
                    case rssdisk::client::read_res::error_read_responce_from_server: sts = "error_read_responce_from_server"; break;
                    case rssdisk::client::read_res::error_send_request_from_server: sts = "error_send_request_from_server"; break;
                    case rssdisk::client::read_res::error_to_connect_to_server: sts = "error_to_connect_to_server"; break;
                    case rssdisk::client::read_res::fail: sts = "fail"; break;
                    case rssdisk::client::read_res::file_not_found: sts = "file_not_found"; break;
                    default: color = "Lime"; is_ok = true; break;
                }

                server_item2* it = new server_item2(ui->scrollArea, network_std::inet_ntoa(d.ip_address_readed), d.group, d.ip_address_readed, is_ok, [this](std::uint32_t suuid, const std::string& filename)
                {
                    open_file(suuid, filename);
                }, [this](std::uint32_t suuid, const std::string& filename)
                {
                    remove_file(suuid, filename);
                }, [this](const std::string& d, const std::string& c)
                {
                    console2(d, c);
                });

                l->addWidget(it);

                if (!d.data.empty())
                {
                    try
                    {
                        Json::Value root;
                        bool parsingSuccessful = Json::Reader().parse(d.data, root);
                        if (parsingSuccessful)
                        {
                            const Json::Value& array = root["sub"];
                            for (unsigned i = 0; i < array.size(); ++i)
                            {
                                std::string subfolder_name = array[i].get("n", "").asString();
                                it->add_disk(subfolder_name);

                                const Json::Value& f = array[i]["f"];
                                for (unsigned i = 0; i < f.size(); ++i)
                                {
                                    std::string folder_name = f[i].get("n", "").asString();
                                    std::string folder_path = f[i].get("p", "").asString();

                                    int s_mb = f[i].get("s_mb", 0).asInt();
                                    int max_s_mb = f[i].get("max_s_mb", 0).asInt();

                                    std::string sz;
                                    if (max_s_mb != 0)
                                    {
                                        double perc = double(s_mb * 100) / double(max_s_mb);
                                        sz += basefunc_std::to_string_double(perc, 1) + "% ";

                                        if (perc > 90.0)
                                        {
                                            console(QString("[warning] ") + QString::fromStdString(network_std::inet_ntoa(d.ip_address_readed)) + QString(" low disk space ") + QString::fromStdString(folder_path), "#f25c5c");
                                        }
                                    }
                                    sz += "(max " + std::to_string(max_s_mb) + "Mb)";

                                    it->add_disk(" - " + sz + " " + folder_name + " (" + folder_path + ")", subfolder_name + folder_name, s_mb, max_s_mb, d.group);
                                }
                            }
                        }
                    }
                    catch(...)
                    {

                    }
                }

                if (!is_ok) console(QString("[") + sts + "] " + QString::fromStdString(network_std::inet_ntoa(d.ip_address_readed)), color);
            }
        }
    }
}

void MainWindow::on_actionAbout_triggered()
{
    QMessageBox msg { QMessageBox::Information, "Rssdisk about", "all rights reserved 2021", QMessageBox::Ok };
    msg.exec();
}

void MainWindow::on_pushButton_3_clicked()
{
    ui->textEdit->clear();
}

void MainWindow::on_pushButton_2_clicked()
{
    QString search;
    search += ui->lineEdit->text();

    {
        rssdisk::pool::guard g = pl.get();
        if (g.get() == nullptr)
        {
            console("[error] cant get pool guard", "Red");
            return;
        }

        std::int32_t count_segments_folder { 1000 };
        std::int32_t segment = get_segment_folder(search.toStdString(), count_segments_folder);

        QString out_for_console;
        if (search.contains("/"))
        {
            QStringList exp = search.split("/");
            if (exp.size() >= 2)
            {
                out_for_console = exp[0] + "/" + QString::number(segment) + "/" + exp[1];
            }
        }
        else
        {
            out_for_console = QString::number(segment) + "/" + search;
        }

        console(QString("[get_file] ") + out_for_console);

        {
            QLayoutItem* item;
            auto sz = l->count();
            for (auto i = 0; i < sz; ++i)
            {
                item = l->itemAt(i);
                server_item2* it = dynamic_cast<server_item2*>(item->widget());
                if (it != nullptr)
                {
                    it->set_file_preview("");
                }
            }
        }

        std::vector<std::int32_t> groups;
        std::vector<std::string> exp = basefunc_std::split(ui->comboBox_6->currentText().toStdString(), ',');
        for (const auto& it : exp)
        {
            std::int32_t v { -1 };
            basefunc_std::stoi(it, v);
            if (v >= 0) groups.push_back(v);
        }

        rssdisk::client::rw_preference pr = rssdisk::client::rw_preference::any;
        if (!groups.empty()) pr = rssdisk::client::rw_preference::specified_group;

        std::vector<rssdisk::read_info> exists;
        g->command(rssdisk::client::read_info_type::file_exists, exists, search.toStdString(), std::numeric_limits<std::int32_t>::max(), 10000, groups, pr);
        std::unordered_set<std::uint32_t> ser_ex;
        for (const auto& it : exists)
        {
            ser_ex.emplace(it.ip);
        }

        last_filename = search.toStdString();
        last_server = 0;

        int count { 0 };
        {
            QLayoutItem* item;
            auto sz = l->count();
            for (auto i = 0; i < sz; ++i)
            {
                item = l->itemAt(i);
                server_item2* it = dynamic_cast<server_item2*>(item->widget());
                if (it != nullptr)
                {
                    if (ser_ex.count(it->get_suuid()) != 0)
                    {
                        it->set_file_preview(search.toStdString());
                        ++count;

                        last_server = it->get_suuid();
                    }
                }
            }
        }

        ui->label_2->setText(QString::number(count));
        ui->label_3->setVisible(count != 0);
    }

}

void MainWindow::on_label_3_clicked()
{
    if (last_server != 0)
    {
        open_file(last_server, last_filename, false, true);
    }
}

std::int32_t MainWindow::get_segment_folder(const std::string& filename, int count_segment_folders)
{
    std::hash<std::string> h;
    return h(filename) % count_segment_folders;
}

void MainWindow::open_file(std::uint32_t suuid, std::string filename, bool is_save, bool is_all_servers)
{
    if (!is_save) ui->textEdit_2->clear();

    {
        rssdisk::pool::guard g = pl.get();
        if (g.get() == nullptr)
        {
            console("[error] cant get pool guard", "Red");
            return;
        }

        ::timer tm;

        std::int32_t count_segments_folder { 1000 };
        std::int32_t segment = get_segment_folder(filename, count_segments_folder);

        bool is_jdb { false };
        bool read_sdb { false };
        if (ui->comboBox_2->currentText().compare("jdb") == 0 || ui->comboBox_2->currentText().compare("ajdb") == 0)
        {
            is_jdb = true;

            filename += "?jdb";
            if (!ui->textEdit_3->toPlainText().toStdString().empty())
            {
                Json::Value s_json;
                try
                {
                    bool parsingSuccessful = Json::Reader().parse(ui->textEdit_3->toPlainText().toStdString(), s_json);
                    if (parsingSuccessful)
                    {

                    }
                    else
                    {
                        console("[error] parse json failed", "#f25c5c");
                    }
                }
                catch(std::exception& ex)
                {
                    console(QString("[error] ") + QString(ex.what()), "#f25c5c");
                    return;
                }

                filename += s_json.toString();
            }
            else
            {
                filename += "{}";
            }
        }
        else if (ui->comboBox_2->currentText().compare("sdb") == 0)
        {
            filename += "?";
            filename += ui->textEdit_3->toPlainText().toStdString();
            read_sdb = true;
        }

        console(QString("[get_file] ") + QString::fromStdString(filename) + " segment: " + QString::number(segment));

        std::string content;
        rssdisk::client::read_responce res { rssdisk::client::read_res::fail };

        std::string log_group { "any" };

        if (is_jdb && is_all_servers)
        {
            rssdisk::client::rw_preference rp = rssdisk::client::rw_preference::any;
            std::vector<std::int32_t> groups;

            QString _gr = ui->comboBox_6->currentText();
            if (_gr != "")
            {
                auto exp = basefunc_std::split(_gr.toStdString(), ',');
                for (const std::string& _g : exp)
                {
                    std::int32_t g { 0 };
                    basefunc_std::stoi(_g, g);

                    groups.push_back(g);
                }
                rp = rssdisk::client::rw_preference::specified_group;

                log_group = "specified_group [" + _gr.toStdString() + "]";
            }

            std::vector<rssdisk::read_info> info;
            res = g->command(rssdisk::client::read_info_type::read_files, info, filename, std::numeric_limits<std::int32_t>::max(), 300000, groups, rp);

            std::unordered_set<std::string> _exists;
            for (const rssdisk::read_info& ri : info)
            {
                if (ri.content.size() > 2)
                {
                    try
                    {
                        Json::Value js;
                        Json::Reader _r;
                        bool ok = _r.parse(ri.content, js);
                        if (ok)
                        {
                            for (int i = 0; i < js.size(); ++i)
                            {
                                std::string d = js[i].toString();

                                if (_exists.count(d) == 0)
                                {
                                    _exists.emplace(d);
                                    if (!content.empty()) content += ",";
                                    content += d;
                                }
                            }
                        }
                        else
                        {
                            console2("Can't parse json", "red");
                        }
                    }
                    catch(std::exception &e)
                    {
                        console2(e.what(), "red");
                    }
                }
            }

            content = "[" + content + "]";
        }
        else
        {
            log_group = "ip [" + network_std::inet_ntoa(suuid) + "]";
            res = g->read_file(filename, content, 20000, 20000, 2, std::vector<std::int32_t>(), rssdisk::client::rw_preference::any, { suuid }, false, read_sdb);
        }

        if (res.status == rssdisk::client::read_res::ok)
        {
            if (!content.empty())
            {
                console(QString("[file] ok, elapsed, ms: ") + QString::number(tm.elapsed_mili()) + " ttl: " + QString::number(res.ri.ttl_days) + " tms: " + QString::number(res.ri.file_tms) + " crc32: " + QString::number(res.ri.file_crc32) + " " + QString::fromStdString(res.ri.file_created.get_date_time()) + ", read_from: " + QString::fromStdString(log_group));

                if (ui->comboBox_2->currentText().compare("Image") == 0)
                {
                    std::string img;
                    if (content[0] == '_')
                    {
                        std::vector<std::string> exp = basefunc_std::split(content, '_');
                        if (exp.size() >= 3)
                        {
                            console(QString("[format] ") + QString::fromStdString(b64.base64_decode(exp[1])));
                            img = b64.base64_decode(exp[2]);
                        }
                        else
                        {
                            console("[error] content explode error", "#f25c5c");
                        }
                    }
                    else
                    {
                        console("[format] not recognized");
                        img = b64.base64_decode(content);
                    }

                    QByteArray ba;

                    auto sz = img.size();
                    for (auto i = 0u; i < sz; ++i)
                    {
                        ba.append(img[i]);
                    }

                    QImage i = QImage::fromData(ba);

                    if(!i.isNull())
                    {
                        if (is_save)
                        {
                            QString to_save = QFileDialog::getSaveFileName(this, "Save Image File", QString(), "Images (*.png)");
                            if (!to_save.isEmpty())
                            {
                                i.save(to_save);
                            }
                        }
                        else
                        {
                            QUrl Uri;
                            QTextDocument * textDocument = ui->textEdit_2->document();
                            textDocument->addResource(QTextDocument::ImageResource, Uri, QVariant ( i ) );
                            QTextCursor cursor = ui->textEdit_2->textCursor();
                            QTextImageFormat imageFormat;
                            imageFormat.setWidth( i.width() );
                            imageFormat.setHeight( i.height() );
                            cursor.insertImage(imageFormat);
                        }
                    }
                    else
                    {
                        console("[error] convert from data to image", "#f25c5c");
                        ui->textEdit_2->setText(QString::fromStdString(img));
                    }

                }
                else if (ui->comboBox_2->currentText().compare("sdb") == 0 || ui->comboBox_2->currentText().compare("Json") == 0 || ui->comboBox_2->currentText().compare("jdb") == 0 || ui->comboBox_2->currentText().compare("ajdb") == 0)
                {
                    if (is_save)
                    {
                        QString to_save = QFileDialog::getSaveFileName(this, "Save Json File", QString(), "Json (*.json)");
                        if (!to_save.isEmpty())
                        {
                            basefunc_std::write_file_to_disk(to_save.toStdString(), content);
                        }
                    }
                    else
                    {
                        if (ui->comboBox_2->currentText().compare("Json") == 0)
                        {
                            content = "[" + content + "]";
                        }

                        try
                        {
                            Json::Value root;
                            bool parsingSuccessful = Json::Reader().parse(content, root);
                            if (parsingSuccessful)
                            {
                                ui->textEdit_2->setText(QString::fromStdString(Json::StyledWriter().write(root)));
                                //ui->textEdit_2->setText(QString::fromStdString(content));
                            }
                            else
                            {
                                console("[error] parse json failed", "#f25c5c");
                            }
                        }
                        catch(std::exception& ex)
                        {
                            console(QString("[error] ") + QString(ex.what()), "#f25c5c");
                        }
                    }
                }
                else if (ui->comboBox_2->currentText().compare("Text") == 0)
                {
                    if (is_save)
                    {
                        QString to_save = QFileDialog::getSaveFileName(this, "Save Text File", QString(), "Text (*.txt)");
                        if (!to_save.isEmpty())
                        {
                            basefunc_std::write_file_to_disk(to_save.toStdString(), content);
                        }
                    }
                    else
                    {
                        ui->textEdit_2->setText(QString::fromStdString(content));
                    }
                }
            }
            else
            {
                console("[error] content is empty", "#f25c5c");
            }
        }
        else
        {
            console(QString("[file] error status ") + QString::number(static_cast<int>(res.status)));
        }
    }
}

void MainWindow::remove_file(std::uint32_t suuid, const std::string& filename)
{
    ui->textEdit_2->clear();

    {
        rssdisk::pool::guard g = pl.get();
        if (g.get() == nullptr)
        {
            console("[error] cant get pool guard", "Red");
            return;
        }

        console(QString("[remove_file] ") + QString::fromStdString(filename) + " on " + QString::fromStdString(network_std::inet_ntoa(suuid)));

        ::timer tm;

        std::vector<rssdisk::read_info> info;
        rssdisk::client::read_res res = g->command(rssdisk::client::read_info_type::remove_file, info, filename, std::numeric_limits<std::int32_t>::max(), 10000, {}, rssdisk::client::rw_preference::any, { suuid });

        if (res == rssdisk::client::read_res::ok)
        {
            console(QString("[remove] ok, elapsed, ms: ") + QString::number(tm.elapsed_mili()));
        }
        else
        {
            console(QString("[remove] error status ") + QString::number(static_cast<int>(res)));
        }
    }
}

void MainWindow::on_lineEdit_returnPressed()
{
    on_pushButton_2_clicked();
}

void MainWindow::on_pushButton_4_clicked()
{
    {
        rssdisk::pool::guard g = pl.get();
        if (g.get() == nullptr)
        {
            console("[error] cant get pool guard", "Red");
            return;
        }

        console(QString("[re-open tcp] starting ..."));

        g->re_open_tcp();
    }

    on_pushButton_clicked();
}

void MainWindow::on_pushButton_5_clicked()
{
    {
        rssdisk::pool::guard g = pl.get();
        if (g.get() == nullptr)
        {
            console("[error] cant get pool guard", "Red");
            return;
        }

        std::vector<rssdisk::client::read_responce_tcp> s = g->get_tcp_statuses();
        for (const rssdisk::client::read_responce_tcp& item : s)
        {
            QString text = "[state] " + QString::fromStdString(network_std::inet_ntoa(item.ip_address_readed)) + " ";
            switch (item.status)
            {
                case tcp_client::socket_status::connected: text += "connected"; break;
                case tcp_client::socket_status::not_connected: text += "not_connected"; break;
                case tcp_client::socket_status::waiting_for_auth: text += "waiting_for_auth"; break;
                case tcp_client::socket_status::waiting_for_encryption_established: text += "waiting_for_encryption_established"; break;
                default: text += "unknown"; break;
            }
            console(text);
        }
    }
}


//save
void MainWindow::on_pushButton_7_clicked()
{
    std::string filename = QFileDialog::getOpenFileName(this, "Open data file", "/", "*").toStdString();
    if (!filename.empty())
    {
        API::base64 b64;
        std::string content = basefunc_std::read_file(filename);

        if (ui->comboBox_2->currentText().compare("Image") == 0)
        {
            content = b64.base64_encode(content);
        }

        if (!content.empty())
        {
            std::string content_sett;
            if (ui->comboBox_2->currentText().compare("jdb") == 0 || ui->comboBox_2->currentText().compare("ajdb") == 0)
            {
                std::string filename_sett = QFileDialog::getOpenFileName(this, "Open settings file", "/", "*").toStdString();
                if (!filename_sett.empty())
                {
                    content_sett = basefunc_std::read_file(filename_sett);
                }
            }

            save_file(content, content_sett);
        }
    }

}

void MainWindow::save_file(std::string content, std::string content_sett)
{
    if (!content.empty())
    {
        rssdisk::w_type type_operation { rssdisk::w_type::updatable };

        std::string to_log { "updatable" };

        if (ui->comboBox_2->currentText().compare("jdb") == 0 || ui->comboBox_2->currentText().compare("ajdb") == 0)
        {
            type_operation = rssdisk::w_type::jdb;
            to_log = "jdb";

            if (ui->comboBox_2->currentText().compare("ajdb") == 0)
            {
                type_operation = rssdisk::w_type::ajdb;
                to_log = "ajdb";
            }

            if (content_sett.empty()) content_sett = "{}";

            content = rssdisk::client::prepare_jdb(content, content_sett);
        }

        rssdisk::pool::guard g = pl.get();
        if (g.get() == nullptr)
        {
            console("[error] cant get pool guard", "Red");
            return;
        }

        rssdisk::client::rw_preference rp = rssdisk::client::rw_preference::any;
        std::vector<std::int32_t> groups;

        std::string log_group { "any" };

        QString _gr = ui->comboBox_6->currentText();
        if (_gr != "")
        {
            auto exp = basefunc_std::split(_gr.toStdString(), ',');
            for (const std::string& _g : exp)
            {
                std::int32_t g { 0 };
                basefunc_std::stoi(_g, g);

                groups.push_back(g);
            }
            rp = rssdisk::client::rw_preference::specified_group;
            log_group = "specified_group";
        }

        QString search;
        search += ui->lineEdit->text();

        std::string filename = search.toStdString();



        rssdisk::client::write_options w_opt;
        w_opt.part_timeout_milisec = 10000;
        w_opt.preffered_netwok_groups = groups;
        w_opt.rw_pref = rp;

        {
            std::lock_guard<std::mutex> _{ _lock_ui };
            QLayoutItem* item;
            auto sz = l->count();
            for (auto i = 0; i < sz; ++i)
            {
                item = l->itemAt(i);
                server_item2* it = dynamic_cast<server_item2*>(item->widget());
                if (it != nullptr)
                {
                    if (it->get_is_write_selected()) w_opt.only_ips.emplace(it->get_suuid());
                }
            }
        }

        if (ui->lineEdit_3->text() != "")
        {
            basefunc_std::stoi(ui->lineEdit_3->text().toStdString(), w_opt.count_server_to_write);
        }
        else
        {
            if (!w_opt.only_ips.empty()) w_opt.count_server_to_write = w_opt.only_ips.size();
        }

        rssdisk::client::write_info ok = g->write_file(type_operation, filename, content, w_opt);

        if (ok.writed_count >= w_opt.count_server_to_write)
        {
            console2("[success] File (" + filename + ") has been writed with no ttl, type " + to_log + ", rw_preference " + log_group + ", on " + std::to_string(ok.writed_count) + " servers");
        }
        else
        {
            console2("[error] File (" + filename + ") has been writed with no ttl, type " + to_log + ", rw_preference " + log_group + ", on " + std::to_string(ok.writed_count) + " servers", "Red");
        }
    }
}

void MainWindow::on_pushButton_8_clicked()
{
    if (last_server != 0)
    {
        open_file(last_server, last_filename, true, true);
    }
}

void MainWindow::on_comboBox_2_currentIndexChanged(int index)
{
    if (ui->comboBox_2->currentText().compare("jdb") == 0 || ui->comboBox_2->currentText().compare("ajdb") == 0 || ui->comboBox_2->currentText().compare("sdb") == 0)
    {
        if (ui->comboBox_2->currentText().compare("jdb") == 0 || ui->comboBox_2->currentText().compare("ajdb") == 0)
        {
            ui->textEdit_3->setVisible(true);
            ui->textEdit_2->setReadOnly(false);

            console("[example] To create index type: {\"index\":[{\"n\":\"o\"},{\"n\":\"d\",\"t\":\"date\"}]}. Where n - name of index fields, t - type of index (supported int and long by default, t: date - for mongo date index, t: string)", "Green");
        }
        else
        {
            ui->textEdit_3->setVisible(true);
            ui->textEdit_2->setReadOnly(false);
        }
    }
    else
    {
        ui->textEdit_3->setVisible(false);
        ui->textEdit_2->setReadOnly(true);
    }
}

/**
 * save jdb
 */
void MainWindow::on_pushButton_10_clicked()
{
    std::string content = ui->textEdit_2->toPlainText().toStdString();

    basefunc_std::trim(content);

    if (content.size() < 2) return;
    if (content[0] != '[' || content[content.size() - 1] != ']')
    {
        console("[error] Context must be an array", "Red");
        return;
    }

    try
    {
        Json::Value root;
        bool parsingSuccessful = Json::Reader().parse(content, root);
        if (!parsingSuccessful) throw std::runtime_error("Failed to parse configuration");
    }
    catch(std::exception& ex)
    {
        console(QString("[error] Error to parse jdb (ajdb) content: ") + ex.what(), "Red");
        return;
    }

    save_file(content, ui->textEdit_3->toPlainText().toStdString());
}

void MainWindow::timer()
{
    std::lock_guard<std::mutex> _{ _lock_ui };

    bool is_data = !_data_stat.empty();

    QLayoutItem* item;
    auto sz = l->count();
    for (auto i = 0; i < sz; ++i)
    {
        item = l->itemAt(i);
        server_item2* it = dynamic_cast<server_item2*>(item->widget());
        if (it != nullptr)
        {
            if (is_data)
            {
                const auto& data = _data_stat[it->get_suuid()];
                it->set_data(data.first);
            }
        }
    }

    _data_stat.clear();
}

void MainWindow::set_item_state(const std::uint32_t uuid, bool status)
{
    std::lock_guard<std::mutex> _{ lock_all_statuses };
    all_statuses[uuid] = status;
}

void MainWindow::on_comboBox_6_currentIndexChanged(int index)
{
    if (!is_inited) return;

    on_pushButton_clicked();

    QSettings settings { QCoreApplication::applicationDirPath() + "/settings.ini", QSettings::IniFormat };
    settings.setValue("selected_group", index);
    settings.sync();
}
