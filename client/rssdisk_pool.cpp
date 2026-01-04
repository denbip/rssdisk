#include "rssdisk_pool.hpp"
using namespace rssdisk;

NotificationHandler::CallbackFunction NotificationHandler::notifyCallback = nullptr;

std::string rssdisk::pool::default_prime_number;

const crc32m client::CRC32;

client::client()
{

}

client::~client()
{
    stop();
}

bool client::init_settings(const std::string& config_file, std::vector<std::int32_t> netwok_groups)
{
    std::vector<std::shared_ptr<tcp_client> > clients_;

    try
    {
        std::ifstream t(config_file);
        if (!t) return false;
        std::string c((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

        Json::Value root;
        bool parsingSuccessful = Json::Reader().parse(c, root);
        if (!parsingSuccessful) throw std::runtime_error("Failed to parse configuration");

        Json::Value::Members m = root.getMemberNames();
        for (int i = 0; i < m.size(); ++i)
        {
            std::string ip = m[i];
            std::int32_t port = root[m[i]]["tcp"]["port"].asInt();

            std::string delimeter = root[m[i]]["tcp"]["delimeter"].asString();
            std::string echo = root[m[i]]["tcp"]["echo"].asString();
            std::string auth = root[m[i]]["secure"]["auth"].asString();
            std::int32_t network_group = root[m[i]]["groups"]["network"].asInt();
            bool use_in_any_rp = root[m[i]]["groups"].get("use_in_any_rp", false).asBool();
            bool disabled_for_clients = root[m[i]].get("disabled_for_clients", false).asBool();
            bool gost_enc = root[m[i]]["secure"].get("gost_enc", false).asBool();

            std::int32_t count_timeouts { 10 };
            std::int32_t disable_for_minutes { 60 };
            if (root[m[i]].isMember("timeout"))
            {
                if (root[m[i]]["timeout"].isMember("count_timeouts")) count_timeouts = root[m[i]]["timeout"]["count_timeouts"].asInt();
                if (root[m[i]]["timeout"].isMember("disable_for_minutes")) disable_for_minutes = root[m[i]]["timeout"]["disable_for_minutes"].asInt();
            }

            if (!disabled_for_clients)
            {
                if (netwok_groups.empty() || std::find(netwok_groups.begin(), netwok_groups.end(), network_group) != netwok_groups.end())
                {
                    std::shared_ptr<tcp_client> cl  = std::make_shared<tcp_client>(ip, port, 5, delimeter, auth, echo, true, network_group, use_in_any_rp);

                    cl->set_use_gost_encryption(gost_enc);
                    cl->timeout.set_count_timeouts(count_timeouts);
                    cl->timeout.set_minutes_timeout(disable_for_minutes);

                    //cl->set_enabled_send_recv_control(true);
                    cl->set_default_prime_number(rssdisk::pool::get_default_prime_number());

                    cl->timeout.set_state_blocked_callback_callback([ip]()
                    {
                        NotificationHandler::notify("Detected timeout " + ip, "rssdisk", "rssdisk", 1);
                    });

                    clients_.push_back(cl);
                }
            }
        }

    }
    catch(std::exception& ex)
    {
        basefunc_std::cout(std::string("cant_load_settings ") + std::string(ex.what()), "rssdisk::pool", basefunc_std::COLOR::RED_COL);
        NotificationHandler::notify(std::string("cant_load_settings ") + std::string(ex.what()), "pool", "rssdisk", 1);
        return false;
    }

    std::swap(clients, clients_);

    return true;
}

void client::wait_for_init_connections(const std::set<std::int32_t>& wait_network_groups, std::int32_t wait_ms, bool alert_not_connected)
{
    std::mutex lock_cond;
    std::condition_variable cond;
    std::map<std::int32_t, std::uint64_t> callback_ids;
    std::atomic_int counts = ATOMIC_VAR_INIT(0);

    std::mutex lock_ip;
    std::set<std::string> ips;

    for (int i = 0; i < clients.size(); ++i)
    {
        std::shared_ptr<tcp_client> c = clients[i];

        if (wait_ms > 0 && ((wait_network_groups.empty() && c->can_use_in_any_rp()) || wait_network_groups.count(c->get_network_group()) != 0))
        {
            std::uint64_t callback_id = c->set_status_callback([&counts, &cond, &lock_ip, &ips](const std::string& ip, const tcp_client::socket_status& old_status, const tcp_client::socket_status& new_status)
            {
                if (new_status == tcp_client::socket_status::connected)
                {
                    {
                        std::lock_guard<std::mutex> _{ lock_ip };
                        ips.erase(ip);
                    }

                    --counts;
                    cond.notify_all();
                }

                /* ||
                   (old_status == tcp_client::socket_status::waiting_for_auth && old_status == tcp_client::socket_status::not_connected) ||
                   (old_status == tcp_client::socket_status::waiting_for_encryption_established && old_status == tcp_client::socket_status::not_connected)*/
            });

            {
                std::lock_guard<std::mutex> _{ lock_ip };
                ips.emplace(c->get_ip());
            }

            ++counts;
            callback_ids.insert( { i, callback_id } );
        }

        c->start();
    }

    if (wait_ms > 0)
    {
        std::unique_lock<std::mutex> lock { lock_cond };
        if (!cond.wait_for(lock, std::chrono::milliseconds(wait_ms / 2), [&counts](){ return counts == 0; }))
        {
            if (callback_ids.size() - counts > 3)
            {
                cond.wait_for(lock, std::chrono::milliseconds(wait_ms / 2), [&counts](){ return counts == 0; });
            }
            else
            {

            }

            if (alert_not_connected && counts != 0)
            {
                std::string ips_str;
                {
                    std::lock_guard<std::mutex> _{ lock_ip };
                    ips_str = basefunc_std::get_string_from_set(ips);
                }

                basefunc_std::cout(std::to_string(counts) + " (" + ips_str + ") were not connected at init", "rssdisk::pool::init", basefunc_std::COLOR::RED_COL);
                NotificationHandler::notify(std::to_string(counts) + " (" + ips_str + ") were not connected at init", "rssdisk::pool::init", "rssdisk", 1);
            }
        }

        for (const auto& it : callback_ids) clients[it.first]->unset_status_callback(it.second);
    }
}

bool client::init(const string &config_file, std::vector<int32_t> netwok_groups)
{
    if (!init_settings(config_file, netwok_groups)) return false;
    wait_for_init_connections({});
    return true;
}

void client::stop()
{
    for (int i = 0; i < clients.size(); ++i)
    {
        clients[i]->stop();
    }
}

void client::fetch_edb_content(const std::string& data, std::size_t start_position, std::function<void(const date_time& dt, const streamer<>& d)> f)
{
    while (data.size() > start_position + 4)
    {
        std::int32_t data_size { 0 };
        bitbase::chars_to_numeric(data.substr(start_position, 4), data_size);

        if (data.size() >= start_position + data_size + 4)
        {
            if (data_size >= 12)
            {
                std::int64_t t { 0 };
                bitbase::chars_to_numeric(data.substr(start_position + 8, 8), t);

                f(date_time::current_date_time(t), { data.substr(start_position + 16, data_size - 12) } );

                start_position += data_size + 4;
            }
            else
            {
                basefunc_std::cout("Error data_size: " + std::to_string(data_size) + " while data.size = " + std::to_string(data.size()) + " " + data, "fetch_edb_content", basefunc_std::COLOR::RED_COL);
                break;
            }
        }
        else
        {
            basefunc_std::cout("Error data_size: " + std::to_string(data_size) + " while data.size = " + std::to_string(data.size()) + " " + data, "fetch_edb_content", basefunc_std::COLOR::RED_COL);
            break;
        }
    }
}

std::string client::prepare_edb(const std::int32_t type_event, const std::string& val, std::int32_t ttl)
{
    std::string data { bitbase::numeric_to_chars(std::int32_t(ttl)) + bitbase::numeric_to_chars(std::int64_t(time(0))) + val };
    return bitbase::numeric_to_chars(type_event) + bitbase::numeric_to_chars(static_cast<std::int32_t>(data.size())) + data;
}

std::string client::prepare_cdb(const std::string& key, const std::string& val, std::int32_t ttl)
{
    std::string val_compressed = commpression_zlib::compress_string(val);
    return bitbase::numeric_to_chars(static_cast<std::int64_t>(time(0))) + bitbase::numeric_to_chars(static_cast<std::int64_t>(client::CRC32.get_hash(val))) + bitbase::numeric_to_chars(static_cast<std::int32_t>(ttl)) + bitbase::numeric_to_chars(static_cast<std::int32_t>(key.size())) + bitbase::numeric_to_chars(static_cast<std::int32_t>(val_compressed.size())) + key + val_compressed;
}

client::write_info client::write_file(w_type write_type, const std::string& filename, const std::string& content_, write_options w_opt)
{
    client::write_info ret;

    //format 04identyauth filename ttl_days flags content
    std::map<std::int32_t, std::vector<std::int32_t> > servers_pool_indexex; // = get_free_size_servers();
    std::int32_t count_servers_pool_indexex { 0 };

    //sub folders function
    auto f_subfolder = filename.find("/");
    std::string subfolder;
    if (f_subfolder != std::string::npos)
    {
        subfolder = filename.substr(0, f_subfolder);
    }

    {
        std::lock_guard<std::mutex> _{ lock_disabled_write_clients };
        for (int i = 0; i < clients.size(); ++i)
        {
            if (write_type != rssdisk::w_type::cdb && write_type != rssdisk::w_type::cdbm)
            {
                auto f = disabled_write_clients.find(i);
                if (f != disabled_write_clients.end())
                {
                    auto f_d_sub = f->second.find(subfolder);
                    if (f_d_sub != f->second.end())
                    {
                        if (f_d_sub->second.date_ > date_time::current_date_time().date_) continue;
                    }
                }
            }

            std::shared_ptr<tcp_client> c = clients[i];

            if (c->get_socket_state() == tcp_client::socket_status::connected && !c->timeout.is_timeout_detected())
            {
                if ((w_opt.rw_pref == rw_preference::any && c->can_use_in_any_rp()) ||
                    std::find(w_opt.preffered_netwok_groups.begin(), w_opt.preffered_netwok_groups.end(), c->get_network_group()) != w_opt.preffered_netwok_groups.end())
                {
                    if (w_opt.only_ips.empty() || w_opt.only_ips.count(c->get_uip()) != 0)
                    {
                        servers_pool_indexex[c->get_network_group()].push_back(i);
                        ++count_servers_pool_indexex;
                    }
                }
            }
        }
    }

    if (w_opt.count_server_to_write > count_servers_pool_indexex)
    {
        return { -1 };
    }

    std::string content;
    if (!content_.empty())
    {
        if (write_type != w_type::updatable_without_compress && write_type != w_type::insert_only_without_compress && write_type != w_type::jdb && write_type != w_type::ajdb && write_type != w_type::cdb && write_type != w_type::cdbm && write_type != w_type::appendable) content = commpression_zlib::compress_string(content_);
        else content = content_;
        if (content.empty()) return { -2 };
    }

    ret.file_tms = time(0);
    ret.file_crc32 = client::CRC32.get_hash(content_);

    std::atomic_int count_ok = ATOMIC_VAR_INIT(0);
    std::atomic_int count_awainting_responses = ATOMIC_VAR_INIT(0);
    std::mutex lock_cond;
    std::condition_variable cond;

    std::mutex lock_netw_gr;
    std::set<std::int32_t> newt_gr_ok;
    std::set<std::uint32_t> ips_ok;

    std::vector<std::shared_ptr<tcp_client::async_send_item> > v_async_send_item;
    std::map<std::int32_t, std::uint64_t> callback_ids;
    std::unordered_set<std::int32_t> used_groups;

    bool _wait_responce { true };
    if (w_opt.w_answer == write_options::wait_answer::no_wait)
    {
        _wait_responce = false;
    }
    else if (w_opt.w_answer == write_options::wait_answer::default_)
    {
        if (write_type == w_type::edb || write_type == w_type::cdb || write_type == w_type::cdbm)
        {
            _wait_responce = false;
        }
    }

    auto f_send = [this, &_wait_responce, &servers_pool_indexex, &used_groups, &write_type, &filename, &w_opt, &content, &v_async_send_item, &callback_ids, &count_awainting_responses, &cond, &count_ok, &lock_netw_gr, &newt_gr_ok, &subfolder ,&ret, &ips_ok]
                  (std::int32_t need_to_send, std::vector<int32_t> preffered_netwok_groups_) -> bool
    {
        while (need_to_send > 0)
        {
            //get preffered network group;
            std::int32_t preffered_network { -1 };
            if (!preffered_netwok_groups_.empty())
            {
                preffered_network = preffered_netwok_groups_[0];
                preffered_netwok_groups_.erase(preffered_netwok_groups_.begin());
            }

            std::int32_t ind = pop_index_rand(servers_pool_indexex, used_groups, preffered_network);

            if (ind == -1) break;
            std::shared_ptr<tcp_client> c = clients[ind];
            tcp_client::socket_status state = c->get_socket_state();
            if (state == tcp_client::socket_status::connected)
            {
                const std::int32_t netw_gr_cl = c->get_network_group();
                const std::int32_t identy = c->get_identy();
                const std::uint32_t ip { c->get_uip() };

                //set read callback
                if (_wait_responce)
                {
                    std::uint64_t callback_id = c->set_read_callback([this, ip, ind, identy, netw_gr_cl, &write_type, &count_awainting_responses, &cond, &count_ok, &lock_netw_gr, &newt_gr_ok, &filename, &subfolder, &ret, &ips_ok](const std::string& message_)
                    {
                        if (message_.size() >= 9)
                        {
                            std::string status = message_.substr(2, 2);
                            std::string identy_ = message_.substr(4, 5);

                            if (identy_.compare(std::to_string(identy)) == 0) //se puede ser llegaran mas veijas respondes
                            {
                                if (status.compare("01") == 0)
                                {
                                    ++count_ok;
                                    std::lock_guard<std::mutex> _{ lock_netw_gr };
                                    newt_gr_ok.emplace(netw_gr_cl);
                                    ips_ok.emplace(ip);
                                }
                                else if (status.compare("03") == 0 || status.compare("12") == 0 || status.compare("14") == 0)
                                {
                                    std::lock_guard<std::mutex> _{ lock_disabled_write_clients };
                                    disabled_write_clients[ind][subfolder] = date_time::current_date_time().add_days(1);
                                }
                                else if (status.compare("09") == 0)
                                {
                                    NotificationHandler::notify("Filename too long: " + filename, "pool", "rssdisk", 1);
                                }
                                else
                                {
                                    std::shared_ptr<tcp_client> c = clients[ind];
                                    basefunc_std::log(c->get_ip() + " " + filename + " " + message_, "rssdisk_pool/write_error", true, basefunc_std::COLOR::RED_COL);
                                }

                                --count_awainting_responses;
                                cond.notify_all();
                            }
                        }
                        else
                        {
                            basefunc_std::log(message_, "rssdisk_pool/write_error_size", true, basefunc_std::COLOR::RED_COL);
                        }
                    });

                    callback_ids.insert( { ind, callback_id } );
                }

                ////type [01] tms [00000000] crc32 [00000000]
                //set write
                std::string flags { "0" };
                if (write_type == w_type::updatable || write_type == w_type::updatable_without_compress || write_type == w_type::jdb || write_type == w_type::ajdb)
                {
                    if (write_type == w_type::jdb)
                    {
                        if (content.size() >= 3 && content.substr(0, 3).compare("jdb") == 0) flags += std::to_string(static_cast<int>(w_type::jdb_formed)); //jdb formed
                        else flags += std::to_string(static_cast<int>(write_type));
                    }
                    else
                    {
                        flags += std::to_string(static_cast<int>(write_type));
                    }

                    flags += bitbase::numeric_to_chars(ret.file_tms) + bitbase::numeric_to_chars(ret.file_crc32);
                }
                else if (write_type == w_type::insert_only_without_compress)
                {
                    flags += std::to_string(static_cast<int>(write_type));
                }
                else if (write_type == w_type::appendable)
                {
                    flags = std::to_string(static_cast<int>(write_type));
                    flags += bitbase::numeric_to_chars(ret.file_tms);
                }

                std::string to_write;

                if (write_type == w_type::edb)
                {
                    if (_wait_responce) to_write = "12";
                    else to_write = "22";

                    to_write += std::to_string(identy) + filename + content;
                }
                else if (write_type == w_type::cdb) //tms[00000000] crc32[00000000] ttl[0000] filename_size[0000] content_size[0000] key content
                {
                    if (_wait_responce) to_write = "32";
                    else to_write = "33";

                    to_write += std::to_string(identy) + client::prepare_cdb(filename, content, w_opt.ttl);
                }
                else if (write_type == w_type::cdbm) //tms[00000000] crc32[00000000] ttl[0000] filename_size[0000] content_size[0000] key content
                {
                    if (_wait_responce) to_write = "32";
                    else to_write = "33";

                    to_write += std::to_string(identy) + content;
                }
                else
                {
                    to_write = "04";

                    ((((((((to_write += std::to_string(identy)) += "auth ") += filename) += " ") += std::to_string(w_opt.ttl)) += " ") += flags) += " ") += content;
                }

                std::shared_ptr<tcp_client::async_send_item> async_send = std::make_shared<tcp_client::async_send_item>(std::move(to_write), identy,
                                                                                                                        [ip, netw_gr_cl, &newt_gr_ok, &lock_netw_gr, &count_ok, &_wait_responce, &count_awainting_responses, &cond, &ips_ok](std::int32_t identy, const tcp_client::status& sts)
                {
                    if (!_wait_responce)
                    {
                        if (sts == tcp_client::status::ok)
                        {
                            ++count_ok;
                            {
                                std::lock_guard<std::mutex> _{ lock_netw_gr };
                                newt_gr_ok.emplace(netw_gr_cl);
                                ips_ok.emplace(ip);
                            }

                            --count_awainting_responses;
                            cond.notify_all();
                        }
                    }
                });

                --need_to_send;
                ++count_awainting_responses;
                v_async_send_item.push_back(async_send);

                ++ret.count_attempts;

                c->send_async(async_send);
            }
        }

        return need_to_send == 0;
    };

    std::int32_t loop_send { w_opt.count_server_to_write };
    for (int i = 0; i < w_opt.count_parts; ++i)
    {
        if (servers_pool_indexex.empty()) break;

        count_awainting_responses.store(0);

        if (i == 0) f_send(loop_send, w_opt.preffered_netwok_groups);
        else
        {
            std::vector<int32_t> preffered_netwok_groups_;
            {
                std::lock_guard<std::mutex> _{ lock_netw_gr };
                for (std::int32_t p_i : w_opt.preffered_netwok_groups)
                {
                    if (newt_gr_ok.count(p_i) == 0) preffered_netwok_groups_.push_back(p_i);
                }
            }
            f_send(loop_send, preffered_netwok_groups_);
        }

        //wait
        std::unique_lock<std::mutex> lock { lock_cond };
        if(cond.wait_for(lock, std::chrono::milliseconds(w_opt.part_timeout_milisec), [&count_awainting_responses]
        {
            return count_awainting_responses <= 0;
        }))
        {

        }

        if (count_ok >= w_opt.count_server_to_write)
        {
            break; //all has been written
        }

        loop_send = w_opt.count_server_to_write - count_ok; //request to new servers to write
    }

    if (write_type == w_type::cdb || write_type == w_type::cdbm)
    {
        for (int i = 0; i < clients.size(); ++i)
        {
            std::shared_ptr<tcp_client> c = clients[i];

            if (ips_ok.count(c->get_uip()) != 0) continue;

            if (c->get_socket_state() == tcp_client::socket_status::connected && !c->timeout.is_timeout_detected())
            {
                if ((w_opt.rw_pref == rw_preference::any && c->can_use_in_any_rp()) ||
                    std::find(w_opt.preffered_netwok_groups.begin(), w_opt.preffered_netwok_groups.end(), c->get_network_group()) != w_opt.preffered_netwok_groups.end())
                {
                    if (w_opt.only_ips.empty() || w_opt.only_ips.count(c->get_uip()) != 0)
                    {
                        const std::int32_t identy = c->get_identy();

                        std::string remover;
                        if (write_type == w_type::cdb)
                        {
                            remover = "33" + std::to_string(identy) + client::prepare_cdb(filename, "", 1);;
                        }
                        else if (write_type == w_type::cdbm)
                        {
                            std::string cutted;

                            std::int32_t pos { 0 };
                            while (true)
                            {
                                if (content.size() > pos + 28)
                                {
                                    const std::int32_t block_pos { pos };
                                    std::int32_t header_bytes { 0 };
                                    std::int32_t content_bytes { 0 };

                                    pos += 20;
                                    bitbase::chars_to_numeric(content.substr(pos, 4), header_bytes);
                                    pos += 4;
                                    bitbase::chars_to_numeric(content.substr(pos, 4), content_bytes);
                                    pos += 4;

                                    if (content.size() >= pos + header_bytes)
                                    {
                                        pos += header_bytes;

                                        if (content.size() >= pos + content_bytes)
                                        {
                                            cutted += content.substr(block_pos, 24);
                                            cutted += bitbase::numeric_to_chars(std::int32_t(0));
                                            cutted += content.substr(block_pos + 28, header_bytes);

                                            pos += content_bytes;

                                        }
                                        else
                                        {
                                            break;
                                        }
                                    }
                                    else
                                    {
                                        break;
                                    }
                                }
                                else
                                {
                                    break;
                                }
                            }

                            remover = "33" + std::to_string(identy) + cutted;
                        }

                        std::shared_ptr<tcp_client::async_send_item> async_send = std::make_shared<tcp_client::async_send_item>(std::move(remover), identy, nullptr);
                        c->send_async(async_send);
                    }
                }
            }
        }
    }

    for (int i = 0; i < v_async_send_item.size(); ++i)
    {
        v_async_send_item[i]->unset_awaiting_send();
        v_async_send_item[i]->unset_callback();
    }

    for (const auto& it : callback_ids)
    {
        std::shared_ptr<tcp_client> c = clients[it.first];
        c->unset_read_callback(it.second);
    }

    ret.writed_count = count_ok;
    {
        std::lock_guard<std::mutex> _{ lock_netw_gr };
        ret.writed_ips = ips_ok;
    }

    return ret;
}

std::vector<client::read_responce> client::get_servers_statuses(std::int32_t timeout_mili_sec)
{
    std::mutex lock_indexex;
    std::vector<client::read_responce> indexex;
    std::unordered_set<std::string> setted_resp;
    std::atomic_int count_awainting_responses = ATOMIC_VAR_INIT(0);

    std::mutex lock_cond;
    std::condition_variable cond;

    std::vector<std::shared_ptr<tcp_client::async_send_item> > v_async_send_item;

    auto f_set_status = [this, &lock_indexex, &setted_resp, &indexex](std::int32_t i, read_res r, std::string data = "") -> void
    {
        std::string ip_s = clients[i]->get_ip();

        std::lock_guard<std::mutex> _{ lock_indexex };
        if (setted_resp.count(ip_s) == 0)
        {
            setted_resp.emplace(ip_s);

            client::read_responce rp { r, network_std::inet_aton(ip_s) };
            rp.data = std::move(data);
            rp.group = clients[i]->get_network_group();

            indexex.push_back(std::move(rp));
        }
    };

    std::map<std::int32_t, std::uint64_t> callback_ids;
    for (int i = 0; i < clients.size(); ++i)
    {
        std::shared_ptr<tcp_client> c = clients[i];
        if (c->get_socket_state() == tcp_client::socket_status::connected && !c->timeout.is_timeout_detected())
        {
            std::int32_t identy = c->get_identy();
            std::string to_write { "05" + std::to_string(identy) };

            std::uint64_t callback_id = c->set_read_callback([identy, i, &indexex, &lock_indexex, &count_awainting_responses, &cond, &setted_resp, &f_set_status](const std::string& message_)
            {
                read_res r { read_res::error_read_responce_from_server };
                std::string data;

                if (message_.size() >= 9)
                {
                    std::string status = message_.substr(2, 2);
                    std::string identy_ = message_.substr(4, 5);

                    if (status.compare("01") == 0)
                    {
                        if (identy_.compare(std::to_string(identy)) == 0)
                        {
                            data = message_.substr(9);
                            r = read_res::ok;
                        }
                    }
                }

                f_set_status(i, r, data);

                --count_awainting_responses;
                cond.notify_all();
            });

            callback_ids.insert( { i, callback_id } );

            std::shared_ptr<tcp_client::async_send_item> async_send = std::make_shared<tcp_client::async_send_item>(std::move(to_write), identy,
                                                                                                                    [&f_set_status, &count_awainting_responses, &cond, i, &indexex, &lock_indexex, &setted_resp](std::int32_t identy, const tcp_client::status& sts)
            {
                if (sts != tcp_client::status::ok)
                {
                    f_set_status(i, read_res::error_send_request_from_server);
                    --count_awainting_responses;
                    cond.notify_all();
                }
            });

            v_async_send_item.push_back(async_send);
            ++count_awainting_responses;
            c->send_async(async_send);
        }
        else
        {
            f_set_status(i, read_res::error_to_connect_to_server);
        }
    }

    //wait
    std::unique_lock<std::mutex> lock { lock_cond };
    if(cond.wait_for(lock, std::chrono::milliseconds(timeout_mili_sec), [&count_awainting_responses]{ return count_awainting_responses == 0; }))
    {
         //std::cout << "ok " << count_awainting_responses << std::endl;
    }
    else
    {
         //std::cout << "timout " << count_awainting_responses << std::endl;
    }

    for (int i = 0; i < v_async_send_item.size(); ++i)
    {
        v_async_send_item[i]->unset_awaiting_send();
        v_async_send_item[i]->unset_callback();
    }

    for (const auto& it : callback_ids)
    {
        std::shared_ptr<tcp_client> c = clients[it.first];
        c->unset_read_callback(it.second);
    }

    //set to others timeout status
    for (int i = 0; i < clients.size(); ++i)
    {
        f_set_status(i, read_res::timeout);
    }

    return indexex;
}

std::vector<client::read_responce> client::get_servers_ping(std::int32_t timeout_mili_sec)
{
    std::mutex lock_indexex;
    std::vector<client::read_responce> indexex;
    std::unordered_set<std::string> setted_resp;
    std::atomic_int count_awainting_responses = ATOMIC_VAR_INIT(0);

    std::mutex lock_cond;
    std::condition_variable cond;

    std::vector<std::shared_ptr<tcp_client::async_send_item> > v_async_send_item;

    timer tm;

    auto f_set_status = [this, &lock_indexex, &setted_resp, &indexex, &tm](std::int32_t i, read_res r) -> void
    {
        std::string ip_s = clients[i]->get_ip();

        std::lock_guard<std::mutex> _{ lock_indexex };
        if (setted_resp.count(ip_s) == 0)
        {
            setted_resp.emplace(ip_s);

            client::read_responce rp { r, network_std::inet_aton(ip_s) };
            if (r == read_res::ok) rp.data = std::to_string(tm.elapsed_mili());
            rp.group = clients[i]->get_network_group();

            indexex.push_back(std::move(rp));
        }
    };

    std::map<std::int32_t, std::uint64_t> callback_ids;
    for (int i = 0; i < clients.size(); ++i)
    {
        std::shared_ptr<tcp_client> c = clients[i];
        if (c->get_socket_state() == tcp_client::socket_status::connected && !c->timeout.is_timeout_detected())
        {
            std::int32_t identy = c->get_identy();
            std::string to_write { "26" + std::to_string(identy) };

            std::uint64_t callback_id = c->set_read_callback([identy, i, &indexex, &lock_indexex, &count_awainting_responses, &cond, &setted_resp, &f_set_status](const std::string& message_)
            {
                read_res r { read_res::error_read_responce_from_server };

                if (message_.size() >= 9)
                {
                    std::string status = message_.substr(2, 2);
                    std::string identy_ = message_.substr(4, 5);

                    if (status.compare("01") == 0)
                    {
                        if (identy_.compare(std::to_string(identy)) == 0)
                        {
                            r = read_res::ok;
                        }
                    }
                }

                f_set_status(i, r);

                --count_awainting_responses;
                cond.notify_all();
            });

            callback_ids.insert( { i, callback_id } );

            std::shared_ptr<tcp_client::async_send_item> async_send = std::make_shared<tcp_client::async_send_item>(std::move(to_write), identy,
                                                                                                                    [&f_set_status, &count_awainting_responses, &cond, i, &indexex, &lock_indexex, &setted_resp](std::int32_t identy, const tcp_client::status& sts)
            {
                if (sts != tcp_client::status::ok)
                {
                    f_set_status(i, read_res::error_send_request_from_server);
                    --count_awainting_responses;
                    cond.notify_all();
                }
            });

            v_async_send_item.push_back(async_send);
            ++count_awainting_responses;
            c->send_async(async_send);
        }
        else
        {
            f_set_status(i, read_res::error_to_connect_to_server);
        }
    }

    //wait
    std::unique_lock<std::mutex> lock { lock_cond };
    if(cond.wait_for(lock, std::chrono::milliseconds(timeout_mili_sec), [&count_awainting_responses]{ return count_awainting_responses == 0; }))
    {
         //std::cout << "ok " << count_awainting_responses << std::endl;
    }
    else
    {
         //std::cout << "timout " << count_awainting_responses << std::endl;
    }

    for (int i = 0; i < v_async_send_item.size(); ++i)
    {
        v_async_send_item[i]->unset_awaiting_send();
        v_async_send_item[i]->unset_callback();
    }

    for (const auto& it : callback_ids)
    {
        std::shared_ptr<tcp_client> c = clients[it.first];
        c->unset_read_callback(it.second);
    }

    //set to others timeout status
    for (int i = 0; i < clients.size(); ++i)
    {
        f_set_status(i, read_res::timeout);
    }

    return indexex;
}

std::vector<std::int32_t> client::get_free_size_servers(std::int32_t timeout_mili_sec)
{
    std::mutex lock_indexex;
    std::vector<std::int32_t> indexex;
    std::atomic_int count_awainting_responses = ATOMIC_VAR_INIT(0);

    std::mutex lock_cond;
    std::condition_variable cond;

    std::vector<std::shared_ptr<tcp_client::async_send_item> > v_async_send_item;

    std::map<std::int32_t, std::uint64_t> callback_ids;
    for (int i = 0; i < clients.size(); ++i)
    {
        std::shared_ptr<tcp_client> c = clients[i];
        if (c->get_socket_state() == tcp_client::socket_status::connected && !c->timeout.is_timeout_detected())
        {
            std::int32_t identy = c->get_identy();
            std::string to_write { "03" + std::to_string(identy) };

            std::uint64_t callback_id = c->set_read_callback([identy, i, &indexex, &lock_indexex, &count_awainting_responses, &cond](const std::string& message_)
            {
                if (message_.size() >= 9)
                {
                    std::string status = message_.substr(2, 2);
                    std::string identy_ = message_.substr(4, 5);

                    if (status.compare("01") == 0)
                    {
                        if (identy_.compare(std::to_string(identy)) == 0)
                        {
                            std::lock_guard<std::mutex> _{ lock_indexex };
                            indexex.push_back(i);
                        }
                    }
                }

                --count_awainting_responses;
                cond.notify_all();
            });

            callback_ids.insert( { i, callback_id } );

            std::shared_ptr<tcp_client::async_send_item> async_send = std::make_shared<tcp_client::async_send_item>(std::move(to_write), identy,
                                                                                                                    [&count_awainting_responses, &cond](std::int32_t identy, const tcp_client::status& sts)
            {
                if (sts != tcp_client::status::ok)
                {
                    --count_awainting_responses;
                    cond.notify_all();
                }
            });

            v_async_send_item.push_back(async_send);
            ++count_awainting_responses;
            c->send_async(async_send);
        }
    }

    //wait
    std::unique_lock<std::mutex> lock { lock_cond };
    if(cond.wait_for(lock, std::chrono::milliseconds(timeout_mili_sec), [&count_awainting_responses]{ return count_awainting_responses == 0; }))
    {
         //std::cout << "ok " << count_awainting_responses << std::endl;
    }
    else
    {
         //std::cout << "timout " << count_awainting_responses << std::endl;
    }

    for (int i = 0; i < v_async_send_item.size(); ++i)
    {
        v_async_send_item[i]->unset_awaiting_send();
        v_async_send_item[i]->unset_callback();
    }

    for (const auto& it : callback_ids)
    {
        std::shared_ptr<tcp_client> c = clients[it.first];
        c->unset_read_callback(it.second);
    }

    return indexex;
}

void client::set_onchange_status_callback(tcp_client::status_callback s_)
{
    for (int i = 0; i < clients.size(); ++i)
    {
        clients[i]->set_status_callback(s_);
    }
}

std::int32_t client::pop_index_rand(std::map<std::int32_t, std::vector<std::int32_t> > &servers, std::unordered_set<std::int32_t>& used_groups, std::int32_t preffered_network)
{
    if (servers.empty()) return -1;
    auto f_pref = servers.find(preffered_network);
    if (f_pref != servers.end())
    {
        std::vector<std::int32_t>& s_vec = f_pref->second;
        std::int32_t sz = s_vec.size();
        if (sz == 0) return -1;
        std::int32_t index_to_pop = basefunc_std::rand(0, sz - 1);
        std::int32_t ret = s_vec[index_to_pop];
        s_vec.erase(s_vec.begin() + index_to_pop);
        if (s_vec.empty()) servers.erase(preffered_network);
        return ret;
    }
    else
    {
        std::vector<std::int32_t> existing_network_groups;
        for (const auto& it : servers)
        {
            if (used_groups.count(it.first) == 0) existing_network_groups.push_back(it.first);
        }

        if (existing_network_groups.empty())
        {
            for (const auto& it : servers)
            {
                existing_network_groups.push_back(it.first);
            }
        }

        std::int32_t index_to_select = basefunc_std::rand(0, existing_network_groups.size() - 1);
        std::int32_t select_netw = existing_network_groups[index_to_select];
        used_groups.emplace(select_netw);

        std::vector<std::int32_t>& s_vec = servers[select_netw];
        std::int32_t sz = s_vec.size();
        if (sz == 0) return -1;
        std::int32_t index_to_pop = basefunc_std::rand(0, sz - 1);
        std::int32_t ret = s_vec[index_to_pop];
        s_vec.erase(s_vec.begin() + index_to_pop);
        if (s_vec.empty()) servers.erase(select_netw);
        return ret;
    }
}

std::int32_t client::get_count_servers_connected(const write_options& w_opt)
{
    std::int32_t ret { 0 };
    for (int i = 0; i < clients.size(); ++i)
    {
        std::shared_ptr<tcp_client> c = clients[i];
        if (c->get_socket_state() == tcp_client::socket_status::connected && !c->timeout.is_timeout_detected())
        {
            if ((w_opt.rw_pref == rw_preference::any && c->can_use_in_any_rp()) ||
                std::find(w_opt.preffered_netwok_groups.begin(), w_opt.preffered_netwok_groups.end(), c->get_network_group()) != w_opt.preffered_netwok_groups.end())
            {
                ++ret;
            }
        }
    }
    return ret;
}

void client::dispatch_events(int minimum_m_sec_to_dispatch)
{
    std::chrono::time_point<std::chrono::steady_clock> t_now = std::chrono::steady_clock::now();
    std::vector<std::pair<std::string, evetne_item>> i_ev = event_items.get_old_events_and_clear([&t_now, &minimum_m_sec_to_dispatch](const time_cache<std::string, evetne_item>::time_cache_item& it)
    {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(t_now - it.last_set_cache) > std::chrono::milliseconds(minimum_m_sec_to_dispatch)) return true;
        return false;
    });

    for (const auto& it : i_ev)
    {
        cb_events(it.second, it.first);
    }
}

void client::listen_events(const std::unordered_set<std::int32_t>& types, read_callback c_, std::int32_t read_past_data_sec)
{
    cb_events = c_;

    for (int i = 0; i < clients.size(); ++i)
    {
        std::shared_ptr<tcp_client> c = clients[i];
        c->listen_events(types, read_past_data_sec);

        std::uint32_t ip_c = network_std::inet_aton(c->get_ip());

        c->set_read_callback([this, ip_c, c_, c](const std::string& str)
        {
            bool is_first { false };
            std::int32_t type { 0 };
            std::int32_t ev_type { 0 };

            std::string::size_type f = str.find("|");
            std::string::size_type f2 { std::string::npos };
            if (f != std::string::npos)
            {
                f2 = str.find("|", f + 1);
                if (f2 != std::string::npos)
                {
                    basefunc_std::stoi(str.substr(0, f), type);
                    if (type == 12)
                    {
                        basefunc_std::stoi(str.substr(f + 1, f2 - f), ev_type);
                    }
                }
            }

            /*if (str.size() >= 2)
            {
                if (str[0] == '1' && str[1] == '2')
                {
                    is_12 = true;

                    if (str.size() >= 5)
                    {
                        if (str[2] == '|' && str[3] == '-')
                        {
                            exp = basefunc_std::split(str, '|');
                            if (exp.size() == 3)
                            {
                                basefunc_std::stoi(exp[1], is_zero);
                                if (is_zero == -1)
                                {
                                    basefunc_std::cout(std::to_string(str.size()) + " " + std::to_string(std::string(exp[2]).size()), "is_zero");
                                }
                            }
                        }
                    }
                }
            }*/

            event_items.update_or_add(ev_type < 0 ? "000000000" + bitbase::numeric_to_chars(std::int32_t(ev_type)) + str.substr(f2 + 1) : str, [this, &ip_c, &is_first](evetne_item& upd)
            {
                if (upd.ips.empty())
                {
                    is_first = true;
                }

                upd.ips.emplace(ip_c);
            }, 600);

            if (is_first && type == 12 && ev_type > 0)
            {
                std::int32_t identy = c->get_identy();
                std::shared_ptr<tcp_client::async_send_item> async_send = std::make_shared<tcp_client::async_send_item>("11" + std::to_string(identy) + std::to_string(ev_type) + "|" + str.substr(f2 + 1));

                c->send_async(async_send);
            }
        });
    }
}

client::read_responce client::get_all_files(std::uint32_t ip, std::string& content, std::int32_t timeout, std::string contain)
{
    std::int32_t index_client { -1 };
    for (int i = 0; i < clients.size(); ++i)
    {
        std::shared_ptr<tcp_client> c = clients[i];
        if (c->get_socket_state() == tcp_client::socket_status::connected)
        {
            std::uint32_t ipi = network_std::inet_aton(c->get_ip());
            if (ipi == ip)
            {
                index_client = i;
                break;
            }
        }
    }

    if (index_client == -1)
    {
        return client::read_res::servers_not_found;
    }

    auto f_read = [this, &content, timeout, &contain](std::int32_t cl_index) -> bool
    {
        std::shared_ptr<tcp_client> c = clients[cl_index];

        std::int32_t identy = c->get_identy();
        std::string to_write { "06" + std::to_string(identy) + contain };

        std::atomic_bool is_ok = ATOMIC_VAR_INIT(false);
        std::atomic_int count_awainting_responses = ATOMIC_VAR_INIT(0);
        std::mutex lock_cond;
        std::condition_variable cond;

        std::uint64_t callback_id = c->set_read_callback([identy, &count_awainting_responses, &cond, &is_ok, &content](const std::string& message_)
        {
            if (message_.size() >= 9)
            {
                std::string status = message_.substr(2, 2);

                if (status.compare("01") == 0)
                {
                    std::string identy_ = message_.substr(4, 5);

                    if (identy_.compare(std::to_string(identy)) == 0)
                    {
                        content = message_.substr(9);
                        is_ok.store(true, std::memory_order_release);
                    }
                }
            }

            --count_awainting_responses;
            cond.notify_all();
        });

        std::shared_ptr<tcp_client::async_send_item> async_send = std::make_shared<tcp_client::async_send_item>(std::move(to_write), identy,
                                                                                                                [&count_awainting_responses, &cond](std::int32_t identy, const tcp_client::status& sts)
        {
            if (sts != tcp_client::status::ok)
            {
                --count_awainting_responses;
                cond.notify_all();
            }
        });

        ++count_awainting_responses;
        c->send_async(async_send);

        //wait
        std::unique_lock<std::mutex> lock { lock_cond };
        cond.wait_for(lock, std::chrono::milliseconds(timeout), [&count_awainting_responses]{ return count_awainting_responses == 0; });

        //remove callbacks
        async_send->unset_awaiting_send();
        async_send->unset_callback();

        c->unset_read_callback(callback_id);

        return is_ok.load(std::memory_order_acquire);
    };

    bool ok_t = f_read(index_client);

    if (ok_t)
    {
        if (!content.empty()) content = commpression_zlib::decompress_string(content);
        return { read_res::ok, network_std::inet_aton(clients[index_client]->get_ip()) };
    }
    else
    {
        return read_res::fail;
    }
}

client::read_responce client::read_file(const std::string& filename, std::string& content, int32_t find_timeout_milisec, std::int32_t part_timeout_milisec, int32_t count_parts, std::vector<std::int32_t> preffered_netwok_groups, rw_preference rw_pref, std::unordered_set<uint32_t> only_ips, bool read_many, bool read_sdb)
{
    std::vector<read_info> servers_pool_indexex;
    if (!only_ips.empty())
    {
        for (int i = 0; i < clients.size(); ++i)
        {
            std::shared_ptr<tcp_client> c = clients[i];
            if (c->get_socket_state() == tcp_client::socket_status::connected && !c->timeout.is_timeout_detected())
            {
                std::uint32_t ipi = network_std::inet_aton(c->get_ip());
                if (only_ips.count(ipi) != 0)
                {
                    read_info ri;
                    ri.index = i;
                    ri.ip = ipi;
                    servers_pool_indexex.push_back(ri);
                }
            }
        }

        if (servers_pool_indexex.empty())
        {
            return client::read_res::servers_not_found;
        }
    }
    else
    {
        client::read_res f_sts = command(read_info_type::file_exists, servers_pool_indexex, filename, count_parts, find_timeout_milisec, preffered_netwok_groups, rw_pref);
        if (f_sts != client::read_res::ok) return f_sts;
    }

    if (servers_pool_indexex.empty())
    {
        return client::read_res::file_not_found;
    }

    read_info ri;

    auto f_read = [this, &filename, &content, &ri, part_timeout_milisec, read_many, read_sdb](std::int32_t cl_index) -> bool
    {
        std::shared_ptr<tcp_client> c = clients[cl_index];

        std::int32_t identy = c->get_identy();
        std::string to_write = read_many ? "09" : "02";
        if (read_sdb) to_write = "30";
        (to_write += std::to_string(identy)) += filename;

        std::atomic_bool is_ok = ATOMIC_VAR_INIT(false);
        std::atomic_int count_awainting_responses = ATOMIC_VAR_INIT(0);
        std::mutex lock_cond;
        std::condition_variable cond;

        ri.index = cl_index;
        ri.ip = network_std::inet_aton(c->get_ip());

        std::uint64_t callback_id = c->set_read_callback([this, identy, &count_awainting_responses, &cond, &is_ok, &content, &ri, read_many](const std::string& message_)
        {
            if (message_.size() > 32)
            {
                std::string status = message_.substr(2, 2);

                if (status.compare("01") == 0)
                {
                    std::string identy_ = message_.substr(4, 5);

                    if (identy_.compare(std::to_string(identy)) == 0)
                    {
                        if (!read_many)
                        {
                            rssdisk::fetch_content(message_, ri);
                            if (!ri.content.empty())
                            {
                                std::swap(content, ri.content);
                                ri.content.clear();
                                is_ok.store(true, std::memory_order_release);
                            }

                            /*std::int32_t file_type { 0 };
                            basefunc_std::stoi(message_.substr(9, 2), file_type);
                            ri.file_type = static_cast<w_type>(file_type);

                            std::int32_t l { 30 };

                            if (ri.file_type == w_type::updatable || ri.file_type == w_type::updatable_without_compress || ri.file_type == w_type::jdb || ri.file_type == w_type::ajdb)
                            {
                                l = 46;

                                bitbase::chars_to_numeric(message_.substr(11, 8), ri.file_tms);
                                bitbase::chars_to_numeric(message_.substr(19, 8), ri.file_crc32);
                                ri.file_created.parse_date_time(message_.substr(27, 19));
                            }
                            else
                            {
                                ri.file_created.parse_date_time(message_.substr(11, 19));
                            }

                            std::string _ttl;
                            for (std::int32_t i = l, n = 0; i < message_.size(), n < 10; ++i, ++n) //ttl
                            {
                                ++l;
                                if (message_[i] == ' ') break;
                                else _ttl += message_[i];
                            }

                            basefunc_std::stoi(_ttl, ri.ttl_days);

                            if (message_.size() > l)
                            {
                                std::string content_incoming = message_.substr(l);
                                if (!content_incoming.empty())
                                {
                                    content = std::move(content_incoming);
                                    is_ok.store(true, std::memory_order_release);
                                }
                            }*/
                        }
                        else
                        {
                            content = message_.substr(9);
                            is_ok.store(true, std::memory_order_release);
                        }
                    }
                    else
                    {
                        return;
                    }
                }
            }
            else if (message_.size() >= 4)
            {
                int sts { 0 };
                basefunc_std::stoi(message_.substr(2, 2), sts);
                basefunc_std::log("Status " + rssdisk::server_response::get(sts) + " ip " + network_std::inet_ntoa(ri.ip), "rssdisk_pool_client_read_file", true, basefunc_std::COLOR::RED_COL);
            }

            --count_awainting_responses;
            cond.notify_all();
        });

        std::shared_ptr<tcp_client::async_send_item> async_send = std::make_shared<tcp_client::async_send_item>(std::move(to_write), identy,
                                                                                                                [this, cl_index, &count_awainting_responses, &cond](std::int32_t identy, const tcp_client::status& sts)
        {
            if (sts != tcp_client::status::ok)
            {
                if (sts == tcp_client::status::waiting_for_previous_responce)
                {
                    basefunc_std::cout("Waiting for previous answer", "rssdisk_pool_client_read_file", basefunc_std::COLOR::RED_COL);
                    std::shared_ptr<tcp_client> c = clients[cl_index];
                    NotificationHandler::notify("rssdisk_pool_client_read_file " + c->get_ip(), "rssdisk", "rssdisk", 1);
                }

                --count_awainting_responses;
                cond.notify_all();
            }
        });

        ++count_awainting_responses;
        c->send_async(async_send);

        //wait
        std::unique_lock<std::mutex> lock { lock_cond };
        cond.wait_for(lock, std::chrono::milliseconds(part_timeout_milisec), [&count_awainting_responses]{ return count_awainting_responses == 0; });

        //remove callbacks
        async_send->unset_awaiting_send();
        async_send->unset_callback();

        c->unset_read_callback(callback_id);

        return is_ok.load(std::memory_order_acquire);
    };

    if (servers_pool_indexex.size() < count_parts) count_parts = servers_pool_indexex.size();

    std::int32_t ok_t_index { -1 };
    for (int i = 0; i < count_parts; ++i)
    {
        bool ok_t = f_read(servers_pool_indexex[i].index);
        if (ok_t)
        {
            ok_t_index = servers_pool_indexex[i].index;
            break;
        }
    }

    if (ok_t_index != -1 && !content.empty())
    {
        bool is_jdb_full { false };

        if (content.size() >= 3 && content.substr(0, 3).compare("jdb") == 0) is_jdb_full = true;

        if (ri.file_type != w_type::insert_only_without_compress && ri.file_type != w_type::updatable_without_compress && ri.file_type != w_type::appendable && !is_jdb_full && !read_many) content = commpression_zlib::decompress_string(content);
    }

    if (content.empty())
    {
        basefunc_std::log("Empty content returned " + filename, "rssdisk_pool_client_read_file", true, basefunc_std::COLOR::RED_COL);
        ok_t_index = -1;
    }

    if (ok_t_index != -1)
    {
        return { read_res::ok, network_std::inet_aton(clients[ok_t_index]->get_ip()), ri };
    }
    else
    {
        return read_res::fail;
    }
}

Json::Value client::parse_cdb_all(const std::vector<read_info>& info, cdb_out_data_format frm)
{
    Json::Value data;

    auto f = [&](const std::string& key, const std::string& value, std::int64_t tms, std::int64_t crc32, std::int64_t valid_until_tms) -> void
    {
        Json::Value& v = data[key];
        v["tms"] = Json::Int64(tms);
        v["crc32"] = Json::Int64(crc32);
        v["valid_to"] = Json::Int64(valid_until_tms);

        if (frm == cdb_out_data_format::json || frm == cdb_out_data_format::prefer_json)
        {
            try
            {
                Json::Value json;

                const char* begin = value.c_str();
                const char* end = begin + value.length();
                if (Json::Reader().parse(begin, end, json, false))
                {
                    v["data"] = json;
                }
                else if (frm == cdb_out_data_format::prefer_json)
                {
                    v["data"] = value;
                }
            }
            catch(...) { }
        }
        else
        {
            v["data"] = value;
        }

        if (!v.isMember("data")) v["data"] = "";
    };

    for (const rssdisk::read_info& it : info)
    {
        //8[local tms] [ 8[tms] 8[crc32] 8[file valid to tms] 4[key size] 4[value size] key compressed(value) ]
        if (it.content.size() > 40)
        {
            std::int64_t tms_local { 0 };
            bitbase::chars_to_numeric(it.content.substr(0, 8), tms_local);

            std::size_t start { 8 };

            while (true)
            {
                if (it.content.size() > start + 32)
                {
                    std::int64_t tms { 0 };
                    std::int64_t crc32 { 0 };
                    std::int64_t valid_until_tms { 0 };
                    std::int32_t key_size { 0 };
                    std::int32_t val_size { 0 };
                    bitbase::chars_to_numeric(it.content.substr(start, 8), tms);
                    start += 8;
                    bitbase::chars_to_numeric(it.content.substr(start, 8), crc32);
                    start += 8;
                    bitbase::chars_to_numeric(it.content.substr(start, 8), valid_until_tms);
                    start += 8;
                    bitbase::chars_to_numeric(it.content.substr(start, 4), key_size);
                    start += 4;
                    bitbase::chars_to_numeric(it.content.substr(start, 4), val_size);
                    start += 4;

                    std::string key = it.content.substr(start, key_size);
                    start += key_size;

                    std::string value = commpression_zlib::decompress_string(it.content.substr(start, val_size));
                    start += val_size;

                    if (data.isMember(key))
                    {
                        if (tms > data[key]["tms"].asInt64())
                        {
                            data[key] = Json::Value();
                            f(key, value, tms, crc32, valid_until_tms);
                        }
                    }
                    else
                    {
                        f(key, value, tms, crc32, valid_until_tms);
                    }

                    data[key]["readed"].append(it.ip);
                }
                else
                {
                    break;
                }
            }
        }
    }

    return data;
}

client::read_res client::command_seq(read_info_type r_type, std::vector<read_info>& indexex, const std::string& filename, std::int32_t timeout_one_server, std::int32_t timeout_general, std::vector<std::int32_t> preffered_netwok_groups, rw_preference rw_pref, std::set<std::uint32_t> only_ips, bool to_all)
{
    if (timeout_one_server < 300)
    {
        NotificationHandler::notify("Low command_seq timeout_one_server " + std::to_string(timeout_one_server), "command_seq", "rssdisk", 1);
        timeout_one_server = 300;
    }

    timer tm;

    std::map<std::int32_t, std::vector<std::int32_t> > servers_pool_indexex;


    for (int i = 0; i < clients.size(); ++i)
    {
        std::shared_ptr<tcp_client> c = clients[i];

        if (c->get_socket_state() == tcp_client::socket_status::connected && !c->timeout.is_timeout_detected())
        {
            if ((rw_pref == rw_preference::any && c->can_use_in_any_rp()) ||
                std::find(preffered_netwok_groups.begin(), preffered_netwok_groups.end(), c->get_network_group()) != preffered_netwok_groups.end())
            {
                if (only_ips.empty() || only_ips.count(c->get_uip()) != 0)
                {
                    servers_pool_indexex[c->get_network_group()].push_back(i);
                }
            }
        }
    }

    std::unordered_set<std::int32_t> used_groups;
    while (tm.elapsed_mili() < timeout_general)
    {
        std::int32_t ind = pop_index_rand(servers_pool_indexex, used_groups, -1);

        if (ind == -1)
        {
            return (to_all && !indexex.empty()) ? client::read_res::ok : client::read_res::file_not_found;
        }

        std::shared_ptr<tcp_client> c = clients[ind];
        tcp_client::socket_status state = c->get_socket_state();
        if (state == tcp_client::socket_status::connected)
        {
            command(r_type, indexex, filename, 1, timeout_one_server, preffered_netwok_groups, rw_pref, { network_std::inet_aton(c->get_ip()) });
            if (!to_all && !indexex.empty()) return client::read_res::ok;
        }
    }

    return client::read_res::timeout;
}

client::read_res client::command(read_info_type r_type, std::vector<read_info>& indexex, const std::string& filename, const std::int32_t count_parts, std::int32_t timeout_mili_sec, std::vector<std::int32_t> preffered_netwok_groups, rw_preference rw_pref, std::unordered_set<std::uint32_t> only_ips)
{
    if (r_type == read_info_type::file_info && rw_pref == rw_preference::any)
    {
        return read_res::error_specify_the_group;
    }

    std::mutex lock_indexex;
    std::atomic_int count_awainting_responses = ATOMIC_VAR_INIT(0);

    std::mutex lock_cond;
    std::condition_variable cond;

    std::mutex lock_timeouts;
    std::set<std::int32_t> timeout_indexes;

    std::vector<std::shared_ptr<tcp_client::async_send_item> > v_async_send_item;

    //bool debug { false };
    //if (filename == "{\"keys\":\"online_tcu_\"}" || filename == "{\"keys\":\"online_tcu_away_\"}") debug = true;

    std::map<std::int32_t, std::uint64_t> callback_ids;
    for (int i = 0; i < clients.size(); ++i)
    {
        std::shared_ptr<tcp_client> c = clients[i];
        if (c->get_socket_state() == tcp_client::socket_status::connected && !c->timeout.is_timeout_detected())
        {
            bool need_send { false };
            if (!only_ips.empty())
            {
                if (only_ips.count(network_std::inet_aton(c->get_ip())) != 0)
                {
                    need_send = true;
                }
            }
            else if ((rw_pref == rw_preference::any && c->can_use_in_any_rp()) ||
                     std::find(preffered_netwok_groups.begin(), preffered_netwok_groups.end(), c->get_network_group()) != preffered_netwok_groups.end())
            {
                need_send = true;
            }

            if (need_send)
            {
                std::int32_t identy = c->get_identy();
                std::string to_write;

                switch (r_type)
                {
                    case read_info_type::read_files: to_write = "02"; break;
                    case read_info_type::read_files_many: to_write = "09"; break;
                    case read_info_type::read_files_many_comressed: to_write = "29"; break;
                    case read_info_type::read_file_tms_newest: to_write = "02"; break;
                    case read_info_type::get_files: to_write = "06"; break;
                    case read_info_type::file_info: to_write = "07"; break;
                    case read_info_type::remove_file: to_write = "08"; break;
                    case read_info_type::read_cdb: to_write = "20"; break;
                    case read_info_type::read_cdb_tms_newest: to_write = "20"; break;
                    case read_info_type::read_cdb_all: to_write = "21"; break;
                    case read_info_type::read_edb_events: to_write = "31"; break;
                    default: to_write = "01"; break;
                }

                (to_write += std::to_string(identy)) += filename;

                std::uint64_t callback_id = c->set_read_callback([this, c, identy, filename, i, &indexex, &lock_indexex, &count_awainting_responses, &cond, &lock_timeouts, &timeout_indexes, &r_type](const std::string& message_)
                {
                    {
                        std::lock_guard<std::mutex> _{ lock_timeouts };
                        timeout_indexes.erase(i);
                    }

                    /*if (debug)
                    {
                        basefunc_std::log(filename + " recieved " + c->get_ip(), "debug_count_awainting_responses_4", false);
                    }*/

                    if (message_.size() >= 9)
                    {
                        std::string status = message_.substr(2, 2);
                        std::string identy_ = message_.substr(4, 5);

                        if (identy_.compare(std::to_string(identy)) == 0)
                        {
                            if (r_type == read_info_type::file_info)
                            {
                                if (status.compare("01") == 0)
                                {
                                    std::string f_info_string = message_.substr(9);

                                    read_info ri;
                                    ri.index = i;
                                    ri.ip = network_std::inet_aton(c->get_ip());

                                    if (f_info_string.size() >= 2)
                                    {
                                        std::int32_t file_type { 0 };
                                        basefunc_std::stoi(f_info_string.substr(0, 2), file_type);
                                        ri.file_type = static_cast<w_type>(file_type);
                                    }

                                    if (ri.file_type == w_type::none)
                                    {

                                    }
                                    else if (ri.file_type == w_type::jdb_data)
                                    {
                                        if (f_info_string.size() >= 6)
                                        {
                                            bitbase::chars_to_numeric(f_info_string.substr(2, 4), ri.file_crc32);
                                        }
                                    }
                                    else
                                    {
                                        if (f_info_string.size() >= 18)
                                        {
                                            if (ri.file_type == w_type::updatable || ri.file_type == w_type::updatable_without_compress || ri.file_type == w_type::jdb || ri.file_type == w_type::ajdb)
                                            {
                                                bitbase::chars_to_numeric(f_info_string.substr(2, 8), ri.file_tms);
                                                bitbase::chars_to_numeric(f_info_string.substr(10, 8), ri.file_crc32);

                                                if (f_info_string.size() >= 38)
                                                {
                                                    ri.file_created.parse_date_time(f_info_string.substr(18, 19));
                                                    basefunc_std::stoi(f_info_string.substr(37), ri.ttl_days);
                                                }
                                            }
                                            else if (f_info_string.size() >= 22)
                                            {
                                                ri.file_created.parse_date_time(f_info_string.substr(2, 19));
                                                basefunc_std::stoi(f_info_string.substr(21), ri.ttl_days);
                                            }
                                        }
                                    }

                                    std::lock_guard<std::mutex> _{ lock_indexex };
                                    indexex.push_back(ri);
                                }
                            }
                            else if (r_type == read_info_type::get_files || r_type == read_info_type::read_cdb_all)
                            {
                                if (status.compare("01") == 0)
                                {
                                    read_info ri;
                                    ri.index = i;
                                    ri.ip = network_std::inet_aton(c->get_ip());
                                    ri.content = message_.substr(9);
                                    if (message_.size() > 9 && r_type == read_info_type::get_files) ri.content = commpression_zlib::decompress_string(message_.substr(9));

                                    std::lock_guard<std::mutex> _{ lock_indexex };
                                    indexex.push_back(ri);
                                }
                            }
                            else if (r_type == read_info_type::read_files || r_type == read_info_type::read_file_tms_newest || r_type == read_info_type::read_cdb || r_type == read_info_type::read_cdb_tms_newest)
                            {
                                if (status.compare("01") == 0 && message_.size() > 32)
                                {
                                    read_info ri;
                                    ri.index = i;
                                    ri.ip = network_std::inet_aton(c->get_ip());

                                    rssdisk::fetch_content(message_, ri);

                                    if (!ri.content.empty())
                                    {
                                        bool is_jdb_full { false };

                                        if (ri.content.size() >= 3 && ri.content.substr(0, 3).compare("jdb") == 0) is_jdb_full = true;

                                        if (ri.file_type != w_type::insert_only_without_compress && ri.file_type != w_type::updatable_without_compress && ri.file_type != w_type::appendable && !is_jdb_full) ri.content = commpression_zlib::decompress_string(ri.content);
                                    }

                                    std::lock_guard<std::mutex> _{ lock_indexex };
                                    indexex.push_back(ri);
                                }
                            }
                            else if (r_type == read_info_type::read_files_many || r_type == read_info_type::read_files_many_comressed || r_type == read_info_type::read_edb_events)
                            {
                                if (status.compare("01") == 0 && message_.size() > 32)
                                {
                                    read_info ri;
                                    ri.index = i;
                                    ri.ip = network_std::inet_aton(c->get_ip());
                                    ri.content = message_.substr(9);

                                    std::lock_guard<std::mutex> _{ lock_indexex };
                                    indexex.push_back(ri);
                                }
                            }
                            else
                            {
                                if (status.compare("01") == 0)
                                {
                                    read_info ri;
                                    ri.index = i;
                                    ri.ip = network_std::inet_aton(c->get_ip());

                                    std::lock_guard<std::mutex> _{ lock_indexex };
                                    indexex.push_back(ri);
                                }
                            }

                            --count_awainting_responses;
                            cond.notify_all();
                        }
                    }


                });

                callback_ids.insert( { i, callback_id } );

                {
                    std::lock_guard<std::mutex> _{ lock_timeouts };
                    timeout_indexes.emplace(i);
                }

                std::shared_ptr<tcp_client::async_send_item> async_send = std::make_shared<tcp_client::async_send_item>(std::move(to_write), identy,
                                                                                                                        [this, i, &count_awainting_responses, &cond, &lock_timeouts, &timeout_indexes](std::int32_t identy, const tcp_client::status& sts)
                {
                    if (sts != tcp_client::status::ok)
                    {
                        if (sts == tcp_client::status::waiting_for_previous_responce)
                        {
                            basefunc_std::cout("Waiting for previous answer", "rssdisk_pool_client_command", basefunc_std::COLOR::RED_COL);
                            std::shared_ptr<tcp_client> c = clients[i];
                            NotificationHandler::notify("rssdisk_pool_client_command " + c->get_ip(), "rssdisk", "rssdisk", 1);
                        }

                        {
                            std::lock_guard<std::mutex> _{ lock_timeouts };
                            timeout_indexes.erase(i);
                        }

                        --count_awainting_responses;
                        cond.notify_one();
                    }
                });

                v_async_send_item.push_back(async_send);
                ++count_awainting_responses;
                c->send_async(async_send);
            }
        }
    }

    if (callback_ids.empty()) return read_res::servers_not_found;

    std::set<std::int32_t> _timeout_indexes;

    std::int32_t count_parts_awaiting = (r_type == read_info_type::read_file_tms_newest || r_type == read_info_type::read_cdb_tms_newest || r_type == read_info_type::read_edb_events) ? std::numeric_limits<std::int32_t>::max() : count_parts;
    if (only_ips.empty() && r_type == read_info_type::read_files_many) count_parts_awaiting = std::numeric_limits<std::int32_t>::max();

    /*if (debug)
    {
        basefunc_std::log(filename + " wait " + std::to_string(count_awainting_responses), "debug_count_awainting_responses_4", false);
    }*/

    //wait
    std::unique_lock<std::mutex> lock { lock_cond };

    const static constexpr std::int32_t loop_timeout_ms { 10 };

    bool is_cond;
    do
    {
        is_cond = cond.wait_for(lock, std::chrono::milliseconds(loop_timeout_ms), [&count_awainting_responses, &indexex, &lock_indexex, &count_parts_awaiting]
        {
            std::int32_t sz_ok { 0 };
            {
                std::lock_guard<std::mutex> _{ lock_indexex };
                sz_ok = indexex.size();
            }

            return count_awainting_responses == 0 || sz_ok >= count_parts_awaiting;
        });

        timeout_mili_sec -= loop_timeout_ms;
    }
    while (!is_cond && timeout_mili_sec > 0);

    if (is_cond)
    {
        if (count_awainting_responses < 0) basefunc_std::log(filename + " " + std::to_string(count_awainting_responses), "debug_count_awainting_responses_m");
        //if (debug) basefunc_std::log(filename + " " + std::to_string(count_awainting_responses), "debug_count_awainting_responses_4", false);

        //std::cout << "ok " << count_awainting_responses << std::endl;
        {
            std::lock_guard<std::mutex> _{ lock_timeouts };
            std::swap(timeout_indexes, _timeout_indexes);
        }
    }
    else
    {
        if (count_awainting_responses < 0) basefunc_std::log(filename + " " + std::to_string(count_awainting_responses), "debug_count_awainting_responses_m");
        //if (debug) basefunc_std::log(filename + " " + std::to_string(count_awainting_responses), "debug_count_awainting_responses_4", false);

        //std::cout << "timeout " << count_awainting_responses << std::endl;
        {
            std::lock_guard<std::mutex> _{ lock_timeouts };
            std::swap(timeout_indexes, _timeout_indexes);
        }

        for (std::int32_t index : _timeout_indexes)
        {
            std::shared_ptr<tcp_client> c = clients[index];
            c->timeout.timeout_detected();
            std::cout << "timeout_detected " << c->get_ip() << std::endl;

            basefunc_std::log(filename + " " + c->get_ip(), "debug_timeout_rssdisk_command");
        }
    }

    for (int i = 0; i < v_async_send_item.size(); ++i)
    {
        v_async_send_item[i]->unset_awaiting_send();
        v_async_send_item[i]->unset_callback();
    }

    for (const auto& it : callback_ids)
    {
        std::shared_ptr<tcp_client> c = clients[it.first];
        c->unset_read_callback(it.second);

        if (_timeout_indexes.count(it.first) == 0) c->timeout.reset_timeout();
    }

    if (r_type == read_info_type::read_file_tms_newest || r_type == read_info_type::read_cdb_tms_newest)
    {
        std::int32_t count_j { 0 };
        std::int64_t tms_j { 0 };
        std::int32_t n { -1 };
        for (auto i = 0; i < indexex.size(); ++i)
        {
            const rssdisk::read_info& ri = indexex[i];
            if (ri.content.size() > 2)
            {
                if (ri.file_tms > tms_j)
                {
                    tms_j = ri.file_tms;
                    count_j = 1;
                    n = i;
                }
                else if (ri.file_tms == tms_j)
                {
                    ++count_j;
                }
            }
        }

        if (count_j < count_parts || n == -1)
        {
            return read_res::count_tms_files_less_than_requeired;
        }

        std::vector<read_info> rin;
        rin.push_back(std::move(indexex[n]));
        indexex = std::move(rin);
    }

    return read_res::ok;
}

void client::re_open_tcp()
{
    for (int i = 0; i < clients.size(); ++i)
    {
        std::shared_ptr<tcp_client> c = clients[i];
        c->restart();
    }
}

std::vector<client::read_responce_tcp> client::get_tcp_statuses()
{
    std::vector<client::read_responce_tcp> ret;

    for (int i = 0; i < clients.size(); ++i)
    {
        std::shared_ptr<tcp_client> c = clients[i];
        client::read_responce_tcp item;
        item.status = c->get_socket_state();
        item.ip_address_readed = network_std::inet_aton(c->get_ip());
        ret.push_back(item);
    }

    return ret;
}

std::string client::prepare_jdb(const Json::Value& data, const Json::Value& sett)
{
    return prepare_jdb(data.toString(), sett.toString());
}

std::string client::prepare_jdb(const std::string& data, const std::string& sett)
{
    return bitbase::numeric_to_chars(static_cast<std::uint32_t>(sett.size())) + sett + data;
}

