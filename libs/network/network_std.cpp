#include "network_std.h"

std::uint32_t network_std::inet_aton(const std::string& ip, uint32_t added_mask)
{
    std::vector<std::string> exp = basefunc_std::split(ip, '.');
    if (exp.size() != 4) return 0;

    std::uint32_t b3 { 0 };
    std::uint32_t b2 { 0 };
    std::uint32_t b1 { 0 };
    std::uint32_t b0 { 0 };

    basefunc_std::stoi(exp[0], b3);
    basefunc_std::stoi(exp[1], b2);
    basefunc_std::stoi(exp[2], b1);
    basefunc_std::stoi(exp[3], b0);

    return b3 * std::pow(256, 3) + b2 * std::pow(256, 2) + b1 * std::pow(256, 1) + b0 + added_mask;
}

std::string network_std::inet_ntoa(const std::uint32_t number)
{
    std::string ret;
    ret.reserve(15);
    for (int i = 24; i >= 0; i -= 8)
    {
        std::uint32_t n = (number >> i) & 0xFF;
        ret += std::to_string(n);
        if (i != 0) ret += '.';
    }
    return ret;
}

std::vector<std::string> network_std::ips_by_mask(const std::string& ip)
{
    std::vector<std::string> ret;

    auto f = ip.find("/");
    if (f != std::string::npos)
    {
        auto addr = network_std::inet_aton(ip.substr(0, f));

        std::int32_t mask { 0 };
        basefunc_std::stoi(ip.substr(f + 1), mask);
        if (mask < 0) mask = 0;
        if (mask > 32) mask = 32;

        std::int32_t bits { 1 << (32 - mask) };
        for (auto i = 0u; i < bits; ++i)
        {
            ret.push_back(network_std::inet_ntoa(addr + i));
        }
    }
    else
    {
        ret.push_back(ip);
    }

    return ret;
}
