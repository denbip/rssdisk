#ifndef BITBASE_H
#define BITBASE_H

#include <string>
#include <unordered_set>

class bitbase
{
public:
    bitbase() = default;

    template<class T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
    static std::int16_t max_bits_used(T n, int sz = sizeof(T))
    {
        std::int16_t ret { 0 };

        for (int i = sz * 8 - 1; i >= 0; --i)
        {
            if (isBitSetted(n, i))
            {
                ret = i + 1;
                break;
            }
        }

        return ret;
    }

    template<class T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
    static void numeric_to_chars(std::string& app, T n, int sz = sizeof(T))
    {
        int s = (sz - 1) * 8;
        for (; s >= 0; s -= 8)
        {
            app += u_char((n >> s) & 0xff);
        }
    }

    template<class T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
    static std::string numeric_to_chars(T n, int sz = sizeof(T))
    {
        std::string ret;
        int s = (sz - 1) * 8;
        for (; s >= 0; s -= 8)
        {
            ret += u_char((n >> s) & 0xff);
        }
        return ret;
    }

    template<class T, typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
    static void chars_to_numeric(const std::string& t, T& n)
    {
        n = 0;
        int s = t.size();
        int j { 0 };
        for (int i = s - 1; i >= 0; --i, ++j)
        {
            u_char c { static_cast<u_char>(t[i]) };
            n += T(c) << (8 * j);
        }
    }

    /**
     * UNSET BIT
     */
    template<class T>
    static inline void unsetBit(T& num, const int bit) noexcept
    {
        num &= ~(T(1) << bit);
    }

    /**
     * SET BIT
     */
    template<class T>
    static inline void setBit(T& num, const int bit) noexcept
    {
        num |= (T(1) << bit);
    }

    /**
     * IS BIT SETTED
     */
    template<class T>
    static inline bool isBitSetted(const T num, const int bit) noexcept
    {
        return (num & ((T)1 << bit)) != 0;
    }

    template<class T>
    static inline bool isBitSettedByNumber(const T num, const T bit_num) noexcept
    {
        return (num & bit_num) != 0;
    }

    template<class T>
    static std::unordered_set<T> uncomress_list(const std::string& _data)
    {
        const size_t size_bytes = sizeof(T);

        std::unordered_set<T> ret;
        if (_data.size() >= size_bytes)
        {
            auto sz { _data.size() - size_bytes };
            for (auto i = 0u; i <= sz; i += size_bytes)
            {
                T r { 0 };
                chars_to_numeric(_data.substr(i, size_bytes), r);
                ret.emplace(r);
            }
        }

        return ret;
    }

    template<class T>
    static void uncomress_list(const std::string& _data, T& ret)
    {
        using type_val = typename T::value_type;

        const size_t size_bytes = sizeof(type_val);

        if (_data.size() >= size_bytes)
        {
            auto sz { _data.size() - size_bytes };
            for (auto i = 0u; i <= sz; i += size_bytes)
            {
                type_val r { 0 };
                chars_to_numeric(_data.substr(i, size_bytes), r);
                ret.emplace(r);
            }
        }
    }

    template<class T>
    static std::string comress_list(const T& _data)
    {
        std::string ret;

        for (const auto& it : _data)
        {
            ret += numeric_to_chars(it);
        }

        return ret;
    }
};

#endif // BITBASE_H
