#ifndef TIME_CACHE_H
#define TIME_CACHE_H

#include <unordered_map>
#include <pthread.h>
#include <iostream>
#include <vector>
#include <functional>
#include "basefunc_std.h"

template<class T_id, class T>
class time_cache
{
public:
    class time_cache_item
    {
    public:
        time_cache_item() : sec_to_caching(60)
        {

        }

        T value_;
        std::chrono::time_point<std::chrono::steady_clock> last_set_cache;
        int sec_to_caching;
    };


    typedef std::function<void(T& val)> update_callback;
    typedef std::function<bool(const time_cache_item& val)> get_predicate;

    time_cache()
    {
        pthread_mutex_init(&mutex, NULL);
    }

    ~time_cache()
    {
        pthread_mutex_destroy(&mutex);
    }

    bool get(const T_id& id)
    {
        auto t = std::chrono::steady_clock::now();

        pthread_mutex_lock(&mutex);

        typename std::unordered_map<T_id, time_cache_item>::iterator it = map.find(id);
        if (it != map.end())
        {
            if (std::chrono::duration_cast<std::chrono::seconds>(t - it->second.last_set_cache) < std::chrono::seconds(it->second.sec_to_caching)) //ВРЕМЯ КЭША НЕ ВЫШЛО
            {
                pthread_mutex_unlock(&mutex);
                return true;
            }
            else //УДАЛЕНИЕ
            {
                map.erase(id);
            }
        }

        pthread_mutex_unlock(&mutex);
        return false;
    }

    bool get_set(const T_id& id, int sec_to_cache)
    {
        auto t = std::chrono::steady_clock::now();

        pthread_mutex_lock(&mutex);

        typename std::unordered_map<T_id, time_cache_item>::iterator it = map.find(id);
        if (it != map.end())
        {
            if (std::chrono::duration_cast<std::chrono::seconds>(t - it->second.last_set_cache) < std::chrono::seconds(it->second.sec_to_caching)) //ВРЕМЯ КЭША НЕ ВЫШЛО
            {
                pthread_mutex_unlock(&mutex);
                return true;
            }
            else //УДАЛЕНИЕ
            {
                map.erase(id);
            }
        }

        //set
        time_cache_item nt;
        nt.last_set_cache = t;
        nt.sec_to_caching = sec_to_cache;

        map[id] = std::move(nt);

        pthread_mutex_unlock(&mutex);
        return false;
    }

    void set(const T_id& id, int sec_to_cache)
    {
        auto t = std::chrono::steady_clock::now();

        pthread_mutex_lock(&mutex);

        typename std::unordered_map<T_id, time_cache_item>::iterator it = map.find(id);
        if (it == map.end())
        {
            time_cache_item it;
            it.last_set_cache = t;
            it.sec_to_caching = sec_to_cache;

            map.insert(std::pair<T_id, time_cache_item>(id, it));
        }
        else
        {
            map[id].last_set_cache = t;
            map[id].sec_to_caching = sec_to_cache;
        }

        pthread_mutex_unlock(&mutex);
    }

    bool get(const T_id& id, T& cl)
    {
        auto t = std::chrono::steady_clock::now();

        pthread_mutex_lock(&mutex);

        typename std::unordered_map<T_id, time_cache_item>::iterator it = map.find(id);
        if (it != map.end())
        {
            if (std::chrono::duration_cast<std::chrono::seconds>(t - it->second.last_set_cache) < std::chrono::seconds(it->second.sec_to_caching)) //ВРЕМЯ КЭША НЕ ВЫШЛО
            {
                cl = it->second.value_;

                pthread_mutex_unlock(&mutex);
                return true;
            }
            else //УДАЛЕНИЕ
            {
                map.erase(id);
            }
        }

        pthread_mutex_unlock(&mutex);
        return false;
    }

    void update_or_add(const T_id& id, update_callback upd, int sec_to_cache)
    {
        auto t = std::chrono::steady_clock::now();

        pthread_mutex_lock(&mutex);

        typename std::unordered_map<T_id, time_cache_item>::iterator it = map.find(id);
        if (it != map.end())
        {
            upd(it->second.value_);
            it->second.last_set_cache = t;
        }
        else
        {
            time_cache_item it;
            it.last_set_cache = t;
            it.sec_to_caching = sec_to_cache;
            upd(it.value_);

            map.insert(std::pair<T_id, time_cache_item>(id, it));
        }

        pthread_mutex_unlock(&mutex);
    }

    std::vector<std::pair<T_id, T>> get_old_events_and_clear(get_predicate p)
    {
        std::vector<std::pair<T_id, T>> ret;

        pthread_mutex_lock(&mutex);

        std::vector<T_id> for_remove;
        for (auto it = map.begin(); it != map.end(); ++it)
        {
            if (p(it->second))
            {
                ret.push_back( { it->first, it->second.value_ } );
                for_remove.push_back(it->first);
            }
        }

        for (auto it = for_remove.begin(); it != for_remove.end(); ++it)
        {
            map.erase(*it);
        }

        pthread_mutex_unlock(&mutex);

        return ret;
    }

    void set(const T_id& id, const T& cl, int sec_to_cache)
    {
        auto t = std::chrono::steady_clock::now();

        pthread_mutex_lock(&mutex);

        typename std::unordered_map<T_id, time_cache_item>::iterator it = map.find(id);
        if (it == map.end())
        {
            time_cache_item it;
            it.last_set_cache = t;
            it.sec_to_caching = sec_to_cache;
            it.value_ = cl;

            map.insert(std::pair<T_id, time_cache_item>(id, it));
        }
        else
        {
            map[id].value_ = cl;
            map[id].last_set_cache = t;
            map[id].sec_to_caching = sec_to_cache;
        }

        pthread_mutex_unlock(&mutex);
    }

    void remove(const T_id& id)
    {
        pthread_mutex_lock(&mutex);

        map.erase(id);

        pthread_mutex_unlock(&mutex);
    }

    /**
     * ОЧИСТКА КЭША
     */
    void clear_cache()
    {
        auto t = std::chrono::steady_clock::now();

        pthread_mutex_lock(&mutex);

        std::vector<T_id> for_remove;
        for (auto it = map.begin(); it != map.end(); ++it)
        {
            if (std::chrono::duration_cast<std::chrono::seconds>(t - it->second.last_set_cache) > std::chrono::seconds(it->second.sec_to_caching)) //ВРЕМЯ КЭША ВЫШЛО
            {
                for_remove.push_back(it->first);
            }
        }

        for (auto it = for_remove.begin(); it != for_remove.end(); ++it)
        {
            map.erase(*it);
        }

        if (!for_remove.empty()) basefunc_std::clear_map<T_id, time_cache_item>(map);

        pthread_mutex_unlock(&mutex);
    }

private:
    std::unordered_map<T_id, time_cache_item> map;
    pthread_mutex_t mutex;
};

#endif // TIME_CACHE_H
