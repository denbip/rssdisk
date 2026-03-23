#include "disk_server.hpp"

using namespace rssdisk;

server::server(const std::string& config_path_) : config_path(config_path_)
{

}

bool server::init_server_settings()
{
    basefunc_std::cout("Reading settings...", "rssdisk");

    std::string self_ip { self_name };

    if (self_ip.empty()) basefunc_std::read_settings(config_path + "self.conf", "main", "ip", self_ip);

    if (!self_ip.empty())
    {
        std::string c;
        bool ok = disk::helper::read_file(config_path, "rssdisk.json", c, 0);

        std::unordered_set<std::string> path_control;

        if (ok)
        {
            try
            {
                Json::Value root;
                bool parsingSuccessful = Json::Reader().parse(c, root);
                if (!parsingSuccessful) throw std::runtime_error("Failed to parse configuration");

                if (!root.isMember(self_ip)) throw std::runtime_error("Failed to find " + self_ip);

                Json::Value& j = root[self_ip];
                config_loaded.enable_read = j["enable_read"].asBool();
                config_loaded.enable_write = j["enable_write"].asBool();
                config_loaded.weight = j["weight"].asInt();

                config_loaded.http_conf.port = j["http"]["port"].asInt();
                config_loaded.http_conf.threads = j["http"]["threads"].asInt();

                config_loaded.ttl_conf.p = j["ttl"]["p"].asString();
                config_loaded.tmp_conf.hdd_path = j["tmp"]["hdd_path"].asString();

                if (!config_loaded.tmp_conf.hdd_path.empty() && config_loaded.tmp_conf.hdd_path[config_loaded.tmp_conf.hdd_path.size() - 1] != '/')
                {
                    config_loaded.tmp_conf.hdd_path += "/";
                }
                if (!config_loaded.tmp_conf.hdd_path.empty())
                {
                    basefunc_std::create_folder_recursive(config_loaded.tmp_conf.hdd_path, 0);
                }

                auto& jtcp = j["tcp"];

                config_loaded.tcp_conf.port = jtcp["port"].asInt();
                config_loaded.tcp_conf.threads = jtcp["threads"].asInt();
                if (jtcp.isMember("worker_threads")) config_loaded.tcp_conf.worker_threads = jtcp["worker_threads"].asInt();
                else config_loaded.tcp_conf.worker_threads = config_loaded.tcp_conf.threads;
                if (jtcp.isMember("fast_worker_threads")) config_loaded.tcp_conf.fast_worker_threads = jtcp["fast_worker_threads"].asInt();
                else config_loaded.tcp_conf.fast_worker_threads = config_loaded.tcp_conf.threads;
                config_loaded.tcp_conf.delimeter = jtcp["delimeter"].asString();
                config_loaded.tcp_conf.echo = jtcp["echo"].asString();

                config_loaded.secure_conf.auth = j["secure"]["auth"].asString();
                config_loaded.secure_conf.aes_key = j["secure"]["aes_key"].asString();
                if (!config_loaded.secure_conf.aes_key.empty())
                {
                    config_loaded.secure_conf.aes_key = MD5(j["secure"]["aes_key"].asString()).hexdigest();
                    config_loaded.secure_conf.iv = config_loaded.secure_conf.aes_key.substr(0, 16);
                }
                config_loaded.secure_conf.gost_enc = j["secure"].get("gost_enc", false).asBool();

                if (!config_loaded.ttl_conf.p.empty()) basefunc_std::create_folder_recursive(config_loaded.ttl_conf.p, 0);

                config_loaded.dirs.clear();

                for (int i = 0; i < j["dirs"].size(); ++i)
                {
                    Json::Value& d = j["dirs"][i];

                    server_conf::directory dir;

                    dir.enabled = d["enabled"].asBool();
                    dir.p = d["p"].asString();

                    if (d["max_size_b"].isInt64())
                    {
                        dir.max_size_b = d["max_size_b"].asInt64();
                    }
                    else if (d["max_size_b"].isString())
                    {
                        std::string str_trash = d["max_size_b"].asString();
                        if (!str_trash.empty())
                        {
                            basefunc_std::stoi(str_trash.substr(0, str_trash.size() - 1), dir.max_size_b);

                            char l = str_trash[str_trash.size() - 1];
                            if (l == 'g' || l == 'G')
                            {
                                dir.max_size_b *= 1024l * 1024l * 1024l;
                            }
                            else if (l == 'm' || l == 'M')
                            {
                                dir.max_size_b *= 1024l * 1024l;
                            }
                            else if (l == 'k' || l == 'K')
                            {
                                dir.max_size_b *= 1024l;
                            }
                        }
                    }

                    dir.fs_block_size_b = disk::helper::get_fs_block_size(dir.p);

                    dir.curr_size_b = disk::helper::get_dir_size(dir.p, dir.fs_block_size_b) + dir.fs_block_size_b * count_segment_folders;

                    if (d.isMember("subfolder")) dir.sub_folder =  d["subfolder"].asString();

                    if (!dir.p.empty())
                    {
                        basefunc_std::create_folder_recursive(dir.p, 0);
                        for (std::int32_t i = 0; i < count_segment_folders; ++i)
                        {
                            basefunc_std::createFolder((dir.p + "/" + std::to_string(i)).c_str());
                        }
                    }

                    //control unique path
                    if (path_control.count(dir.p) != 0)
                    {
                        basefunc_std::cout(dir.p + " was already added", "rssdisk", basefunc_std::COLOR::RED_COL);
                        return false;
                    }
                    path_control.emplace(dir.p);

                    config_loaded.dirs.insert( std::move(std::pair<std::string, server_conf::directory>(dir.sub_folder, std::move(dir))) );
                }

                config_loaded.self_ip = self_ip;
            }
            catch(std::exception& ex)
            {
                basefunc_std::cout(std::string("cant_load_settings ") + std::string(ex.what()), "rssdisk", basefunc_std::COLOR::RED_COL);
            }
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }

    return true;
}

const server::server_conf& server::get_server_settings2() const noexcept
{
    return config_loaded;
}

bool server::remove_file_by_tms(std::string file_name, std::int64_t tms, std::int64_t crc32, bool force)
{
    //sub folders function
    auto f_subfolder = file_name.find("/");
    std::string subfolder;
    if (f_subfolder != std::string::npos)
    {
        subfolder = file_name.substr(0, f_subfolder);
        file_name = file_name.substr(f_subfolder + 1);
    }

    const std::string qry = std::to_string(get_segment_folder(file_name)) + "/" + file_name;

    bool is_locked { false };
    for (int i = 0; i < 50; ++i)
    {
        if (!lock_to_write.add(qry, true))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        else
        {
            is_locked = true;
            break;
        }
    }

    if (!is_locked)
    {
        basefunc_std::cout("File " + qry + " is locked", "server::remove_file_by_tms", basefunc_std::COLOR::RED_COL);
        return false;
    }

    locker_file lock_file_guard { &lock_to_write, qry };

    const rssdisk::server::server_conf& s = get_server_settings2();
    bool is_file_exists { false };

    auto f_dir = s.dirs.find(subfolder);
    if (f_dir != s.dirs.end())
    {
        rssdisk::server::server_conf::directory* dir = &(f_dir->second);
        is_file_exists = disk::helper::is_file_exists(dir->p + qry);

        if (is_file_exists)
        {
            bool need_remove { false };
            if (!force) //check tms
            {
                std::string content;
                bool ok = disk::helper::read_file(dir->p, qry, content, 64);

                if (ok)
                {
                    if (content.size() >= 18)
                    {
                        //check type file //type [01] tms [00000000] crc32 [00000000]
                        std::int16_t file_type { 0 };
                        std::int64_t file_tms { 0 };
                        std::int64_t file_crc32 { 0 };

                        basefunc_std::stoi(content.substr(0, 2), file_type);
                        bitbase::chars_to_numeric(content.substr(2, 8), file_tms);
                        bitbase::chars_to_numeric(content.substr(10, 8), file_crc32);

                        rssdisk::w_type _f_type = static_cast<rssdisk::w_type>(file_type);

                        //basefunc_std::cout(qry + " has " + std::to_string(file_tms) + " " + std::to_string(file_crc32), "server::remove_file_by_tms", basefunc_std::COLOR::RED_COL);

                        if (_f_type == rssdisk::w_type::updatable ||
                            _f_type == rssdisk::w_type::updatable_without_compress ||
                            _f_type == rssdisk::w_type::jdb)
                        {
                            if (file_tms < tms || (file_tms == tms && file_crc32 < crc32)) //remove file
                            {
                                //basefunc_std::cout(dir->p + qry, "remove_file_by_oplog");
                                need_remove = true;
                            }
                        }
                        else if (_f_type == rssdisk::w_type::insert_only ||
                                 _f_type == rssdisk::w_type::insert_only_without_compress) //replace insertable file with updatable
                        {
                            //basefunc_std::cout(dir->p + qry, "remove_file_by_oplog_insertable");
                            need_remove = true;
                        }
                        else if (_f_type == rssdisk::w_type::ajdb)
                        {

                        }
                        else
                        {
                            //basefunc_std::cout(dir->p + qry, "remove_file_by_oplog_huj_kakoj_to");
                            need_remove = true;
                        }
                    }
                }
                else
                {
                    return false;
                }
            }

            if (need_remove || force)
            {
                std::int32_t bytes = disk::helper::get_file_size_fs_block_size(dir->p + qry, dir->fs_block_size_b);
                dir->inc_curr_disk_size(-bytes);

                std::remove((dir->p + qry).c_str());

                auto check_additionals = [this, &dir, &qry](const std::string& ext)
                {
                    std::string item_jdb_data { dir->p + qry + ext };
                    if (disk::helper::is_file_exists(item_jdb_data))
                    {
                        std::int32_t bytes = disk::helper::get_file_size_fs_block_size(item_jdb_data, dir->fs_block_size_b);
                        dir->inc_curr_disk_size(-bytes);
                        std::remove(item_jdb_data.c_str());
                    }
                };

                check_additionals(storage::JDatabaseManager::get_j_extension_name());
            }
            else
            {
                //basefunc_std::cout("File " + qry + " wasn't", "server::remove_file_by_tms", basefunc_std::COLOR::RED_COL);
            }
        }
        else
        {
            //basefunc_std::cout("File " + qry + " not found", "server::remove_file_by_tms", basefunc_std::COLOR::RED_COL);
        }
    }

    return true;
}

Json::Value server::read_file_info(string file_name, string &content)
{
    auto f_jdb = file_name.find("?jdb");
    if (f_jdb != std::string::npos)
    {
        file_name = file_name.substr(0, f_jdb);
    }

    //sub folders function
    auto f_subfolder = file_name.find("/");
    std::string subfolder;
    if (f_subfolder != std::string::npos)
    {
        subfolder = file_name.substr(0, f_subfolder);
        file_name = file_name.substr(f_subfolder + 1);
    }

    std::string name_for_segment { file_name };
    basefunc_std::replaceAll(name_for_segment, storage::JDatabaseManager::get_j_extension_name(), "");

    Json::Value res;
    std::string qry = std::to_string(get_segment_folder(name_for_segment)) + "/" + file_name;

    if (!lock_to_write.add(qry, false))
    {
        server::response::set(res, rssdisk::server_response::status::file_busy);
    }
    else
    {
        locker_file lock_file_guard { &lock_to_write, qry };

        const rssdisk::server::server_conf& s = get_server_settings2();
        bool is_file_exists { false };
        std::string path;

        if (s.enable_read)
        {
            auto f_dir = s.dirs.find(subfolder);
            if (f_dir != s.dirs.end())
            {
                rssdisk::server::server_conf::directory& d = f_dir->second;

                is_file_exists = disk::helper::is_file_exists(d.p + qry);

                if (is_file_exists)
                {
                    path = d.p;

                    rssdisk::w_type version_w { rssdisk::w_type::none };

                    auto check_additionals = [&](const rssdisk::w_type tp, const std::string& registred_ext) -> bool
                    {
                        if (qry.find(registred_ext) != std::string::npos)
                        {
                            version_w = tp;

                            int _r = CRC32.get_hash_of_file(path + qry);
                            if (_r == 0)
                            {
                                server::response::set(res, rssdisk::server_response::status::cant_read_file);
                            }
                            else
                            {
                                content = std::to_string(static_cast<int>(version_w));
                                if (content.size() == 1) content = "0" + content;
                                content += bitbase::numeric_to_chars(_r);
                                server::response::set(res, rssdisk::server_response::status::ok);
                            }

                            return true;
                        }

                        return false;
                    };

                    if (check_additionals(rssdisk::w_type::jdb_data, storage::JDatabaseManager::get_j_extension_name())) { }
                    else
                    {
                        bool ok = disk::helper::read_file(path, qry, content, 64);

                        if (ok && !content.empty())
                        {
                            if (content.size() > 18)
                            {
                                //search ttl
                                std::int32_t version_int { static_cast<int>(rssdisk::w_type::none) };
                                std::string version = content.substr(0, 2);
                                basefunc_std::stoi(version, version_int);

                                rssdisk::w_type _w_type = static_cast<rssdisk::w_type>(version_int);

                                std::int32_t l { 2 }; //minimum header size
                                if (_w_type == rssdisk::w_type::updatable ||
                                    _w_type == rssdisk::w_type::updatable_without_compress ||
                                    _w_type == rssdisk::w_type::jdb ||
                                    _w_type == rssdisk::w_type::ajdb)
                                {
                                    l = 18; //minimum header size
                                }

                                std::int32_t l_last_ttl { l + 20 }; //19 - date + 1 for ttl as minimum

                                for (std::int32_t i = l_last_ttl, n = 0; i < content.size(), n < 10; ++i, ++n) //ttl
                                {
                                    if (content[i] == ' ') break;
                                    ++l_last_ttl;
                                }

                                if (l_last_ttl < content.size())
                                {
                                    content = content.substr(0, l_last_ttl);
                                }
                                else
                                {
                                    content = content.substr(0, 18);
                                }

                                server::response::set(res, rssdisk::server_response::status::ok);
                            }
                            else
                            {
                                server::response::set(res, rssdisk::server_response::status::cant_read_file);
                            }
                        }
                        else
                        {
                            ok = false;
                            server::response::set(res, rssdisk::server_response::status::cant_read_file);
                        }
                    }
                }
                else
                {
                    server::response::set(res, rssdisk::server_response::status::file_not_found);
                }
            }
            else
            {
                server::response::set(res, rssdisk::server_response::status::file_not_found);
            }
        }
        else server::response::set(res, rssdisk::server_response::status::read_is_disabled);
    }

    return res;
}

Json::Value server::read_file(std::string file_name, std::string& content, read_options ro)
{
    //sub folders function
    auto f_subfolder = file_name.find("/");
    std::string subfolder;
    if (f_subfolder != std::string::npos)
    {
        subfolder = file_name.substr(0, f_subfolder);
        file_name = file_name.substr(f_subfolder + 1);
    }

    std::string jdb_query;
    auto f_jdb = file_name.find("?jdb");
    bool is_jdb { false };
    if (f_jdb != std::string::npos)
    {
        jdb_query = file_name.substr(f_jdb + 4);
        file_name = file_name.substr(0, f_jdb);
        is_jdb = true;
    }

    int read_opt = static_cast<int>(ro);

    Json::Value res;
    std::string qry = std::to_string(get_segment_folder(file_name)) + "/" + file_name;

    auto _read = [&]()
    {
        const rssdisk::server::server_conf& s = get_server_settings2();
        bool is_file_exists { false };
        std::string path;

        if (s.enable_read)
        {
            auto f_dir = s.dirs.find(subfolder);
            if (f_dir != s.dirs.end())
            {
                rssdisk::server::server_conf::directory& d = f_dir->second;

                is_file_exists = disk::helper::is_file_exists(d.p + qry);

                if (is_file_exists)
                {
                    path = d.p;

                    bool ok { false };
                    if (is_jdb)
                    {
#ifndef debug_disable_jdb
                        content = jdb.fetch(path + qry, jdb_query, ro);
                        ok = true;
#endif
                    }               
                    else
                    {
                        ok = disk::helper::read_file(path, qry, content, 0);
                    }

                    if (ok && !content.empty()) server::response::set(res, rssdisk::server_response::status::ok);
                    else if (!content.empty()) server::response::set(res, rssdisk::server_response::status::errorstr);
                    else server::response::set(res, rssdisk::server_response::status::cant_read_file);
                }
                else
                {
                    server::response::set(res, rssdisk::server_response::status::file_not_found);
                }
            }
            else
            {
                server::response::set(res, rssdisk::server_response::status::file_not_found);
            }
        }
        else server::response::set(res, rssdisk::server_response::status::read_is_disabled);
    };

    if (!lock_to_write.add(qry, false))
    {
        server::response::set(res, rssdisk::server_response::status::file_busy);
    }
    else
    {
        locker_file lock_file_guard { &lock_to_write, qry };
        _read();
    }

    return res;
}

Json::Value server::accept_file(std::string&& content, bool need_check_auth, bool need_check_aes)
{
    Json::Value res;
    timer tm;

    if (content.size() > 2)
    {
        const server::server_conf& server_sett = get_server_settings2();

        if (server_sett.enable_write)
        {
            if (need_check_aes && !server_sett.secure_conf.aes_key.empty())
            {
                content = CRYPTOPP.decrypt(content, server_sett.secure_conf.aes_key, server_sett.secure_conf.iv, 32);
            }

            if (content.size() > 2)
            {
                std::string auth, file_name, ttl_days, flags;
                std::int32_t filename_filled { 0 };
                std::size_t start_content_from { 0 };
                bool is_auth_ok { true };
                std::int32_t max_auth_size { 256 };
                std::int32_t stable_read_count_bytes { 0 };
                for (auto i = 0; i < content.size(); ++i)
                {
                    char c = content[i];
                    if (c == ' ' && stable_read_count_bytes == 0)
                    {
                        if (need_check_auth && filename_filled == 0 && !server_sett.secure_conf.auth.empty() && server_sett.secure_conf.auth.compare(auth) != 0)
                        {
                            is_auth_ok = false;
                            break;
                        }

                        start_content_from = i;
                        if (filename_filled == 3) break;
                        ++filename_filled;
                    }
                    else
                    {
                        if (stable_read_count_bytes > 0) --stable_read_count_bytes;

                        if (filename_filled == 0)
                        {
                            if (max_auth_size == 0)
                            {
                                is_auth_ok = false;
                                break;
                            }
                            --max_auth_size;
                            auth += c;
                        }
                        else if (filename_filled == 1) file_name += c;
                        else if (filename_filled == 2) ttl_days += c;
                        else
                        {
                            flags += c;

                            if (flags.size() == 2)
                            {
                                int flags_int { 0 };
                                basefunc_std::stoi(flags, flags_int);
                                rssdisk::w_type _w_type = static_cast<rssdisk::w_type>(flags_int);

                                if (_w_type == rssdisk::w_type::updatable ||
                                    _w_type == rssdisk::w_type::updatable_without_compress ||
                                    _w_type == rssdisk::w_type::jdb ||
                                    _w_type == rssdisk::w_type::ajdb ||
                                    _w_type == rssdisk::w_type::jdb_formed) //updatable reciving
                                {
                                    stable_read_count_bytes = 16;
                                }
                                else if (_w_type == rssdisk::w_type::appendable)
                                {
                                    stable_read_count_bytes = 8;
                                }
                            }
                        }
                    }
                }

                if (file_name.size() > 64)
                {
                    server::response::set(res, rssdisk::server_response::status::too_long_filename);
                }
                else if (!is_auth_ok)
                {
                    server::response::set(res, rssdisk::server_response::status::auth_failed);
                }
                else
                {
                    std::string base_filename { file_name };

                    //sub folders function
                    auto f_subfolder = file_name.find("/");
                    std::string subfolder;
                    if (f_subfolder != std::string::npos)
                    {
                        subfolder = file_name.substr(0, f_subfolder);
                        file_name = file_name.substr(f_subfolder + 1);
                    }

                    if (file_name.empty())
                    {
                        server::response::set(res, rssdisk::server_response::status::file_name_must_not_be_empty);
                    }
                    else
                    {
                        file_name = std::to_string(get_segment_folder(file_name)) + "/" + file_name;

                        //write protection the same name files
                        if (!lock_to_write.add(file_name, true))
                        {
                            server::response::set(res, rssdisk::server_response::status::file_busy);
                        }
                        else
                        {
                            locker_file lock_file_guard { &lock_to_write, file_name };

                            auto f_dir = server_sett.dirs.find(subfolder);
                            if (f_dir != server_sett.dirs.end())
                            {
                                rssdisk::server::server_conf::directory& d = f_dir->second;

                                bool file_exists_in_directory = disk::helper::is_file_exists(d.p + file_name);
                                bool wr_enabled { file_exists_in_directory };

                                //search dir
                                if (!wr_enabled)
                                {
                                    if (d.enabled)
                                    {
                                        if (d.curr_size_b < d.max_size_b)
                                        {
                                            wr_enabled = true;
                                        }
                                    }
                                }

                                if (!wr_enabled)
                                {
                                    server::response::set(res, rssdisk::server_response::status::all_directories_are_full);
                                }
                                else
                                {
                                    std::int32_t ttl { 0 };
                                    basefunc_std::stoi(ttl_days, ttl);

                                    std::int32_t bytes_written { 0 };

                                    bool ok_jdb_write { true };
                                    bool is_oplog { false };
                                    bool is_append { false };
                                    bool write_header { true };
                                    std::vector<std::string> headers;
                                    rssdisk::w_type _w_type { rssdisk::w_type::none };

                                    std::string to_write;

                                    if (flags.size() >= 2)
                                    {
                                        std::string type = flags.substr(0, 2);
                                        std::int32_t type_int { 0 };
                                        basefunc_std::stoi(type, type_int);

                                        _w_type = static_cast<rssdisk::w_type>(type_int);

                                        switch (_w_type)
                                        {
                                            case rssdisk::w_type::updatable:
                                            case rssdisk::w_type::updatable_without_compress:
                                            case rssdisk::w_type::jdb:
                                            case rssdisk::w_type::ajdb:
                                            case rssdisk::w_type::jdb_formed:
                                            {
                                                if (flags.size() == 18)
                                                {
                                                    if (_w_type == rssdisk::w_type::jdb_formed)
                                                    {
                                                        headers.push_back("0" + std::to_string(static_cast<int>(rssdisk::w_type::jdb)));
                                                    }
                                                    else
                                                    {
                                                        std::string w_type_str { std::to_string(static_cast<int>(_w_type)) };
                                                        if (w_type_str.size() == 1) w_type_str = "0" + w_type_str;

                                                        headers.push_back(std::move(w_type_str));
                                                    }

                                                    headers.push_back(flags.substr(2, 8));
                                                    headers.push_back(flags.substr(10, 8));

                                                    if (_w_type != rssdisk::w_type::ajdb) is_oplog = true;
                                                }
                                                break;
                                            }
                                            case rssdisk::w_type::insert_only_without_compress:
                                            {
                                                headers.push_back("0" + std::to_string(static_cast<int>(_w_type)));
                                                break;
                                            }
                                            case rssdisk::w_type::appendable:
                                            {
                                                if (flags.size() == 10)
                                                {
                                                    headers.push_back(std::to_string(static_cast<int>(_w_type)));
                                                    headers.push_back(flags.substr(2, 8));
                                                }
                                                break;
                                            }
                                            default: break;
                                        }

                                        if (_w_type == rssdisk::w_type::jdb || _w_type == rssdisk::w_type::ajdb)
                                        {
                                            if (content.size() > start_content_from + 6)
                                            {
                                                std::string jdb_data, jdb_sett;
                                                std::uint32_t sz_of_sett { 0 };
                                                bitbase::chars_to_numeric(content.substr(start_content_from + 1, 4), sz_of_sett);

                                                if (content.size() > start_content_from + 5 + sz_of_sett)
                                                {
                                                    jdb_sett = content.substr(start_content_from + 5, sz_of_sett);
                                                    jdb_data = content.substr(start_content_from + 5 + sz_of_sett);

#ifndef debug_disable_jdb
                                                    storage::JDatabaseManager::DatabaseContent _jdb = jdb.createDatabaseContent(d.p + file_name, jdb_data, jdb_sett, _w_type);
                                                    to_write = _jdb.title;
                                                    is_append = _jdb.append;

                                                    if (!_jdb.errorOccurred)
                                                    {
                                                        if (_jdb.version == 1)
                                                        {
                                                            to_write += _jdb.body;
                                                        }
                                                        else if (_jdb.version == 2)
                                                        {
                                                            std::int32_t bytes_written_jdb { 0 };

                                                            if (is_append)
                                                            {
                                                                ok_jdb_write = disk::helper::append_to_file(d.p + file_name + storage::JDatabaseManager::get_j_extension_name(), _jdb.body);
                                                                bytes_written_jdb = _jdb.body.size();
                                                            }
                                                            else
                                                            {
                                                                ok_jdb_write = disk::helper::write_file_to_disk(d.p, file_name + storage::JDatabaseManager::get_j_extension_name(), _jdb.body, bytes_written_jdb, 0, d.fs_block_size_b, {}, false);
                                                            }

                                                            if (ok_jdb_write)
                                                            {
                                                                d.inc_curr_disk_size(bytes_written_jdb);

                                                                res["bytes_written_jdb"] = bytes_written_jdb;
                                                                res["file_name_jdb"] = base_filename + storage::JDatabaseManager::get_j_extension_name();
                                                            }
                                                        }
                                                    }
                                                    else
                                                    {
                                                        ok_jdb_write = false;
                                                    }
#else
                                                    ok_jdb_write = false;
#endif
                                                }
                                            }
                                        }
                                        else
                                        {
                                            to_write = content.substr(start_content_from + 1);
                                        }
                                    }
                                    else
                                    {
                                        to_write = content.substr(start_content_from + 1);
                                    }

                                    if (ok_jdb_write)
                                    {
                                        if (headers.empty()) headers.push_back("00");

                                        bool ok { false };

                                        if (_w_type == rssdisk::w_type::appendable && file_exists_in_directory)
                                        {
                                            ok = ok_jdb_write = disk::helper::append_to_file(d.p + file_name, to_write);
                                            std::int32_t count_new_blocks = to_write.size() != 0 ? to_write.size() / d.fs_block_size_b + 1 : 0;
                                            bytes_written += (count_new_blocks * d.fs_block_size_b);
                                        }
                                        else
                                        {
                                            if (write_header)
                                            {
                                                ok = disk::helper::write_file_to_disk(d.p, file_name, to_write, bytes_written, ttl, d.fs_block_size_b, headers);
                                            }
                                            else
                                            {
                                                ok = true;
                                            }
                                        }

                                        if (ok)
                                        {
                                            //check flags
                                            if (is_oplog)
                                            {
                                                std::int64_t tms_int { 0 };
                                                std::int64_t crc32_int { 0 };
                                                bitbase::chars_to_numeric(headers[1], tms_int);
                                                bitbase::chars_to_numeric(headers[2], crc32_int);

                                                if (on_write_ != nullptr) on_write_(base_filename, tms_int, crc32_int);
                                            }

                                            d.inc_curr_disk_size(bytes_written);

                                            server::response::set(res, rssdisk::server_response::status::ok);

                                            res["bytes_written"] = bytes_written;

                                            res["file_name"] = base_filename;
                                            std::string headers_l;
                                            for (const std::string& h : headers)
                                            {
                                                headers_l += h;
                                            }
                                            res["headers"] = headers_l;

                                            if (ttl > 0 && !is_append)
                                            {
                                                date_time dt_ttl = date_time::current_date_time().add_days(ttl);
                                                std::string path_ttl = server_sett.ttl_conf.p + dt_ttl.get_date() + "/ttl";
                                                basefunc_std::create_folder_recursive(path_ttl, 0, false);

                                                bool ttl_success = disk::helper::append_to_file(path_ttl, d.p + file_name + "\n");

                                                int i_t { 0 };
                                                while (!ttl_success && i_t < server_sett.tcp_conf.threads)
                                                {
                                                    ttl_success = disk::helper::append_to_file(path_ttl + std::to_string(i_t), d.p + file_name + "\n");
                                                    ++i_t;
                                                }

                                                res["ttl_success"] = ttl_success;
                                            }
                                        }
                                        else
                                        {
                                            server::response::set(res, rssdisk::server_response::status::cant_write_file_to_disk);
                                        }
                                    }
                                    else
                                    {
                                        server::response::set(res, rssdisk::server_response::status::cant_write_file_to_disk);
                                    }
                                }
                            }
                            else
                            {
                                server::response::set(res, rssdisk::server_response::status::subfolder_not_found);
                            }
                        }
                    }
                }
            }
            else
            {
                server::response::set(res, rssdisk::server_response::status::incorrect_secure_key);
            }
        }
        else
        {
            server::response::set(res, rssdisk::server_response::status::write_is_disabled);
        }
    }
    else
    {
        server::response::set(res, rssdisk::server_response::status::to_small_content_size);
    }

    res["el_micro"] = (int)tm.elapsed_micro();

    return res;
}

void server::run_ttl_remover()
{
    const date_time dt { date_time::current_date_time() };

    const server::server_conf& server_sett = get_server_settings2();

    auto remove_date = [this, &server_sett](const std::string& _dt)
    {
        basefunc_std::cout("Removing ttl date " + _dt, "run_ttl_remover");

        std::map<std::string, rssdisk::server::server_conf::directory*> path_index;
        for (auto& it : server_sett.dirs)
        {
            path_index.insert( { it.second.p, &(it.second) } );
        }

        auto f_clear = [&](const std::string& ttl_name)
        {
            std::string content;
            disk::helper::read_file(server_sett.ttl_conf.p + _dt + "/", ttl_name, content, 0);

            if (!content.empty())
            {
                std::stringstream ss(content);
                std::string item;

                try
                {
                    std::unordered_set<std::string> removed;
                    while (std::getline(ss, item, '\n'))
                    {
                        basefunc_std::trim(item);

                        if (removed.count(item) == 0 && !item.empty())
                        {
                            std::string lock_file_name;

                            std::vector<std::string> exp = basefunc_std::split(item, '/');
                            std::string path;
                            for (int i = 0; i < exp.size() - 2; ++i)
                            {
                                if (exp[i].empty()) continue;
                                path += "/" + exp[i];
                            }
                            path += "/";

                            if (exp.size() >= 2)
                            {
                                lock_file_name = exp[exp.size() - 2] + "/" + exp[exp.size() - 1];
                            }

                            //wait for lock
                            while (!lock_to_write.add(lock_file_name, true)) std::this_thread::yield();
                            locker_file lock_file_guard { &lock_to_write, lock_file_name };

                            auto f = path_index.find(path);
                            if (f != path_index.end())
                            {
                                std::int32_t bytes = disk::helper::get_file_size_fs_block_size(item, f->second->fs_block_size_b);
                                f->second->inc_curr_disk_size(-bytes);

                                auto check_additionals = [&f, &item, &bytes](const std::string& ext)
                                {
                                    std::string file_ext { item + ext };

                                    if (disk::helper::is_file_exists(file_ext))
                                    {
                                        bytes = disk::helper::get_file_size_fs_block_size(file_ext, f->second->fs_block_size_b);
                                        f->second->inc_curr_disk_size(-bytes);
                                    }
                                };

                                check_additionals(storage::JDatabaseManager::get_j_extension_name());
                            }

                            removed.emplace(item);
                            std::remove(item.c_str());
                            storage::JDatabaseManager::deleteFile(item);
                        }
                    }
                }
                catch(std::exception& ex)
                {
                    std::cout << ex.what() << std::endl;
                }
            }
        };

        f_clear("ttl");

        for (int i = 0; i < server_sett.tcp_conf.threads; ++i) f_clear("ttl" + std::to_string(i));

        try
        {
            boost::filesystem::remove_all(server_sett.ttl_conf.p + _dt);
        }
        catch(std::exception& ex)
        {
            std::cout << ex.what() << std::endl;
        }
    };

    std::vector<std::string> dirs = disk::helper::list_dirs(server_sett.ttl_conf.p);
    for (const auto& dir : dirs)
    {
        date_time dt_del;
        if (dt_del.parse_date_time(dir, "yyyy-MM-dd", date_time::date_time_sel::date_only))
        {
            if (dt_del.date_ <= dt.date_)
            {
                remove_date(dt_del.get_date());
            }
        }
    }
}

std::int32_t server::get_segment_folder(const std::string& filename)
{
    std::hash<std::string> h;
    return h(filename) % count_segment_folders;
}
