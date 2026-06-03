#ifndef rssdisk_pool_H
#define rssdisk_pool_H

#include <set>

#include "../../libs/network/tcp_client.hpp"
#include "../../libs/json/json/json.h"
#include "../../libs/thread_timer.h"
#include "../../libs/crc32.h"
#include "../../libs/network/network_std.h"
#include "../../libs/time_cache.h"
#include "../../libs/date_time.h"
#include "rssdisk_w_type.hpp"
#include "../../libs/streamer.h"

namespace rssdisk
{
    struct helper
    {
        typedef std::function<void(const std::string& text, const std::string& header, const std::string& topic, std::int32_t ttl)> alert_callback;
        static alert_callback alert_cb;
        static void alert(const std::string& text, const std::string& header, const std::string& topic, std::int32_t ttl)
        {
            if (alert_cb != nullptr) alert_cb(text, header, topic, ttl);
        }
    };

    class client
    {
    public:
        struct evetne_item
        {
            std::unordered_set<std::uint32_t> ips;
        };

        typedef std::function<void(const evetne_item& ev, const std::string& str)> read_callback;

        enum class rw_preference
        {
            any,
            specified_group
        };

        enum class cdb_out_data_format
        {
            json,
            data,
            prefer_json
        };

        enum class read_res
        {
            ok,
            fail,
            file_not_found,
            error_to_connect_to_server,
            error_read_responce_from_server,
            error_send_request_from_server,
            timeout,
            servers_not_found,
            error_specify_the_group,
            count_tms_files_less_than_requeired,
            readed_particulary
        };

        enum class read_info_type
        {
            file_exists,
            file_info,
            remove_file,
            get_files,
            read_files,
            read_files_many,
            read_files_many_comressed,
            read_file_tms_newest,
            read_cdb,
            read_cdb_tms_newest,
            read_cdb_all,
            read_streamed,
            read_streamed_unsecure,
            read_sdb,
            read_edb_events
        };

        struct read_responce
        {
            read_responce(read_res status_) : status(status_) { }
            read_responce(read_res status_, std::uint32_t ip_n) : status(status_), ip_address_readed(ip_n) { }
            read_responce(read_res status_, std::uint32_t ip_n, const read_info& _ri) : status(status_), ip_address_readed(ip_n), ri(_ri) { }
            read_res status;
            std::uint32_t ip_address_readed = 0;
            std::string data;
            std::int32_t group = -1;
            read_info ri;
        };

        struct read_responce_tcp
        {
            tcp_client::socket_status status;
            std::uint32_t ip_address_readed = 0;
            std::string data;
        };

        struct write_info
        {
            write_info() = default;
            write_info(std::int32_t w_count) : writed_count(w_count) { }

            std::int64_t file_tms = 0;
            std::int64_t file_crc32 = 0;
            std::int32_t writed_count = 0;
            std::int32_t count_attempts = 0;
            std::set<std::uint32_t> writed_ips;
        };

        struct write_options
        {
            enum class wait_answer
            {
                default_,
                wait,
                no_wait
            };

            write_options() {}

            std::int32_t ttl = 0;
            int32_t count_server_to_write = 3;
            std::int32_t part_timeout_milisec = 2000;
            std::int32_t count_parts = 3;
            std::vector<std::int32_t> preffered_netwok_groups = {};
            rw_preference rw_pref = rw_preference::any;
            wait_answer w_answer = wait_answer::default_;
            std::set<std::uint32_t> only_ips;
        };

        client();
        ~client();

        bool init(const std::string& config_file, std::vector<std::int32_t> netwok_groups);
        bool init_settings(const std::string& config_file, std::vector<std::int32_t> netwok_groups);
        void wait_for_init_connections(const std::set<std::int32_t>& wait_network_groups, std::int32_t wait_ms = 2000, bool alert_not_connected = true);
        void stop();

        static std::string prepare_sdb(const std::string& data, const Json::Value& sett);
        static std::string prepare_sdb(const std::string& data, const std::string& sett);
        static std::string prepare_jdb(const Json::Value& data, const Json::Value& sett);
        static std::string prepare_jdb(const std::string& data, const std::string& sett);
        static std::string prepare_cdb(const std::string& key, const std::string& val, std::int32_t ttl);
        static std::string prepare_edb(const std::int32_t type_event, const std::string& val, std::int32_t ttl);
        static void fetch_edb_content(const std::string& data, std::size_t start_position, std::function<void(const date_time& dt, const streamer<>& d)> f);

        client::write_info write_file(w_type write_type, const std::string& filename, const std::string& content_, write_options w_opt = {});

        read_responce read_file(const std::string& filename,
                                std::string& content,
                                std::int32_t find_timeout_milisec = 2000,
                                std::int32_t part_timeout_milisec = 2000,
                                int32_t count_parts = 2,
                                std::vector<std::int32_t> preffered_netwok_groups = {},
                                rw_preference rw_pref = rw_preference::any,
                                std::unordered_set<std::uint32_t> only_ips = {},
                                bool read_many = false,
                                bool read_sdb = false);

        read_responce get_all_files(std::uint32_t ip, std::string& content, std::int32_t timeout = 2000, std::string contain = "");

        void set_onchange_status_callback(tcp_client::status_callback s_);

        client* operator ->() { return this; }

        std::int32_t get_count_servers_connected(const write_options& w_opt);
        std::int32_t get_count_servers(const write_options& w_opt);

        std::vector<read_responce> get_servers_statuses(std::int32_t timeout_mili_sec = 1000);
        std::vector<read_responce> get_servers_ping(std::int32_t timeout_mili_sec = 1000);

        read_res command(read_info_type r_type,
                         std::vector<read_info>& indexex,
                         const std::string &filename,
                         const int32_t count_parts,
                         std::int32_t timeout_mili_sec,
                         std::vector<std::int32_t> preffered_netwok_groups,
                         rw_preference rw_pref,
                         std::unordered_set<std::uint32_t> only_ips = {});

        read_res command_seq(read_info_type r_type,
                             std::vector<read_info>& indexex,
                             const std::string &filename,
                             int32_t timeout_one_server,
                             std::int32_t timeout_general,
                             std::vector<std::int32_t> preffered_netwok_groups,
                             rw_preference rw_pref,
                             std::set<std::uint32_t> only_ips = {},
                             bool to_all = false);

        read_res command_partitional(std::unordered_map<std::string, std::string>& sub_readed,
                                     const std::string &filename,
                                     std::unordered_set<std::string> sub_elements,
                                     std::int32_t timeout_mili_sec,
                                     std::vector<std::int32_t> preffered_netwok_groups,
                                     rw_preference rw_pref,
                                     bool skip_aes);

        void re_open_tcp();
        std::vector<read_responce_tcp> get_tcp_statuses();

        static Json::Value parse_cdb_all(const std::vector<read_info>& info, cdb_out_data_format frm);

        static bool fetch_sdb_string(const read_info& it, std::unordered_map<std::int64_t, std::string>& ret)
        {
            std::uint8_t holder_presize { 0 };
            bitbase::chars_to_numeric(it.content.substr(0, 1), holder_presize);
            //std::cout << network_std::inet_ntoa(it.ip) << " holder_presize " << (int)holder_presize << " header " << it.header << " it.content.size() " << it.content.size() << std::endl;
            std::size_t p { 1 };
            while (p < it.content.size())
            {
                std::int64_t i { 0 };
                if (p + 8 > it.content.size()) break;
                bitbase::chars_to_numeric(it.content.substr(p, 8), i);
                p += 8;

                std::uint32_t sz { 0 };
                if (p + holder_presize > it.content.size()) break;
                bitbase::chars_to_numeric(it.content.substr(p, holder_presize), sz);

                p += holder_presize;
                if (p + sz > it.content.size()) break;
                std::string data { it.content.substr(p, sz) };
                p += sz;

                ret.insert( { i, std::move(data) } );
            }
            if (p != it.content.size())
            {
                return false;
            }

            return true;
        }

        template<std::size_t BYTES_STR_SIZE, class T>
        static bool fetch_sdb(const read_info& it, std::unordered_map<std::int64_t, T>& ret)
        {
            std::uint8_t holder_presize { 0 };
            bitbase::chars_to_numeric(it.content.substr(0, 1), holder_presize);
            //std::cout << "holder_presize " << (int)holder_presize << " header " << it.header << " it.content.size() " << it.content.size() << std::endl;
            std::size_t p { 1 };
            while (p < it.content.size())
            {
                std::int64_t i { 0 };
                if (p + 8 > it.content.size()) break;
                bitbase::chars_to_numeric(it.content.substr(p, 8), i);
                p += 8;

                std::uint32_t sz { 0 };
                if (p + holder_presize > it.content.size()) break;
                bitbase::chars_to_numeric(it.content.substr(p, holder_presize), sz);

                p += holder_presize;
                if (p + sz > it.content.size()) break;
                std::string data { it.content.substr(p, sz) };
                p += sz;

                streamer<BYTES_STR_SIZE> str { std::move(data) };
                T t2;
                str >> t2;
                t2.id = i;

                ret.insert( { i, std::move(t2) } );
            }
            if (p != it.content.size())
            {
                return false;
            }

            return true;
        }

        template<std::size_t BYTES_STR_SIZE, class T>
        static bool fetch_sdb(const std::vector<read_info>& info, std::unordered_map<std::int64_t, T>& ret, std::size_t index = 0)
        {
            if (index < info.size())
            {
                return fetch_sdb<BYTES_STR_SIZE, T>(info[index], ret);
            }
            else
            {
                return false;
            }
        }

        std::int32_t pool_index = 0;

    private:
        std::vector<std::shared_ptr<tcp_client> > clients;
        std::mutex lock_disabled_write_clients;
        std::map<std::int32_t, std::map<std::string, date_time>> disabled_write_clients;

        std::int32_t pop_index_rand(std::map<std::int32_t, std::vector<std::int32_t> >& servers, std::unordered_set<std::int32_t>& used_groups, std::int32_t preffered_network);

        std::vector<std::int32_t> get_free_size_servers(std::int32_t timeout_mili_sec = 1000);



        static const crc32m CRC32;

        //events
        friend class pool;
        read_callback cb_events;
        time_cache<std::string, evetne_item> event_items;
        void listen_events(const std::unordered_set<std::int32_t>& types, read_callback c_, std::int32_t read_past_data_sec);
        void dispatch_events(int minimum_m_sec_to_dispatch);
    };

    class pool
    {
        class cli_mutex
        {
        public:
            cli_mutex(std::int32_t start_min_clients_, std::int32_t min_clients_, std::int32_t max_clients_, std::vector<std::int32_t> netwok_groups_) : start_min_clients(start_min_clients_), min_clients(min_clients_), max_clients(max_clients_), netwok_groups(netwok_groups_)
            {

            }

            std::shared_ptr<client> get(std::int32_t& index, const std::set<std::int32_t>& wait_network_groups)
            {
                std::shared_ptr<client> ret { nullptr };
                index = -1;
                bool need_to_connect { false };

                {
                    std::lock_guard<std::mutex> _{ lock };
                    for (auto i = 0; i < clients.size(); ++i)
                    {
                        std::int32_t _pool_index = clients[i]->pool_index;

                        if (in_use.count(_pool_index) == 0)
                        {
                            ret = clients[i];
                            index = _pool_index;
                            using_clients[_pool_index] = date_time::current_date_time();
                            in_use.emplace(_pool_index);
                            break;
                        }
                    }

                    if (index == -1 && clients.size() < max_clients) //not found
                    {
                        ret = std::make_shared<rssdisk::client>();
                        bool ok_init = ret->init_settings(settings_path, netwok_groups);

                        if (ok_init)
                        {
                            need_to_connect = true;

                            if (!free_indexex.empty())
                            {
                                std::int32_t _pool_index = free_indexex.back();
                                free_indexex.pop_back();

                                ret->pool_index = _pool_index;
                                clients.push_back(ret);
                                index = _pool_index;
                                using_clients[_pool_index] = date_time::current_date_time();
                                in_use.emplace(_pool_index);

                                //basefunc_std::log("new " + std::to_string(_pool_index), "debug_rssdisk_pool");
                            }
                        }
                    }
                }

                if (need_to_connect)
                {
                    ret->wait_for_init_connections(wait_network_groups, 2000, true);
                }

                if (index == -1)
                {
                    //std::cout << "get" << std::endl;
                    helper::alert("Pool is full", "rssdisk_pool", "rssdisk", 1);
                }

                return ret;
            }

            void free(std::int32_t index)
            {
                std::lock_guard<std::mutex> _{ lock };
                in_use.erase(index);
                free_cond.notify_all();
            }

            void init(const std::string& settings_path_)
            {
                std::lock_guard<std::mutex> _{ lock };
                settings_path = settings_path_;
                int i = 0;
                for (; i < start_min_clients; ++i)
                {
                    std::shared_ptr<client> c = std::make_shared<rssdisk::client>();
                    bool ok_init = c->init_settings(settings_path, netwok_groups);
                    if (ok_init)
                    {
                        c->pool_index = i;
                        c->wait_for_init_connections({}, 0, false);
                        clients.push_back(c);
                    }
                    else
                    {
                        free_indexex.push_back(i);
                    }
                }
                for (; i < max_clients; ++i)
                {
                    free_indexex.push_back(i);
                }

                std::this_thread::sleep_for(std::chrono::seconds(1)); //wait for pool connections
            }

            bool stop()
            {
                //std::cout << "stop" << std::endl;
                {
                    std::lock_guard<std::mutex> _{ lock };
                    min_clients = 0;
                    max_clients = 0;
                    free_indexex.clear();
                    clients.clear();
                }
                //std::cout << "stop2" << std::endl;

                //wait for all clients ends to use clients ? (in_use size must be 0)
                std::unique_lock<std::mutex> l { lock };
                if (!free_cond.wait_for(l, std::chrono::seconds(5), [this](){ return in_use.empty(); }))
                {
                    basefunc_std::cout("Timeout reached", "~rssdisk::pool", basefunc_std::COLOR::RED_COL);
                    return false;
                }

                return true;
            }

            void clear()
            {
                std::vector<std::shared_ptr<client>> clients_to_release;

                {
                    date_time c = date_time::current_date_time();
                    std::vector<std::int32_t> del;
                    std::lock_guard<std::mutex> _{ lock };

                    if (clients.size() > min_clients)
                    {
                        for (int i = 0; i < clients.size(); ++i)
                        {
                            std::int32_t _pool_index = clients[i]->pool_index;

                            auto f = using_clients.find(_pool_index);
                            if (f != using_clients.end())
                            {
                                if (f->second.secs_to(c) >= 3600)
                                {
                                    if (in_use.count(_pool_index) == 0)
                                    {
                                        del.push_back(_pool_index);
                                    }
                                }
                            }
                        }

                        for (auto i = 0; i < del.size(); ++i)
                        {
                            std::int32_t _pool_index_del = del[i];
                            for (int i = 0; i < clients.size(); ++i)
                            {
                                if (clients[i]->pool_index == _pool_index_del)
                                {
                                    clients_to_release.push_back(clients[i]);
                                    clients.erase(clients.begin() + i);
                                    free_indexex.push_back(_pool_index_del);
                                    //basefunc_std::log("free " + std::to_string(_pool_index_del), "debug_rssdisk_pool");
                                    break;
                                }
                            }

                            if (clients.size() <= min_clients) break;
                        }
                    }
                }
            }

        private:
            std::mutex lock;
            std::vector<std::shared_ptr<client> > clients;
            std::unordered_map<std::int32_t, date_time> using_clients;
            std::unordered_set<std::int32_t> in_use;
            std::int32_t start_min_clients;
            std::int32_t min_clients;
            std::int32_t max_clients;
            std::vector<std::int32_t> free_indexex;
            std::vector<std::int32_t> netwok_groups;
            std::string settings_path;
            std::condition_variable free_cond;
        };

    public:
        class guard
        {
        public:
            guard(cli_mutex* cl_pool_, std::int32_t index_, std::shared_ptr<client> c_) : cl_pool(cl_pool_), index(index_), c(c_) { }

            ~guard()
            {
                cl_pool->free(index);
            }

            client& operator ->() { return *c; }
            std::shared_ptr<client> get() { return c; }

        private:
            std::shared_ptr<client> c = nullptr;
            std::int32_t index = -1;
            cli_mutex* cl_pool;
        };

        pool(std::int32_t start_min_clients_, std::int32_t min_clients_, std::int32_t max_clients_, std::vector<std::int32_t> netwok_groups_ = {}) : cl_pool(start_min_clients_, min_clients_, max_clients_, netwok_groups_) { }

        ~pool()
        {
            stop();
        }

        void init(const std::string& settings_path_)
        {
            is_running.store(true);
            cl_pool.init(settings_path_);
            thead_watcher = std::thread([this]()
            {
                while (is_running.load(std::memory_order_acquire) && tm.wait_for(std::chrono::minutes(10)))
                {
                    cl_pool.clear();
                }
            });
        }

        pool::guard get(std::set<std::int32_t> wait_network_groups = {})
        {
            std::int32_t index { -1 };
            std::shared_ptr<client> c = cl_pool.get(index, wait_network_groups);
            return { &cl_pool, index, c };
        }

        void stop()
        {
            is_running.store(false);
            tm.kill();
            tm_events.kill();
            cl_pool.stop();
            if (thead_watcher.joinable()) thead_watcher.join();
            if (t_events.joinable()) t_events.join();
        }

        void events_wait(const std::unordered_set<std::int32_t>& types, rssdisk::client::read_callback cb_, int minimum_m_sec_to_dispatch, int read_past_data_sec)
        {
            t_events = std::thread([this, cb_, minimum_m_sec_to_dispatch, types, read_past_data_sec]()
            {
                while (is_running.load(std::memory_order_acquire))
                {
                    rssdisk::pool::guard _g = get();
                    if (_g.get() != nullptr)
                    {
                        _g->listen_events(types, cb_, read_past_data_sec);

                        std::int32_t t_c_cache = time(0);
                        while (is_running.load(std::memory_order_acquire) && tm_events.wait_for(std::chrono::milliseconds(minimum_m_sec_to_dispatch)))
                        {
                            if (is_running.load(std::memory_order_acquire))
                            {
                                _g->dispatch_events(minimum_m_sec_to_dispatch);

                                std::int32_t _c_time = time(0);
                                if (_c_time - t_c_cache > 600)
                                {
                                    t_c_cache = _c_time;
                                    _g->event_items.clear_cache();
                                }
                            }
                        }
                    }
                    else
                    {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                    }
                }
            });

        }

        void add_delayed_cdb(const std::string& key, const std::string& val, std::int32_t ttl)
        {
            std::string d = client::prepare_cdb(key, val, ttl);
            std::lock_guard<std::mutex> _{ lock_delayed_cdb };
            delayed_cdb += d;
        }

        void write_delayed_cdb()
        {
            std::string d;
            d.reserve(1024 * 1024);
            {
                std::lock_guard<std::mutex> _{ lock_delayed_cdb };
                std::swap(delayed_cdb, d);
            }

            for (int i = 0; i < 3; ++i)
            {
                rssdisk::pool::guard g = get();
                if (g.get() != nullptr)
                {
                    rssdisk::client::write_options opt;
                    opt.part_timeout_milisec = 1000;
                    opt.count_parts = 1;

                    g->write_file(rssdisk::w_type::cdbm, "", d, opt);

                    break;
                }
                else
                {
                    helper::alert("write_delayed_cdb. Get pool error, stage " + std::to_string(i), "rssdisk_pool", "rssdisk", 1);
                }

                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }

        static void generate_prime_number()
        {
            default_prime_number = cryptopp_::generate_prime_number(2048, true, true);
        }

        static std::string get_default_prime_number() { return default_prime_number; }

    private:
        cli_mutex cl_pool;
        std::thread thead_watcher;
        ::thread::timer tm;
        std::atomic_bool is_running = ATOMIC_VAR_INIT(false);

        //events
        std::thread t_events;
        ::thread::timer tm_events;

        //delayed cdb
        std::mutex lock_delayed_cdb;
        std::string delayed_cdb;

        static std::string default_prime_number;
    };

    class pool_guard
    {
    public:
        pool_guard(pool* _p) : p(_p) {}
        ~pool_guard()
        {
            if (p != nullptr) p->stop();
        }

    private:
        pool* p;
    };
}


#endif // rssdisk_pool_H
