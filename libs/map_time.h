#ifndef MAP_TIME_H
#define MAP_TIME_H

#include <unordered_map>
#include <map>
#include <mutex>
#include <iostream>
#include "basefunc_std.h"
#include "bitbase.h"

template<class T_key, class T>
class map_time
{
public:
    template<class T_T>
    struct timed_val
    {
        T_T val;
        std::chrono::time_point<std::chrono::steady_clock> valid_until;
    };

    map_time() = default;

    struct stat
    {
        std::size_t count_elements = 0;
        std::size_t count_times = 0;

        friend std::ostream& operator<<(std::ostream& io, const stat& o)
        {
            return io << "count_elements: " << o.count_elements << " count_times: " << o.count_times;
        }
    };

    void insert(const T_key& _key, const T& _v, std::int32_t ttl_sec)
    {
        timed_val<T> _val;
        _val.val = _v;
        _val.valid_until = std::chrono::steady_clock::now() + std::chrono::seconds(ttl_sec);

        std::lock_guard<std::mutex> _{ lock };
        map[_key] = _val;
        times.insert( { _val.valid_until, _key } );
    }

    void erase(const T_key& _key)
    {
        std::lock_guard<std::mutex> _{ lock };
        map.erase(_key);
    }

    bool get(const T_key& key, T* val) const
    {
        std::lock_guard<std::mutex> _{ lock };
        auto f = map.find(key);
        if (f != map.end())
        {
            if (f->second.valid_until >= std::chrono::steady_clock::now())
            {
                *val = f->second.val;
                return true;
            }
        }
        return false;
    }

    void remove_if(const T_key& key, std::function<bool(const T&)> _pred)
    {
        std::lock_guard<std::mutex> _{ lock };
        auto f = map.find(key);
        if (f != map.end())
        {
            if (_pred(f->second.val))
            {
                map.erase(key);
            }
        }
    }

    void clear()
    {
        std::lock_guard<std::mutex> _{ lock };
        map.clear();
        times.clear();
        basefunc_std::clear_map(map);
    }

    static time_t steady_clock_to_time_t( std::chrono::steady_clock::time_point t )
    {
        return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() + std::chrono::duration_cast<std::chrono::system_clock::duration>(t - std::chrono::steady_clock::now()));
    }

    static std::chrono::steady_clock::time_point time_t_to_steady_clock( time_t t )
    {
        return std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::system_clock::from_time_t(t) - std::chrono::system_clock::now());
    }

    void clear_ttl()
    {
        auto n = std::chrono::steady_clock::now();

        std::lock_guard<std::mutex> _{ lock };
        if (times.empty()) return;
        auto it = times.begin();
        for (; it != times.end(); ++it)
        {
            if (it->first < n)
            {
                auto f = map.find(it->second);
                if (f != map.end())
                {
                    if (f->second.valid_until < n)
                    {
                        map.erase(it->second);
                    }
                }
            }
            else
            {
                break;
            }
        }

        if (it != times.cbegin())
        {
            times.erase(times.cbegin(), it);
        }
    }

    stat get_stat() const
    {
        stat s;

        std::lock_guard<std::mutex> _{ lock };
        s.count_elements = map.size();
        s.count_times = times.size();
        return s;
    }

    std::unordered_map<T_key, timed_val<T>> get_all() const
    {
        std::lock_guard<std::mutex> _{ lock };
        return map;
    }

    std::string serialize() const
    {
        std::string ret;

        if (serialize_function != nullptr)
        {
            std::lock_guard<std::mutex> _{ lock };
            for (const auto& it : map)
            {
                ret += serialize_function(it);
            }
        }

        return ret;
    }

    void deserialize(const std::string& _data)
    {
        if (deserialize_function != nullptr && _data.size() > 4)
        {
            std::size_t start { 0 };

            std::lock_guard<std::mutex> _{ lock };
            while (true)
            {
                if (_data.size() >= start + 4)
                {
                    std::int32_t block_size { 0 };
                    bitbase::chars_to_numeric(_data.substr(start, 4), block_size);
                    start += 4;

                    if (_data.size() >= start + block_size)
                    {
                        std::pair<T_key, timed_val<T>> item;
                        if (deserialize_function(_data.substr(start, block_size), item))
                        {
                            map.insert(item);
                            times.insert( { item.second.valid_until, item.first } );
                        }
                        start += block_size;
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
        }
    }

private:
    mutable std::mutex lock;
    std::unordered_map<T_key, timed_val<T>> map;
    std::multimap<std::chrono::time_point<std::chrono::steady_clock>, T_key> times;
public:
    std::function<std::string(const std::pair<T_key, timed_val<T>>&)> serialize_function = nullptr;
    std::function<bool(const std::string&, std::pair<T_key, timed_val<T>>&)> deserialize_function = nullptr;
};

#endif // MAP_TIME_H
