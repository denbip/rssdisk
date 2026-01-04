#ifndef DISK_SERVER_H
#define DISK_SERVER_H

#include "../../libs/json/json/json.h"
#include "../../libs/timer.h"
#include "../../libs/md5.h"
#include "../../libs/cryptopp_.h"
#include "../../libs/bitbase.h"
#include "../../libs/crc32.h"
#include "../../libs/streamer.h"
#include "../client/rssdisk_w_type.hpp"
#include "extentions/j_manager.hpp"

#include <mutex>
#include <set>
#include <map>

#include "disk_helper.hpp"

//#define debug_disable_jdb

namespace rssdisk
{
    class server
    {
    public:
        typedef std::function<void(const std::string&, std::int64_t, std::int64_t)> callback_on_write;

        server(const std::string& config_path_);

        class response
        {
        public:
            static void set(Json::Value& res, rssdisk::server_response::status s)
            {
                std::int32_t ss_int = std::int32_t(s);
                std::string ss = std::to_string(ss_int);
                if (ss.size() == 1) ss = "0" + ss;
                res["status"] = ss;
                if (ss_int != 1)
                {
                    res["status_m"] = server_response::get(ss_int);
                }
            }
        };

        class locked_files
        {
        public:
            void erase(const std::string& file_name_)
            {
                std::lock_guard<std::mutex> _{ lock };

                auto f_p = pending_update.find(file_name_);
                if (f_p != pending_update.end())
                {
                    if (f_p->second == std::this_thread::get_id())
                    {
                        pending_update.erase(file_name_);
                    }
                }

                auto f = files.find(file_name_);
                if (f != files.end())
                {
                    --f->second;
                    if (f->second == 0)
                    {
                        files.erase(file_name_);
                    }
                }
            }

            bool add(const std::string& file_name_, bool is_write)
            {
                {
                    std::lock_guard<std::mutex> _{ lock };

                    if (pending_update.find(file_name_) != pending_update.end()) return false; //file is updating

                    if (is_write)
                    {
                        pending_update.insert( { file_name_, std::this_thread::get_id() } );

                        if (files.find(file_name_) == files.end())
                        {
                            is_write = false;

                            ++files[file_name_];
                        }
                    }
                    else
                    {
                        ++files[file_name_];
                    }
                }

                if (is_write) //wait all thread will leave file
                {
                    while (true)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(30));

                        std::lock_guard<std::mutex> _{ lock };

                        if (files.find(file_name_) == files.end())
                        {
                            ++files[file_name_];
                            break;
                        }
                    }
                }

                return true;
            }

        private:
            std::mutex lock;
            std::map<std::string, std::uint8_t> files;
            std::map<std::string, std::thread::id> pending_update;
        };

        class locker_file
        {
        public:
            locker_file(locked_files* l_, const std::string& file_name_) : l(l_), file_name(file_name_) { }
            ~locker_file()
            {
                l->erase(file_name);
            }

        private:
            std::string file_name;
            locked_files* l;
        };

        struct server_conf
        {
            struct directory
            {
                directory() = default;
                directory(const directory& o)
                {
                    enabled = o.enabled;
                    p = o.p;
                    max_size_b = o.max_size_b;
                    curr_size_b = o.curr_size_b;
                    fs_block_size_b = o.fs_block_size_b;
                }

                bool enabled = false;
                std::string p;
                std::string sub_folder;
                std::int64_t max_size_b = 0;
                std::int64_t curr_size_b = 0;
                std::int64_t fs_block_size_b = 0;

                void inc_curr_disk_size(std::int32_t sz)
                {
                    std::lock_guard<std::mutex> _{ m };
                    curr_size_b += sz;
                }

            private:
                std::mutex m;
            };

            struct http
            {
                std::int32_t port = 10111;
                std::int32_t threads = 10;
            };

            struct ttl
            {
                std::string p;
            };

            struct delay_work
            {
                std::string p;
            };

            struct tmp
            {
                std::string hdd_path;
            };

            struct secure
            {
                std::string auth;
                std::string aes_key;
                std::string iv;

                bool gost_enc = false;
            };

            struct tcp
            {
                std::int32_t port;
                std::int32_t threads;
                std::int32_t worker_threads;
                std::int32_t fast_worker_threads;
                std::string delimeter;
                std::string echo;
            };

            http http_conf;
            bool enable_read = true;
            bool enable_write = true;
            std::int32_t weight = 1;
            ttl ttl_conf;
            delay_work delay_work_conf;
            tmp tmp_conf;
            secure secure_conf;
            tcp tcp_conf;

            std::string self_ip;

            mutable std::unordered_map<std::string, directory> dirs;
        };

        const server_conf& get_server_settings2() const noexcept;
        bool init_server_settings();

        Json::Value accept_file(std::string&& content, bool need_check_auth = false, bool need_check_aes = false);
        Json::Value read_file(string file_name, string &content, read_options ro = read_options::none);
        Json::Value read_file_info(string file_name, string &content);
        bool remove_file_by_tms(string file_name, std::int64_t tms, std::int64_t crc32, bool force = false);

        void run_ttl_remover();

        cryptopp_ CRYPTOPP;

        static std::int32_t get_segment_folder(const std::string& filename);

        callback_on_write on_write_ = nullptr;

    private:
        const static constexpr std::int32_t count_segment_folders = 1000; //do not change this settings on existing systems. May be changed only on a new fresh starting system

        std::string config_path;
        std::string self_name;

        server_conf config_loaded;

        locked_files lock_to_write;

        storage::JDatabaseManager jdb;

        mutable std::mutex _lock_io_stat;

        crc32m CRC32;
};
}

#endif
