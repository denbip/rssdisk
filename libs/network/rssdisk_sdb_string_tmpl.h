#ifndef RSSDISK_SDB_STRING_TMPL_H
#define RSSDISK_SDB_STRING_TMPL_H

#include "rssdisk_w_type.hpp"
#include "rssdisk_pool.hpp"

namespace rssdisk
{
namespace sdb
{
    typedef std::tuple<std::int64_t, std::string> _tuple;

    struct info
    {
        const static constexpr std::int32_t key_bytes = 8;
        const static constexpr bool key_unsigned = false;
        const static constexpr std::size_t bytes_string = 8;
        const static constexpr std::int32_t MBW = 10; //MBytes per qry send
    };

    class string : public rssdisk::sdb_item<_tuple, info::key_bytes, info::key_unsigned, true>
    {
    public:
        class process
        {
        public:
            enum class result : std::int32_t
            {
                not_runned = -1,
                ok = 0,
                min_copies = 1,
                file_not_found = 2,
                different_data = 3,
                error_to_get_pool = 4,
                error_to_create = 5,
                cant_create_file_exists = 6,
                created_less_than_requested = 7,
                error_to_write_data = 8
            };

            class enum_to_string
            {
            public:

                static const std::string& get(const result value)
                {
                    static std::unordered_map<result, std::string> strings;
                    if (strings.empty())
                    {
                        static std::mutex _lock;
                        std::lock_guard<std::mutex> _{ _lock };

                        if (strings.empty())
                        {
#define INSERT_ELEMENT(p) strings[p] = #p
                            INSERT_ELEMENT(result::not_runned);
                            INSERT_ELEMENT(result::ok);
                            INSERT_ELEMENT(result::min_copies);
                            INSERT_ELEMENT(result::file_not_found);
                            INSERT_ELEMENT(result::different_data);
                            INSERT_ELEMENT(result::error_to_get_pool);
                            INSERT_ELEMENT(result::error_to_create);
                            INSERT_ELEMENT(result::cant_create_file_exists);
                            INSERT_ELEMENT(result::created_less_than_requested);
                            INSERT_ELEMENT(result::error_to_write_data);
#undef INSERT_ELEMENT
                        }
                    }

                    return strings[value];
                }
            };

            process(rssdisk::pool::guard& _g, const std::string& _db_name, std::size_t _min_copies) : g(_g), db_name(_db_name), min_copies(_min_copies) { }

            result check()
            {
                reads.clear();

                if (g.get() != nullptr)
                {
                    std::vector<rssdisk::read_info> indexex;
                    rssdisk::client::read_res read = g->command_seq(rssdisk::client::read_info_type::read_sdb, indexex, db_name + "?{\"i\":0}", 30000, 600000, {}, rssdisk::client::rw_preference::any, {}, true);

                    for (const rssdisk::read_info& r_i : indexex)
                    {
                        date_time d { r_i.header };
                        reads[d].push_back(r_i.ip);
                    }

                    basefunc_std::cout("Read status " + std::to_string(static_cast<int>(read)) + " count " + std::to_string(indexex.size()) + ". Reads differents: " + std::to_string(reads.size()), "sdb::string::process");

                    if (reads.size() == 1)
                    {
                        if (reads.cbegin()->second.size() >= min_copies) state = result::ok;
                        else state = result::min_copies;
                    }
                    else if (reads.empty())
                    {
                        state = result::file_not_found;
                    }
                    else
                    {
                        state = result::different_data;
                    }
                }
                else
                {
                    state = result::error_to_get_pool;
                }

                return state;
            }

            result create(const std::set<std::uint32_t>& _only_ips_w)
            {
                if (state == result::not_runned) check();
                if (state == result::file_not_found)
                {
                    streamer<rssdisk::sdb::info::bytes_string> str;
                    rssdisk::sdb::string t;
                    str << t;
                    bool ok = write_f(_only_ips_w, str, date_time::current_date_time().add_days(-1), true, true);

                    if (ok)
                    {
                        state = result::ok;
                    }
                    else
                    {
                        state = result::error_to_create;
                    }
                }
                else
                {
                    state = result::cant_create_file_exists;
                }

                return state;
            }

            result repair()
            {
                if (state == result::not_runned || state == result::error_to_write_data) check();
                if (state == result::different_data)
                {
                    std::set<std::uint32_t> only_ips_w;

                    std::set<std::uint32_t> only_ips;
                    auto last_it { reads.crbegin() };
                    date_time last_updated { last_it->first };
                    for (const auto& it : last_it->second)
                    {
                        only_ips.emplace(it);
                    }

                    ++last_it;

                    for (; last_it != reads.crend(); ++last_it)
                    {
                        date_time d { last_it->first };
                        d.add_days(1);

                        for (const auto& it : last_it->second)
                        {
                            only_ips_w.emplace(it);
                        }

                        std::vector<rssdisk::read_info> r_newest;

                        g->command_seq(rssdisk::client::read_info_type::read_sdb, r_newest, db_name + "?{\"tm\":{\"$gte\":" + std::to_string(d.get_time_from_epoch()) + "}}", 60000, 60000, {}, rssdisk::client::rw_preference::any, only_ips);

                        std::unordered_map<std::int64_t, rssdisk::sdb::string> data_back;
                        bool ok_fetch = rssdisk::client::fetch_sdb<rssdisk::sdb::info::bytes_string>(r_newest, data_back);

                        std::set<std::string> ips_str;
                        for (const auto& it : only_ips_w)
                        {
                            ips_str.emplace(network_std::inet_ntoa(it));
                        }

                        basefunc_std::cout("Fetched ok " + std::to_string(ok_fetch) + " from " + d.get_date_time() + " " + basefunc_std::get_string_from_set(ips_str), "sdb::string::process");

                        if (ok_fetch)
                        {
                            streamer<rssdisk::sdb::info::bytes_string> str;
                            for (const auto& it : data_back)
                            {
                                const rssdisk::sdb::string& t { it.second };
                                str << t;

                                if (str.size() > rssdisk::sdb::info::MBW * 1024 * 1024)
                                {
                                    if (!write_f(only_ips_w, str, last_updated, false)) return state;
                                    str.clear();
                                }
                            }

                            if (!write_f(only_ips_w, str, last_updated, true)) return state;
                        }
                    }
                }

                if (state == result::different_data) check();

                return state;
            }

            result write(const std::vector<rssdisk::sdb::string>& new_data)
            {
                if (state == result::not_runned || state == result::error_to_write_data) check();
                if (state == result::ok)
                {
                    if (new_data.empty()) return state;

                    std::set<std::uint32_t> only_ips_w;

                    auto last_it { reads.cbegin() };
                    const date_time last_updated { last_it->first };
                    for (const auto& it : last_it->second)
                    {
                        only_ips_w.emplace(it);
                    }
                    date_time new_header { last_updated };
                    new_header.add_days(1);

                    if (new_header.date_ >= date_time::current_date_time().date_)
                    {
                        basefunc_std::cout("Up to date", "sdb::string::process");
                        new_header = last_updated;
                    }

                    basefunc_std::cout("Readed last_updated " + last_updated.get_date_time() + " from " + std::to_string(only_ips_w.size()) + " servers", "sdb::string::process");

                    streamer<rssdisk::sdb::info::bytes_string> str;
                    for (const auto& it : new_data)
                    {
                        str << it;

                        if (str.size() > rssdisk::sdb::info::MBW * 1024 * 1024)
                        {
                            if (!write_f(only_ips_w, str, new_header, false))
                            {
                                state = result::error_to_write_data;
                                return state;
                            }
                            str.clear();
                        }
                    }

                    if (!write_f(only_ips_w, str, new_header, true))
                    {
                        state = result::error_to_write_data;
                        return state;
                    }
                }

                return state;
            }

        private:
            result state = result::not_runned;
            rssdisk::pool::guard& g;
            const std::string db_name;
            const std::size_t min_copies;

            std::map<date_time, std::vector<std::uint32_t>> reads;

            bool write_f(const std::set<std::uint32_t>& only_ips_w, const streamer<rssdisk::sdb::info::bytes_string>& str, const date_time& header, bool write_header, bool recreate = false)
            {
                if (str.empty()) return false;

                std::set<std::uint32_t> w_only_ips_w;
                for (const auto& it : only_ips_w)
                {
                    rssdisk::client::write_options w_opt;
                    w_opt.only_ips.emplace(it);
                    w_opt.part_timeout_milisec = 180000;

                    Json::Value settings;
                    auto& j_sett = settings["settings"];

                    j_sett["kb"] = rssdisk::sdb::info::key_bytes;
                    j_sett["ku"] = rssdisk::sdb::info::key_unsigned;
                    settings["data"]["header"] = header.get_date();
                    settings["data"]["write_header"] = write_header;
                    if (recreate) settings["settings"]["update_if_exists"] = false;

                    rssdisk::client::write_info resp = g->write_file(rssdisk::w_type::sdb, db_name, rssdisk::client::prepare_sdb(str.get(), settings), w_opt);
                    if (resp.writed_count != w_opt.only_ips.size())
                    {
                        basefunc_std::cout("Writed_count " + std::to_string(resp.writed_count) + " resp.writed_ips " + basefunc_std::get_string_from_set(resp.writed_ips) + " str.get() " + std::to_string(str.get().size()), "sdb::string::process", basefunc_std::COLOR::RED_COL);
                    }

                    for (const auto& wit : resp.writed_ips)
                    {
                        w_only_ips_w.emplace(wit);
                    }
                }

                if (only_ips_w.size() == w_only_ips_w.size()) return true;

                return false;
            }
        };

        string() = default;
        string(const std::int64_t _id, const std::string _s) : s(_s) { this->id = _id; }

        const std::string get() const
        {
            return s;
        }

    private:
        std::string s;

        void read()
        {
            id = std::get<0>(data);
            s = std::get<1>(data);
        }

        void write() const
        {
            std::get<0>(data) = id;
            std::get<1>(data) = s;
        }

        template<std::size_t b>
        friend streamer<b>& operator<<(streamer<b>& str, const sdb::string& it)
        {
            it.write();
            const rssdisk::sdb_item<_tuple, info::key_bytes, info::key_unsigned, true>* i = static_cast<const rssdisk::sdb_item<_tuple, info::key_bytes, info::key_unsigned, true>*>(&it);
            str << *i;

            return str;
        }

        template<std::size_t b>
        friend streamer<b>& operator>>(streamer<b>& str, sdb::string& it)
        {
            rssdisk::sdb_item<_tuple, info::key_bytes, info::key_unsigned, true>* i = static_cast<rssdisk::sdb_item<_tuple, info::key_bytes, info::key_unsigned, true>*>(&it);
            str >> *i;
            it.read();
            return str;
        }
    };


}
}

#endif // RSSDISK_SDB_STRING_TMPL_H
