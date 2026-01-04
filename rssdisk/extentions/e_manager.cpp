#include "e_manager.h"

using namespace storage;

EDatabaseManager::EventResult EDatabaseManager::insert(const std::string& d)
{
    EDatabaseManager::EventResult r;
#ifndef SIMPLE_BUILD
    if (d.size() > 8)
    {
        bitbase::chars_to_numeric(d.substr(0, 4), r.eventType);

        std::int32_t header_bytes { 0 };
        bitbase::chars_to_numeric(d.substr(4, 4), header_bytes);

        if (d.size() >= header_bytes + 8)
        {
            if (r.eventType > 0)
            {
                r.payload = d.substr(8, header_bytes);  //header name

                EDatabaseManager::EventResult s;
                s.payload = d.substr(header_bytes + 8); //value
                s.eventType = r.eventType;
                s.dtm = date_time::current_date_time();

                std::lock_guard<std::mutex> _{ mutexLock };
                eventStorage[r.eventType][r.payload] = std::move(s);
            }
            else
            {
                if (header_bytes > 12)
                {
                    r.payload = d.substr(4);  //data

                    EDatabaseManager::EventResult s;
                    bitbase::chars_to_numeric(d.substr(8, 4), s.timeToLive);
                    s.payload = r.payload; //value
                    s.eventType = r.eventType;
                    s.dtm = date_time::current_date_time();

                    std::lock_guard<std::mutex> _{ mutexLock };
                    eventQueue[r.eventType].emplace_back(std::move(s));
                }
            }
        }
    }
#endif
    return r;
}

void EDatabaseManager::reset()
{
    std::lock_guard<std::mutex> _{ mutexLock };

    for (auto& itt : eventQueue)
    {
        auto& v = itt.second;

        v.erase(std::remove_if(v.begin(), v.end(), [](const EventResult& r)
        {
            return r.dtm < date_time::current_date_time().add_secs(-r.timeToLive);
        }), v.end());
    }
}

EDatabaseManager::EventResult EDatabaseManager::retrieve(const std::string& k) const
{
    EDatabaseManager::EventResult r;
#ifndef SIMPLE_BUILD
    std::string kk;
    auto fr = k.find("|");
    if (fr != std::string::npos)
    {
        basefunc_std::stoi(k.substr(0, fr), r.eventType);
        kk = k.substr(fr + 1);
    }

    std::lock_guard<std::mutex> _{ mutexLock };
    auto f = eventStorage.find(r.eventType);
    if (f != eventStorage.end())
    {
        auto f2 = f->second.find(kk);
        if (f2 != f->second.end())
        {
            r = f2->second;
        }
    }
#endif
    return r;
}

std::vector<EDatabaseManager::EventResult> EDatabaseManager::fetchEvents(const std::string& k) const
{
    std::vector<EDatabaseManager::EventResult> ret;
#ifndef SIMPLE_BUILD
    std::vector<std::int32_t> v;
    basefunc_std::get_vector_from_string(k, v);
    std::unordered_set<std::int32_t> s;
    for (auto i = 1; i < v.size(); ++i)
    {
        s.emplace(v[i]);
    }

    std::unordered_map<std::int32_t, EventResult> r0;

    if (!v.empty())
    {
        date_time d = date_time::current_date_time();

        std::lock_guard<std::mutex> _{ mutexLock };
        for (std::int32_t i : s)
        {
            if (i > 0)
            {
                auto f = eventStorage.find(i);
                if (f != eventStorage.end())
                {
                    for (const auto& it : f->second)
                    {
                        const EventResult& r = it.second;

                        if (r.dtm.secs_to(d) < v[0])
                        {
                            ret.push_back(r);
                        }
                    }
                }
            }
            else
            {
                auto f = eventQueue.find(i);
                if (f != eventQueue.end())
                {
                    for (const EventResult& r : f->second)
                    {
                        if (r.dtm.secs_to(d) < v[0])
                        {
                            EventResult& rr = r0[i];
                            rr.eventType = i;
                            rr.payload += r.payload;
                        }
                    }
                }
            }
        }
    }

    for (auto& it : r0)
    {
        ret.push_back(std::move(it.second));
    }
#endif
    return ret;
}

std::string EDatabaseManager::generateKey(const std::string& k, const std::string& d)
{
    return bitbase::numeric_to_chars(static_cast<std::int32_t>(k.size())) + k + d;
}
