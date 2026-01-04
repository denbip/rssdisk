#ifndef DISK_USAGE_H
#define DISK_USAGE_H

#include <mutex>
#include <unordered_map>
#include <unordered_set>

class disk_usage
{
public:
    disk_usage() {}

    struct d_sz
    {
        std::int32_t sz = 0;
        std::int32_t m_sz = 0;
        std::int32_t group = 0;
    };

    struct data
    {
        std::unordered_map<std::string, std::unordered_map<std::string, d_sz>> szs;
    };

    void set_size(std::uint32_t ip, std::int32_t group, const std::string& fn, const std::string& dir, std::int32_t sz, std::int32_t m_sz)
    {
        std::lock_guard<std::mutex> _{ lock };
        d_sz& dd = d[ip].szs[fn][dir];

        dd.sz = sz;
        dd.m_sz = m_sz;
        dd.group = group;
    }

    d_sz get(const std::string& fn, const std::unordered_set<std::int32_t>& gr)
    {
        d_sz t;
        std::lock_guard<std::mutex> _{ lock };

        for (const auto& it : d)
        {
            auto f = it.second.szs.find(fn);
            if (f != it.second.szs.end())
            {
                for (const auto& it2 : f->second)
                {
                    if (gr.empty() || gr.count(it2.second.group) != 0)
                    {
                        t.sz += it2.second.sz;
                        t.m_sz += it2.second.m_sz;
                    }
                }
            }
        }

        return t;
    }

    std::unordered_map<std::uint32_t, data> get() const
    {
        std::lock_guard<std::mutex> _{ lock };
        return d;
    }

private:
    mutable std::mutex lock;
    std::unordered_map<std::uint32_t, data> d;
};

#endif // DISK_USAGE_H
