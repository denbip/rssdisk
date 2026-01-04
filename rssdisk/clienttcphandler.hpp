#ifndef CLIENT_TCP_HANDLER_H
#define CLIENT_TCP_HANDLER_H

#include "../../libs/basefunc_std.h"
#include "../../libs/timer.h"
#include "../../libs/json/json/json.h"
#include "../../libs/network/epoll_v2.hpp"
#include "../../libs/cache/sized_vector.h"

#include "disk_server.hpp"
#include "disk_helper.hpp"
#include "extentions/e_manager.h"
#include "extentions/c_manager.h"

#include <thread>

class ClientTCPHandler
{
public:
    ClientTCPHandler(rssdisk::server* diskServer_, storage::CDatabaseManager* cdbInstance_, storage::EDatabaseManager* edbInstance_) :
        diskServer(diskServer_),
        cdbInstance(cdbInstance_),
        edbInstance(edbInstance_)
    {
    }

    ~ClientTCPHandler()
    {
        terminate();
    }

    void initiate()
    {
        if (!isActive)
        {
            isActive = true;

            const rssdisk::server::server_conf& serverConfig = diskServer->get_server_settings2();

            workerThreadPool = new thread_pool::thread_pool(serverConfig.tcp_conf.worker_threads, false);

            tcpHandler = new epoll_v2 { serverConfig.tcp_conf.threads, serverConfig.tcp_conf.delimeter, serverConfig.tcp_conf.echo, true, serverConfig.secure_conf.gost_enc };
            tcpHandler->on_message = [this](epoll_v2::socket_client& client, std::string &&message, thread_pool::thread_pool& threadPool, epoll_v2::responser& responder)
            {
                if (!client.is_authorized)
                {
                    const rssdisk::server::server_conf& serverConfig = diskServer->get_server_settings2();

                    if (serverConfig.secure_conf.auth.empty())
                    {
                        client.is_authorized = true;
                        basefunc_std::cout("Success", "client_auth");
                    }
                    else if (serverConfig.secure_conf.auth.compare(message) == 0)
                    {
                        client.is_authorized = true;
                        basefunc_std::cout("Success", "client_auth");
                    }
                    else
                    {
                        basefunc_std::cout("Fail", "client_auth", basefunc_std::COLOR::RED_COL);
                    }
                }
                else
                {
                    std::int32_t fileDescriptor = client.fd;
                    std::uint64_t acceptedCount = client.count_accepted_on;

                    if (message.size() >= 7)
                    {
                        unsigned currentWorks = threadPool.get_count_works();
                        std::size_t totalWorks = threadPool.get_count_threads();
                        epoll_v2::secure secureData = client._s;

                        unsigned workerCurrentWorks = workerThreadPool->get_count_works();
                        std::size_t workerTotalWorks = workerThreadPool->get_count_threads();

                        auto task = [this, &client, &responder, fileDescriptor, acceptedCount, message, currentWorks, totalWorks, secureData, workerCurrentWorks, workerTotalWorks]()
                        {
                            timer timerInstance;

                            const rssdisk::server::server_conf& serverConfig = diskServer->get_server_settings2();

                            std::string queryType = message.substr(0, 2);
                            std::string identifier = message.substr(2, 5);
                            std::string query = message.substr(7);

                            std::int32_t queryTypeInt { 0 };
                            basefunc_std::stoi(queryType, queryTypeInt);

                            switch (queryTypeInt)
                            {
                                case 1: //file_exists
                                {
                                    auto f_jdb = query.find("?jdb");
                                    if (f_jdb != std::string::npos)
                                    {
                                        query = query.substr(0, f_jdb);
                                    }

                                    bool is_file_exists { false };

                                    std::string subfolder;
                                    auto f_subfolder = query.find("/");
                                    if (f_subfolder != std::string::npos)
                                    {
                                        subfolder = query.substr(0, f_subfolder);
                                        query = query.substr(f_subfolder + 1);
                                    }

                                    auto f_dir = serverConfig.dirs.find(subfolder);
                                    if (f_dir == serverConfig.dirs.end())
                                    {
                                        responder.add(fileDescriptor, acceptedCount, &secureData, "0107" + identifier + query);
                                    }
                                    else
                                    {

                                        query = std::to_string(diskServer->get_segment_folder(query)) + "/" + query;

                                        const rssdisk::server::server_conf::directory& d = f_dir->second;
                                        is_file_exists = disk::helper::is_file_exists(d.p + query);

                                        if (is_file_exists) responder.add(fileDescriptor, acceptedCount, &secureData, "0101" + identifier + query);
                                        else responder.add(fileDescriptor, acceptedCount, &secureData, "0107" + identifier + query);
                                    }
                                    break;
                                }
                                case 2: //get_file
                                {
                                    std::string content;
                                    Json::Value res = diskServer->read_file(query, content);

                                    content = "02" + res["status"].asString() + identifier + content;

                                    responder.add(fileDescriptor, acceptedCount, &secureData, std::move(content));
                                    break;
                                }
                                case 3: //is_enought_storage_for_accepting_file
                                {
                                    bool is_file_accepting { false };

                                    auto f_dir = serverConfig.dirs.find(query); //qry is subfolder
                                    if (f_dir == serverConfig.dirs.end())
                                    {
                                        responder.add(fileDescriptor, acceptedCount, &secureData, "0303" + identifier);
                                    }
                                    else
                                    {
                                        const rssdisk::server::server_conf::directory& d = f_dir->second;

                                        if (serverConfig.enable_write)
                                        {
                                            if (d.curr_size_b < d.max_size_b)
                                            {
                                                is_file_accepting = true;
                                                break;
                                            }
                                        }

                                        if (is_file_accepting) responder.add(fileDescriptor, acceptedCount, &secureData, "0301" + identifier);
                                        else responder.add(fileDescriptor, acceptedCount, &secureData, "0303" + identifier);
                                    }
                                    break;
                                }
                                case 4: //send_file
                                {
                                    Json::Value res = diskServer->accept_file(std::move(query));
                                    responder.add(fileDescriptor, acceptedCount, &secureData, "04" + res["status"].asString() + identifier + res.get("errorstr", "").asString());
                                    /*if (res["status"].asString().compare("01") == 0)
                                    {
                                        std::string event;
                                        event.reserve(128);
                                        ((((event += "04|") += res["headers"].asString()) += "|") += res["file_name"].asString()) += "|";
                                        responder.add_event(event);
                                    }*/
                                    break;
                                }
                                case 5: //get_stat
                                {
                                    std::string stat { "{\"sub\":[" };
                                    bool f1 { true };
                                    for (const auto& it : serverConfig.dirs)
                                    {
                                        if (!f1) stat += ",";

                                        stat += "{\"n\":\"";
                                        stat += (it.first.empty()) ? "main(empty)" : it.first;
                                        stat += "\",\"f\":[";

                                        stat += "{\"n\":\"0\",\"p\":\"" + it.second.p +
                                                "\",\"s_mb\":" + std::to_string(it.second.curr_size_b / (1024 * 1024)) +
                                                ",\"max_s_mb\":" + std::to_string(it.second.max_size_b / (1024 * 1024)) +
                                                "}";

                                        stat += "]}";

                                        f1 = false;
                                    }

                                    stat += "]}";

                                    responder.add(fileDescriptor, acceptedCount, &secureData, "0501" + identifier + stat);
                                    break;
                                }
                                case 6: //get_all_files_names
                                {
                                    std::string content;
                                    for (const auto& it : serverConfig.dirs)
                                    {
                                        std::vector<std::string> ls_dir = disk::helper::list_dir(it.second.p, query);
                                        for (const auto& s : ls_dir)
                                        {
                                            if (!content.empty()) content += ",";
                                            if (!it.first.empty()) (content += it.first) += "/";
                                            content += s;
                                        }
                                    }

                                    responder.add(fileDescriptor, acceptedCount, &secureData, "0601" + identifier + commpression_zlib::compress_string(content));
                                    break;
                                }
                                case 7: //get_file_info
                                {
                                    std::string content;
                                    Json::Value res = diskServer->read_file_info(query, content);
                                    responder.add(fileDescriptor, acceptedCount, &secureData, "07" + res["status"].asString() + identifier + content);
                                    break;
                                }
                                case 8: //remove_file
                                {
                                    bool ok = diskServer->remove_file_by_tms(query, 0, 0, true);
                                    std::string status = ok ? "01" : "02";
                                    responder.add(fileDescriptor, acceptedCount, &secureData, "08" + status + identifier);
                                    break;
                                }
                                case 29://get_files_compressed
                                case 9: //get_files
                                {
                                    std::string jdb_query;
                                    auto f_jdb = query.find("?jdb");
                                    if (f_jdb != std::string::npos)
                                    {
                                        jdb_query = query.substr(f_jdb);
                                        query = query.substr(0, f_jdb);
                                    }

                                    Json::Value res;

                                    std::vector<std::string> files = basefunc_std::split(query, ',');
                                    std::string content;
                                    for (const auto& file : files)
                                    {
                                        content.clear();
                                        Json::Value r = diskServer->read_file(file + jdb_query, content, rssdisk::read_options::no_compress | rssdisk::read_options::no_header);

                                        if (!content.empty())
                                        {
                                            if (queryTypeInt == 29)
                                            {
                                                try
                                                {
                                                    rssdisk::read_info ri;
                                                    rssdisk::fetch_content(content, ri, 0);

                                                    if (ri.file_type != rssdisk::w_type::insert_only_without_compress && ri.file_type != rssdisk::w_type::updatable_without_compress && ri.file_type != rssdisk::w_type::appendable) content = commpression_zlib::decompress_string(ri.content);
                                                    else content = ri.content;
                                                }
                                                catch(...) { }
                                            }

                                            try
                                            {
                                                Json::Value root;
                                                if (Json::Reader().parse(content, root)) r["_c_"] = root;
                                            }
                                            catch(...)
                                            {
                                                continue;
                                            }
                                        }

                                        r["_fn_"] = file;
                                        res.append(r);
                                    }

                                    responder.add(fileDescriptor, acceptedCount, &secureData, "0901" + identifier + res.toString());
                                    break;
                                }
                                case 11: //get data event
                                {
                                    storage::EDatabaseManager::EventResult r = edbInstance->retrieve(query);
                                    responder.add(fileDescriptor, acceptedCount, &secureData, "1101" + identifier + bitbase::numeric_to_chars(r.eventType) + r.payload);
                                    break;
                                }
                                case 22:
                                case 12: //set data event
                                {
                                    storage::EDatabaseManager::EventResult r = edbInstance->insert(query);
                                    if (queryTypeInt == 12) responder.add(fileDescriptor, acceptedCount, &secureData, "1201" + identifier);

                                    epoll_v2::event_edb event;
                                    event.data.reserve(128);
                                    event.type = r.eventType;
                                    (((event.data += "12|") += std::to_string(r.eventType)) += "|") += r.payload;
                                    responder.add_event(event);
                                    break;
                                }
                                case 13: //listen events
                                {
                                    std::vector<std::int32_t> v;
                                    basefunc_std::get_vector_from_string(query, v);
                                    std::unordered_set<std::int32_t> s;
                                    for (auto i = 1; i < v.size(); ++i)
                                    {
                                        s.emplace(v[i]);
                                    }

                                    responder.listen_events(fileDescriptor, s);
                                    basefunc_std::cout("Event listener", "New event listener has been registred");

                                    std::this_thread::sleep_for(std::chrono::milliseconds(100));

                                    std::vector<storage::EDatabaseManager::EventResult> r = edbInstance->fetchEvents(query);
                                    for (const storage::EDatabaseManager::EventResult& it : r)
                                    {
                                        responder.add(fileDescriptor, acceptedCount, &secureData, "1401" + identifier + bitbase::numeric_to_chars(it.eventType) + it.payload);
                                    }
                                    break;
                                }
                                case 14: //get events
                                {
                                    std::vector<storage::EDatabaseManager::EventResult> r = edbInstance->fetchEvents(query);
                                    for (const storage::EDatabaseManager::EventResult& it : r)
                                    {
                                        responder.add(fileDescriptor, acceptedCount, &secureData, "1401" + identifier + bitbase::numeric_to_chars(it.eventType) + it.payload);
                                    }
                                    break;
                                }
                                case 20: //get_cdb
                                {
                                    std::string content;
                                    if (cdbInstance->retrieve(query, content))
                                    {
                                        std::string w_type { std::to_string(static_cast<std::int32_t>(rssdisk::w_type::cdb)) };
                                        if (w_type.size() == 1) w_type = "0" + w_type;
                                        responder.add(fileDescriptor, acceptedCount, &secureData, std::string("2001") + identifier + w_type + "00000" + content);
                                    }
                                    else
                                    {
                                        responder.add(fileDescriptor, acceptedCount, &secureData, std::string("2007") + identifier);
                                    }
                                    break;
                                }
                                case 21: //get_cdb_all
                                {
                                    std::string content = cdbInstance->searchAll(query);
                                    responder.add(fileDescriptor, acceptedCount, &secureData, std::string("2101") + identifier + content);
                                    break;
                                }
                                case 24:
                                case 25: //set_cdbm
                                {
                                    if (cdbInstance->insert(query))
                                    {
                                        if (queryTypeInt == 24) responder.add(fileDescriptor, acceptedCount, &secureData, std::string("2401") + identifier);
                                    }
                                    else
                                    {
                                        if (queryTypeInt == 24) responder.add(fileDescriptor, acceptedCount, &secureData, std::string("2404") + identifier);
                                    }
                                    break;
                                }
                                case 26: //ping
                                {
                                    responder.add(fileDescriptor, acceptedCount, &secureData, std::string("2601") + identifier);
                                    break;
                                }
                                case 31: //get events splitted
                                {
                                    std::vector<storage::EDatabaseManager::EventResult> r = edbInstance->fetchEvents(query);
                                    std::string content;
                                    for (const storage::EDatabaseManager::EventResult& it : r)
                                    {
                                        content += it.payload;
                                    }
                                    responder.add(fileDescriptor, acceptedCount, &secureData, "3101" + identifier + content);
                                    break;
                                }
                                case 32:
                                case 33: //set_cdbm
                                {
                                    if (cdbInstance->insert(query))
                                    {
                                        if (queryTypeInt == 32) responder.add(fileDescriptor, acceptedCount, &secureData, std::string("3201") + identifier);
                                    }
                                    else
                                    {
                                        if (queryTypeInt == 32) responder.add(fileDescriptor, acceptedCount, &secureData, std::string("3204") + identifier);
                                    }
                                    break;
                                }
                                default: break;
                            }

                            if (timerInstance.elapsed_mili() > 1000)
                            {
                                if (query.size() <= 1000) basefunc_std::log(std::to_string(timerInstance.elapsed_mili()) + "ms queryType=" + queryType + "&query=" + query, "$date/long_query");
                                else basefunc_std::log(std::to_string(timerInstance.elapsed_mili()) + "ms queryType=" + queryType + "&query=" + query.substr(0, 1000) + "&querySize=" + std::to_string(query.size()), "$date/long_query");
                            }
                        };

                        if (currentWorks >= totalWorks)
                        {
                            basefunc_std::log("currentWorks=" + std::to_string(currentWorks) + " total=" + std::to_string(totalWorks), "$date/full_thread_stack_tcp");
                        }
                        if (workerCurrentWorks >= workerTotalWorks)
                        {
                            basefunc_std::log("currentWorks=" + std::to_string(workerCurrentWorks) + " total=" + std::to_string(workerTotalWorks), "$date/full_thread_stack_worker");
                        }

                        workerThreadPool->push_back(std::move(task));
                    }
                    else if (message.size() == 2 && message.compare("06") == 0)
                    {
                        responder.listen_events(fileDescriptor, { });
                        basefunc_std::cout("Event listener", "New event listener has been registered");
                    }
                }
            };

            t = std::thread([this, serverConfig]()
            {
                tcpHandler->listen("0.0.0.0", serverConfig.tcp_conf.port, isActive);
            });
        }
    }

    void terminate()
    {
        if (isActive)
        {
            workerThreadPool->stop();

            isActive = false;
            if (t.joinable()) t.join();
            delete tcpHandler;

            delete workerThreadPool;
        }
    }

private:
    rssdisk::server* diskServer;
    epoll_v2* tcpHandler;
    std::thread t;
    std::atomic_bool isActive = ATOMIC_VAR_INIT(false);
    storage::EDatabaseManager* edbInstance;
    storage::CDatabaseManager* cdbInstance;

    thread_pool::thread_pool* workerThreadPool;

};

#endif
