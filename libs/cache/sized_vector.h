#ifndef TTL_MAP_H
#define TTL_MAP_H

#include <string>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <mutex>
#include <type_traits>

#include <boost/noncopyable.hpp>

#include "../basefunc_std.h"

using namespace std::chrono;

namespace cache
{
    template<class key>
    class sized_vector : private boost::noncopyable
    {
    public:
        sized_vector(std::size_t sz)
        {
            for (auto i = 0u; i < sz; ++i)
            {
                _vec.push_back(key());
            }
        }

        void push_back(const key& v)
        {
            std::lock_guard<std::mutex> _{ _lock };
            _vec[_cursor] = v;
            ++_cursor;
            if (_cursor >= _vec.size()) _cursor = 0;
        }

        std::vector<key> get(std::function<bool(const key&)> pred)
        {
            std::vector<key> ret;
            std::lock_guard<std::mutex> _{ _lock };

            for (const auto& it : _vec)
            {
                if (pred(it)) ret.push_back(it);
            }

            return ret;
        }

    private:
        std::vector<key> _vec;
        std::mutex _lock;
        std::size_t _cursor = 0u;
    };
}

#endif // TTL_MAP_H
