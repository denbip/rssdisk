#ifndef RSSDISK_W_TYPE_HPP
#define RSSDISK_W_TYPE_HPP

#include "../../libs/basefunc_std.h"
#include "../../libs/date_time.h"
#include "../../libs/streamer.h"

#include <unordered_map>
#include <string>
#include <mutex>
#include <type_traits>

#define TIMESTAMP_RESET_FROM 1732778853

namespace rssdisk
{
    enum class w_type : int //DONT CHANGE NUMERATIONS (used in headers)
    {
        insert_only = 0,
        updatable = 1,
        insert_only_without_compress = 2,
        updatable_without_compress = 3,
        jdb = 4,
        jdb_formed = 5,
        jdb_data = 6,
        none = 7,
        edb = 8,
        ajdb = 9,
        cdb = 10,
        cdbm = 11,
        appendable = 12
    };

    class enum_to_string
    {
    public:

        static const std::string& get(const w_type value)
        {
            static std::unordered_map<w_type, std::string> strings;
            if (strings.empty())
            {
                static std::mutex _lock;
                std::lock_guard<std::mutex> _{ _lock };

                if (strings.empty())
                {
#define INSERT_ELEMENT(p) strings[p] = #p
                    INSERT_ELEMENT(w_type::insert_only);
                    INSERT_ELEMENT(w_type::updatable);
                    INSERT_ELEMENT(w_type::insert_only_without_compress);
                    INSERT_ELEMENT(w_type::updatable_without_compress);
                    INSERT_ELEMENT(w_type::jdb);
                    INSERT_ELEMENT(w_type::jdb_formed);
                    INSERT_ELEMENT(w_type::jdb_data);
                    INSERT_ELEMENT(w_type::none);
                    INSERT_ELEMENT(w_type::edb);
                    INSERT_ELEMENT(w_type::ajdb);
                    INSERT_ELEMENT(w_type::cdb);
                    INSERT_ELEMENT(w_type::cdbm);
                    INSERT_ELEMENT(w_type::appendable);
#undef INSERT_ELEMENT
                }
            }

            return strings[value];
        }
    };

    enum class read_options : int //bits
    {
        none = 0,
        no_compress = 1<<0,
        no_header = 1<<1
    };

    inline read_options operator|(read_options a, read_options b)
    {
        return static_cast<read_options>(static_cast<int>(a) | static_cast<int>(b));
    }

    class server_response
    {
    public:
        enum class status : std::int32_t
        {
            ok = 1,
            cant_read_file = 6,
            file_not_found = 7,
            file_name_must_not_be_empty = 5,
            file_busy = 8,
            all_directories_are_full = 3,
            cant_write_file_to_disk = 4,
            to_small_content_size = 2,
            too_long_filename = 9,
            auth_failed = 10,
            incorrect_secure_key = 11,
            write_is_disabled = 12,
            read_is_disabled = 13,
            subfolder_not_found = 14,
            setting_not_found = 15,
            errorstr = 16
        };

        static const std::string& get(std::int32_t s)
        {
            static const std::unordered_map<std::int32_t, std::string> texts
            {
                { 1, "ok" },
                { 2, "to_small_content_size" },
                { 3, "all_directories_are_full" },
                { 4, "cant_write_file_to_disk" },
                { 5, "file_name_must_not_be_empty" },
                { 6, "cant_read_file" },
                { 7, "file_not_found" },
                { 8, "file_busy" },
                { 9, "too_long_filename" },
                { 10, "auth_failed" },
                { 11, "incorrect_secure_key" },
                { 12, "write_is_disabled" },
                { 13, "read_is_disabled" },
                { 14, "subfolder_not_found" },
                { 15, "setting_not_found" },
                { 16, "errorstr" },
            };

            auto f = texts.find(s);
            if (f != texts.end()) return f->second;
            static std::string emp;
            return emp;
        }

        /*static void set(Json::Value& res, response::status s)
        {
            std::int32_t ss_int = std::int32_t(s);
            std::string ss = std::to_string(ss_int);
            if (ss.size() == 1) ss = "0" + ss;
            res["status"] = ss;
            if (ss_int != 1)
            {
                static const std::map<std::int32_t, std::string> texts
                {
                    { 2, "to_small_content_size" },
                    { 3, "all_directories_are_full" },
                    { 4, "cant_write_file_to_disk" },
                    { 5, "file_name_must_not_be_empty" },
                    { 6, "cant_read_file" },
                    { 7, "file_not_found" },
                    { 8, "file_busy" },
                    { 9, "too_long_filename" },
                    { 10, "auth_failed" },
                    { 11, "incorrect_secure_key" },
                    { 12, "write_is_disabled" },
                    { 13, "read_is_disabled" },
                    { 14, "subfolder_not_found" },
                    { 15, "setting_not_found" },
                };

                auto f = texts.find(ss_int);
                if (f != texts.end()) res["status_m"] = f->second;
            }
        }*/
    };

    struct read_info
    {
        std::int32_t index = -1;
        std::uint32_t ip = 0;

        w_type file_type = w_type::insert_only;
        std::int64_t file_tms = 0;
        std::int64_t file_crc32 = 0;
        date_time file_created;
        std::int16_t ttl_days = -1;
        std::string header;
        std::string content;
    };

    static void fetch_content(const std::string& message_, rssdisk::read_info& ri, std::size_t start_from = 9)
    {
        std::int32_t file_type { 0 };
        basefunc_std::stoi(message_.substr(start_from, 2), file_type);
        start_from += 2;
        ri.file_type = static_cast<w_type>(file_type);

        //std::int32_t l { start_from + 19 };

        if (ri.file_type == w_type::cdb)
        {
            bitbase::chars_to_numeric(message_.substr(16, 8), ri.file_tms);
            bitbase::chars_to_numeric(message_.substr(24, 8), ri.file_crc32);
            ri.content = message_.substr(32);

            return;
        }
        else if (ri.file_type == w_type::updatable || ri.file_type == w_type::updatable_without_compress || ri.file_type == w_type::jdb || ri.file_type == w_type::ajdb)
        {
            //l = start_from + 35;

            bitbase::chars_to_numeric(message_.substr(start_from, 8), ri.file_tms);
            start_from += 8;
            bitbase::chars_to_numeric(message_.substr(start_from, 8), ri.file_crc32);
            start_from += 8;
            ri.file_created.parse_date_time(message_.substr(start_from, 19));
            start_from += 19;
        }
        else if (ri.file_type == w_type::appendable)
        {
            //l = start_from + 27;

            bitbase::chars_to_numeric(message_.substr(start_from, 8), ri.file_tms);
            start_from += 8;
            ri.file_created.parse_date_time(message_.substr(start_from, 19));
            start_from += 19;
        }
        else
        {
            ri.file_created.parse_date_time(message_.substr(start_from, 19));
            start_from += 19;
        }

        std::string _ttl;
        for (std::size_t i = start_from, n = 0; i < message_.size(), n < 10; ++i, ++n) //ttl
        {
            ++start_from;
            if (message_[i] == ' ') break;
            else _ttl += message_[i];
        }

        basefunc_std::stoi(_ttl, ri.ttl_days);

        if (message_.size() > start_from)
        {
            ri.header = message_.substr(0, start_from);
            ri.content = message_.substr(start_from);
        }
    }

}

#endif // RSSDISK_W_TYPE_HPP
