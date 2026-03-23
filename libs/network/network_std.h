#ifndef NETWORK_STD_H
#define NETWORK_STD_H

#include <string>
#include <math.h>
#include "../basefunc_std.h"

class network_std
{
public:
    static std::uint32_t inet_aton(const std::string& ip, std::uint32_t added_mask = 0);
    static std::string inet_ntoa(const std::uint32_t number);
    static std::vector<std::string> ips_by_mask(const std::string& ip);

    template<class T>
    static std::vector<std::string> inet_ntoa(const T& numbers)
    {
        std::vector<std::string> ret;
        for (const auto& it : numbers)
        {
            ret.push_back(inet_ntoa(it));
        }
        return ret;
    }
};

#endif // NETWORK_STD_H
