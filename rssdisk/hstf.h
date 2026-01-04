#ifndef HSTF_H
#define HSTF_H

#include "../../libs/basefunc_std.h"
#include "../../libs/json/json/json.h"
#include "../../libs/bitbase.h"
#include "../../libs/base64.h"
#include "../../libs/curl_http.h"
#include "../../libs/thread_worker.h"
#include "disk_server.hpp"

#include "../../libs/SimpleWeb/server_http.hpp"

#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>

using HttpServer = SimpleWeb::Server<SimpleWeb::HTTP>;

typedef boost::shared_mutex Lock;
typedef boost::unique_lock< Lock >  WriteLock;
typedef boost::shared_lock< Lock >  ReadLock;

class hstf
{
public:
    hstf() = default;

    bool init(const std::string& cfg, const std::string& ip);
    void add(const std::string& filename, std::int64_t tms, std::int64_t crc32);
    std::string get(const std::string& qry) const;
    void clear();

private:
    std::size_t size = 0;
    std::string path;

    constexpr const static std::size_t max_size { 10000000 };
    constexpr const static std::int32_t ssgmt { 88 };
    constexpr const static char* h_file { "h_file" };

    mutable Lock lock;
    std::string content;
    std::uint64_t p = 0;
};

class hstf_client
{
public:
    hstf_client() = default;

    bool init(const std::string& cfg, const std::string& ip, rssdisk::server* _rd);
    void stop()
    {
        hstf_client::is_running.store(false, std::memory_order_release);
    }

private:
    struct item
    {
        item() = default;
        item(const std::string& _ip, std::int32_t _p, rssdisk::server* _rd, const std::string& r_sett)
        {
            rd = _rd;

            t = new std::thread([this,_ip,_p,r_sett]()
            {
                basefunc_std::stoi(basefunc_std::read_file(r_sett), remote);

                while (hstf_client::is_running.load(std::memory_order_acquire))
                {
                    std::this_thread::sleep_for(std::chrono::seconds(hstf_client::strp));

                    std::string r;
                    auto http_code = curl_http::post("http://" + _ip + ":" + std::to_string(_p) + "/hstf", "p=" + std::to_string(remote), r, 5);
                    if (http_code == 0)
                    {
                        try
                        {
                            Json::Value root;
                            bool parsingSuccessful = Json::Reader().parse(r, root);
                            if (!parsingSuccessful) throw std::runtime_error("Failed to parse configuration");

                            if (root.isMember("d") && root.isMember("ssgmt"))
                            {
                                std::int32_t ssgmt = root["ssgmt"].asInt();
                                std::string d = API::base64::base64_decode(root["d"].asString());

                                std::int32_t el { 0 };
                                while (true)
                                {
                                    if (d.size() >= ssgmt * (el + 1))
                                    {
                                        process(d.substr(ssgmt * el, ssgmt));
                                        ++el;
                                    }
                                    else
                                    {
                                        break;
                                    }
                                }
                            }

                            if (root.isMember("p"))
                            {
                                std::uint64_t up = root["p"].asInt64();

                                if (up != remote)
                                {
                                    remote = up;
                                    basefunc_std::write_file_to_disk(r_sett, std::to_string(remote));
                                }
                            }
                        }
                        catch(std::exception& ex)
                        {

                        }
                    }
                }
            });
        }
        ~item()
        {
            if (t != nullptr) t->join();
            delete t;
        }

        std::uint64_t remote { 0 };
        rssdisk::server* rd;
        std::thread* t { nullptr };

        void process(const std::string& v)
        {
            std::int64_t tms { 0 };
            std::int64_t crc32 { 0 };

            bitbase::chars_to_numeric(v.substr(8, 8), tms);
            bitbase::chars_to_numeric(v.substr(16, 8), crc32);

            std::string filename = v.substr(24);
            auto f_end = filename.find('\0');
            if (f_end != std::string::npos)
            {
                filename = filename.substr(0, f_end);
            }

            bool ok = rd->remove_file_by_tms(filename, tms, crc32);

            //std::cout << get_segment_folder(filename) << "/" << filename << " sz " << filename.size() << " " << tms << " " << crc32 << " ok " << ok << std::endl;
        }

        std::int32_t get_segment_folder(const std::string& filename)
        {
            std::hash<std::string> h;
            return h(filename) % 1000;
        }
    };

    static std::atomic_bool is_running;

    std::map<std::string, item*> clients;
    std::string path;

    constexpr const static char* fldr = "rmt";
    constexpr const static std::int32_t strp { 1 };
};

class hstf_server
{
public:
    hstf_server() = default;

    bool init(hstf* _HSTF, const std::int32_t port, const std::int32_t threads)
    {
        HSTF = _HSTF;
        server.config.address = "0.0.0.0";
        server.config.port = port;
        server.config.thread_pool_size = threads;

        basefunc_std::cout("Http server started on port: " + std::to_string(port) + " wtih " + std::to_string(threads) + " threads", "start");

        server.resource["^/hstf$"]["POST"] = [&](std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request)
        {
            std::string r = HSTF->get(request->content.string());
            *response << "HTTP/1.1 200 OK\r\nContent-Length: " << r.size() << "\r\n\r\n" << r;
        };

        server_thread = new std::thread([this]()
        {
            server.start();
        });

        return true;
    }

    void stop()
    {
        if (server_thread != nullptr)
        {
            server.stop();
            if (server_thread->joinable()) server_thread->join();
            server_thread = nullptr;
        }
    }

private:
    hstf* HSTF;
    HttpServer server;
    std::thread* server_thread { nullptr };
};

class hstf_eco
{
public:
    hstf_eco() = default;

    bool init(const std::string& cfg, rssdisk::server* rd)
    {
        const rssdisk::server::server_conf& server_sett = rd->get_server_settings2();

        if (!HSTF.init(cfg, server_sett.self_ip)) return false;
        if (!HSTF_CLIENT.init(cfg, server_sett.self_ip, rd)) return false;
        if (!HSTF_SERVER.init(&HSTF, server_sett.http_conf.port, server_sett.http_conf.threads)) return false;

        return true;
    }

    void stop()
    {
        HSTF_CLIENT.stop();
        HSTF_SERVER.stop();
    }

    void add(const std::string& filename, std::int64_t tms, std::int64_t crc32)
    {
        HSTF.add(filename, tms, crc32);
    }

    void clear()
    {
        HSTF.clear();
    }

private:
    hstf HSTF;
    hstf_client HSTF_CLIENT;
    hstf_server HSTF_SERVER;
};

#endif // HSTF_H
