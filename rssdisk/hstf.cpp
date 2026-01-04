#include "hstf.h"

std::atomic_bool hstf_client::is_running { ATOMIC_VAR_INIT(true) };

bool hstf::init(const std::string& cfg, const std::string& ip)
{
    try
    {
        std::ifstream t(cfg + "/rssdisk.json");
        if (!t) return false;
        std::string c((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

        Json::Value root;
        bool parsingSuccessful = Json::Reader().parse(c, root);
        if (!parsingSuccessful) throw std::runtime_error("Failed to parse configuration");

        const Json::Value& m = root[ip];
        path = m["replication"]["p"].asString();
        size = m["replication"]["oplog_size_bytes"].asInt64();
        if (size > max_size) size = max_size;
    }
    catch(std::exception& ex)
    {
        basefunc_std::cout(std::string("cant_load_settings ") + std::string(ex.what()), "hstf::init", basefunc_std::COLOR::RED_COL);
        return false;
    }

    basefunc_std::create_folder_recursive(path, 0);

    std::string f { path + h_file };

    if (basefunc_std::is_file(f))
    {
        content = basefunc_std::read_file(f);
        if (content.size() % ssgmt != 0)
        {
            std::remove(f.c_str());
            content.clear();
        }
        else if (!content.empty())
        {
            std::string last { content.substr(content.size() - ssgmt, 8) };
            bitbase::chars_to_numeric(last, p);
        }
    }

    return true;
}

void hstf::add(const std::string& filename, std::int64_t tms, std::int64_t crc32)
{
    std::string f { path + h_file };

    WriteLock _{ lock };

    ++p;
    std::string a { bitbase::numeric_to_chars(p) + bitbase::numeric_to_chars(tms) + bitbase::numeric_to_chars(crc32) + filename };
    if (a.size() > ssgmt) a = a.substr(0, ssgmt);
    while (a.size() < ssgmt) a += '\0';

    content += a;
    basefunc_std::write_file_to_disk_a(f, a);
}

void hstf::clear()
{
    std::string f { path + h_file };

    WriteLock _{ lock };

    if (content.size() > size)
    {
        std::size_t d = (content.size() - size) / ssgmt;
        content = content.substr(d * ssgmt);
        basefunc_std::write_file_to_disk(f, content);
    }
}

std::string hstf::get(const std::string& qry) const
{
    std::uint64_t c { 0 };

    auto f = qry.find("p=");
    if (f != std::string::npos)
    {
        auto fe = qry.find("&", f + 2);
        if (fe == std::string::npos)
        {
            basefunc_std::stoi(qry.substr(f + 2), c);
        }
        else
        {
            basefunc_std::stoi(qry.substr(f + 2, fe - (f + 2)), c);
        }
    }

    std::string r;

    ReadLock _{ lock };

    r = "{\"p\":" + std::to_string(p);

    if (c < p)
    {
        std::int64_t sb { (p - c) * ssgmt };
        std::int64_t from { static_cast<std::int64_t>(content.size()) - sb };
        if (from < 0) from = 0;

        ((r += ",\"d\":\"") += API::base64::base64_encode(content.substr(from))) += "\"";
        (r += ",\"ssgmt\":") += std::to_string(ssgmt);
    }

    r += "}";

    return r;
}

bool hstf_client::init(const std::string& cfg, const std::string& ip, rssdisk::server* _rd)
{
    try
    {
        std::ifstream t(cfg + "/rssdisk.json");
        if (!t) return false;
        std::string c((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());

        Json::Value root;
        bool parsingSuccessful = Json::Reader().parse(c, root);
        if (!parsingSuccessful) throw std::runtime_error("Failed to parse configuration");

        Json::Value::Members m = root.getMemberNames();

        std::set<std::int32_t> repl_networks;
        for (int i = 0; i < m.size(); ++i)
        {
            std::string _ip = m[i];
            if (_ip.compare(ip) == 0)
            {
                path = root[_ip]["replication"]["p"].asString();

                const Json::Value& networks = root[_ip]["replication"]["networks"];
                for (int j = 0; j < networks.size(); ++j)
                {
                    repl_networks.emplace(networks[j].asInt());
                }
                break;
            }
        }

        basefunc_std::create_folder_recursive(path + fldr, 0);

        for (int i = 0; i < m.size(); ++i)
        {
            std::string _ip = m[i];
            if (_ip.compare(ip) == 0)
            {
                continue;
            }

            std::int32_t current_network = root[_ip]["groups"]["network"].asInt();
            if (repl_networks.count(current_network) == 0) continue;

            bool disabled_for_service = root[_ip].get("disabled_for_service", false).asBool();
            if (disabled_for_service) continue;

            std::int32_t port = root[_ip]["http"]["port"].asInt();

            clients.insert( { _ip, new item{ _ip, port, _rd, path + fldr + "/" + _ip } } );
        }

    }
    catch(std::exception& ex)
    {
        basefunc_std::cout(std::string("cant_load_settings ") + std::string(ex.what()), "hstf_client::init", basefunc_std::COLOR::RED_COL);
        return false;
    }

    return true;
}
