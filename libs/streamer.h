#ifndef STREAMER_H
#define STREAMER_H

#include "bitbase.h"
#include "date_time.h"
#include "commpression_zlib.h"

#include <list>
#include <vector>
#include <set>
#include <unordered_set>
#include <deque>

#define STREAMER_WITH_JSON

#ifdef STREAMER_WITH_JSON
#include "json/json/json.h"
#endif

template<std::size_t BYTES_STR_SIZE = sizeof(std::size_t), typename std::enable_if<BYTES_STR_SIZE == 8 ||
                                                                                   BYTES_STR_SIZE == 7 ||
                                                                                   BYTES_STR_SIZE == 6 ||
                                                                                   BYTES_STR_SIZE == 5 ||
                                                                                   BYTES_STR_SIZE == 4 ||
                                                                                   BYTES_STR_SIZE == 3 ||
                                                                                   BYTES_STR_SIZE == 2 ||
                                                                                   BYTES_STR_SIZE == 1, bool>::type n = true>
class streamer
{
    template <typename Container>
    struct is_container : std::false_type { };

    template <typename Ts> struct is_container<std::list<Ts>> : std::true_type { };
    template <typename Ts> struct is_container<std::vector<Ts>> : std::true_type { };
    template <typename Ts> struct is_container<std::set<Ts>> : std::true_type { };
    template <typename Ts> struct is_container<std::unordered_set<Ts>> : std::true_type { };
    template <typename Ts> struct is_container<std::deque<Ts>> : std::true_type { };

    template <class T>
    struct is_streamer : std::false_type { };

    template <std::size_t b> struct is_streamer<streamer<b>> : std::true_type { };

public:
    enum class _get_type
    {
        clear,
        compress,
        string
    };

    struct _skip
    {
        enum type
        {
            none,
            _byte,
            _string,
            _date_time,
            _int16,
            _int32,
            _int64
        };

        static _skip::type get(const std::string& value)
        {
            static std::unordered_map<std::string, _skip::type> strings;
            if (strings.empty())
            {
                static std::mutex _lock;
                std::lock_guard<std::mutex> _{ _lock };

                if (strings.empty())
                {
#define INSERT_ELEMENT(p) strings[#p] = p
                    INSERT_ELEMENT(none);
                    INSERT_ELEMENT(_byte);
                    INSERT_ELEMENT(_string);
                    INSERT_ELEMENT(_date_time);
                    INSERT_ELEMENT(_int16);
                    INSERT_ELEMENT(_int32);
                    INSERT_ELEMENT(_int64);
#undef INSERT_ELEMENT
                }
            }

            auto f = strings.find(value);
            if (f != strings.end()) return f->second;

            return none;
        }
    };

    streamer() = default;
    streamer(const std::string& _data) : data(_data) { }
    streamer(std::string&& _data) : data(std::move(_data)) { }

    std::string get_pattern(const std::string& p) const
    {
        std::size_t _read_pos { read_pos };

        auto exp = basefunc_std::split(p, ',');
        std::int32_t sz { exp.size() };
        sz -= 1;
        for (auto i = 0; i < sz; ++i)
        {
            this->operator>>(_skip::get(exp[i]));
        }

        std::string ret;
        this->operator>>(ret);

        read_pos = _read_pos;

        return ret;
    }

    template<std::size_t b>
    void fetch_streamer(streamer<b>& s, _get_type tp = _get_type::compress) const
    {
        std::string f;
        this->operator>>(f);

        if (tp == _get_type::compress)
        {
            s.set(std::move(commpression_zlib::decompress_string(f)));
        }
        else
        {
            s.set(std::move(f));
        }
    }

    template<std::size_t b>
    const streamer& operator>>(streamer<b>& s) const
    {
        std::string f;
        this->operator>><sizeof(std::size_t)>(f);
        s.set(std::move(f));
        return *this;
    }

#ifdef STREAMER_WITH_JSON

    const streamer& operator>>(Json::Value& json) const
    {
        std::string f;
        this->operator>><sizeof(std::size_t)>(f);

        try
        {
            const char* begin = f.c_str();
            const char* end = begin + f.length();
            Json::Reader().parse(begin, end, json, false);
        }
        catch(std::exception& ex)
        {

        }

        return *this;
    }

    const streamer& operator<<(const Json::Value& json)
    {
        this->operator<<<sizeof(std::size_t)>(json.toString());
        return *this;
    }

#endif

    const streamer& operator>>(const typename streamer::_skip::type& sk) const
    {
        std::size_t sz { 0 };

        switch (sk)
        {
            case _skip::type::none: return *this;
            case _skip::type::_byte: sz = sizeof(char); break;
            case _skip::type::_int16: sz = sizeof(std::int16_t); break;
            case _skip::type::_int32: sz = sizeof(std::int32_t); break;
            case _skip::type::_int64: sz = sizeof(std::int64_t); break;
            case _skip::type::_string:
            {
                sz = sizeof(std::size_t);

                if (data.size() >= read_pos + sz)
                {
                    std::size_t sz_str { 0 };
                    bitbase::chars_to_numeric(data.substr(read_pos, sz), sz_str);
                    sz += sz_str;
                }
                else
                {
                    sz = 0;
                }

                break;
            }
            case _skip::type::_date_time: sz = sizeof(std::time_t); break;
            default: break;
        }

        if (data.size() >= read_pos + sz)
        {
            read_pos += sz;
        }

        return *this;
    }

    template<class T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
    const streamer& operator>>(T& v) const
    {
        auto sz = sizeof(T);

        if (data.size() >= read_pos + sz)
        {
            bitbase::chars_to_numeric(data.substr(read_pos, sz), v);
            read_pos += sz;
        }

        return *this;
    }

    template<class T, std::enable_if_t<is_container<T>::value, bool> = true>
    const streamer& operator>>(T& v) const
    {
        std::size_t s { 0 };
        auto sz = sizeof(sz);

        if (data.size() >= read_pos + sz)
        {
            bitbase::chars_to_numeric(data.substr(read_pos, sz), s);
            read_pos += sz;
        }

        if (s > 0)
        {
            if (data.size() >= read_pos + s)
            {
                bitbase::uncomress_list(data.substr(read_pos, s), v);
                read_pos += s;
            }
        }

        return *this;
    }

    template<class ...args>
    const streamer& operator>>(std::tuple<args...>& v) const
    {
        std::int32_t sz_bytes { static_cast<std::int32_t>(std::ceil(double(std::tuple_size<std::tuple<args...>>{} / 8.0))) };
        std::uint8_t bytes[sz_bytes];

        for (auto i = 0; i < sz_bytes; ++i)
        {
            this->operator>>(bytes[i]);
        }

        unpack_tuple(v, bytes, *this);

        return *this;
    }

    template<std::size_t b>
    const streamer& operator>>(std::vector<streamer<b>>& v) const
    {
        std::size_t sz { 0 };
        this->operator>>(sz);

        for (auto i = 0ul; i < sz; ++i)
        {
            std::string d;
            this->operator>><sizeof(std::size_t)>(d);

            streamer<b> str { std::move(d) };
            v.push_back(std::move(str));
        }

        return *this;
    }

    template<std::size_t bytes_str = BYTES_STR_SIZE>
    const streamer& operator>>(std::string& v) const
    {
        if (data.size() >= read_pos + bytes_str)
        {
            std::size_t sz_str { 0 };
            bitbase::chars_to_numeric(data.substr(read_pos, bytes_str), sz_str);

            if (data.size() >= read_pos + bytes_str + sz_str)
            {
                v = data.substr(read_pos + bytes_str, sz_str);

                read_pos += bytes_str;
                read_pos += sz_str;
            }
        }

        return *this;
    }

    const streamer& operator>>(char& v) const
    {
        if (data.size() >= read_pos + 1)
        {
            v = data[read_pos];
            read_pos += 1;
        }

        return *this;
    }

    const streamer& operator>>(std::float_t& v) const
    {
        auto sz = sizeof(std::uint32_t);

        if (data.size() >= read_pos + sz)
        {
            std::uint32_t vu { 0 };
            bitbase::chars_to_numeric(data.substr(read_pos, sz), vu);

            v = *((std::float_t*)&vu);

            read_pos += sz;
        }

        return *this;
    }

    const streamer& operator>>(date_time& v) const
    {
        auto sz = sizeof(std::time_t);

        if (data.size() >= read_pos + sz)
        {
            std::time_t vu { 0 };
            bitbase::chars_to_numeric(data.substr(read_pos, sz), vu);

            if (vu != 0) v = date_time::current_date_time(vu);

            read_pos += sz;
        }

        return *this;
    }

    const streamer& operator>>(std::double_t& v) const
    {
        auto sz1 = sizeof(dbl_packed::exp);
        auto sz2 = sizeof(dbl_packed::frac);

        if (data.size() >= read_pos + sz1 + sz2)
        {
            dbl_packed vu;

            bitbase::chars_to_numeric(data.substr(read_pos, sz1), vu.exp);
            read_pos += sz1;
            bitbase::chars_to_numeric(data.substr(read_pos, sz2), vu.frac);
            read_pos += sz2;

            v = unpack(&vu);
        }

        return *this;
    }

    template<std::size_t b>
    streamer& operator<<(const streamer<b>& v)
    {
        data += v.get(streamer<b>::_get_type::string);
        return *this;
    }

    template<std::size_t b>
    streamer& operator+=(const streamer<b>& v)
    {
        data += v.get();
        return *this;
    }

    template<class T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
    streamer& operator<<(const T& v)
    {
        data += bitbase::numeric_to_chars(v);
        return *this;
    }

    template<std::size_t bytes_str = BYTES_STR_SIZE>
    streamer& operator<<(const std::string& v)
    {
        std::size_t sz { v.size() };
        std::size_t max_sz { static_cast<std::size_t>(powl(256, bytes_str) - 1) };
        if (sz > max_sz) sz = max_sz;

        data += bitbase::numeric_to_chars(sz, bytes_str);
        data += v.substr(0, max_sz);

        return *this;
    }

    streamer& operator<<(char v)
    {
        data += v;

        return *this;
    }

    streamer& operator<<(const std::float_t& v)
    {
        data += bitbase::numeric_to_chars((*(std::uint32_t*)&v));
        return *this;
    }

    streamer& operator<<(const date_time& v)
    {
        std::time_t vu = v.get_time_from_epoch();
        data += bitbase::numeric_to_chars(vu);
        return *this;
    }

    streamer& operator<<(const std::double_t& v)
    {
        dbl_packed p;
        pack(v, &p);
        data += bitbase::numeric_to_chars(p.exp);
        data += bitbase::numeric_to_chars(p.frac);
        return *this;
    }

    template<class T, std::enable_if_t<is_container<T>::value, bool> = true>
    streamer& operator<<(const T& v)
    {
        std::string d = bitbase::comress_list(v);
        data += bitbase::numeric_to_chars(std::size_t(d.size()));
        data += d;
        return *this;
    }

    template<std::size_t b>
    streamer& operator<<(const std::vector<streamer<b>>& v)
    {
        data += bitbase::numeric_to_chars(std::size_t(v.size()));

        for (const streamer<b>& it : v)
        {
            data += it.get(streamer<b>::_get_type::string);
        }

        return *this;
    }

    template<class ...args>
    streamer& operator<<(const std::tuple<args...>& v)
    {
        std::uint8_t b { 0 };
        streamer data;

        pack_tuple(v, b, *this, data);

        this->operator+=(data);

        return *this;
    }

    std::string get(_get_type tp = _get_type::clear) const
    {
        if (tp != _get_type::clear)
        {
            std::string vv;
            if (tp == _get_type::compress)
            {
                vv = commpression_zlib::compress_string(data, 1);
            }

            const std::string& v = (tp == _get_type::compress) ? vv : data;
            std::size_t sz { v.size() };

            return bitbase::numeric_to_chars(sz) + v;
        }
        return data;
    }

    std::string to_string(std::size_t bytes_size = sizeof(std::size_t)) const
    {
        return bitbase::numeric_to_chars(data.size(), bytes_size) + data;
    }

    void fetch_string(std::string& v, std::size_t bytes_size = sizeof(std::size_t)) const
    {
        if (data.size() >= read_pos + bytes_size)
        {
            std::size_t sz_str { 0 };
            bitbase::chars_to_numeric(data.substr(read_pos, bytes_size), sz_str);

            if (data.size() >= read_pos + bytes_size + sz_str)
            {
                v = data.substr(read_pos + bytes_size, sz_str);

                read_pos += bytes_size;
                read_pos += sz_str;
            }
        }
    }

    std::string get_bytes(std::size_t bytes_size) const
    {
        if (data.size() >= read_pos + bytes_size)
        {
            std::string r { data.substr(read_pos, bytes_size) };
            read_pos += bytes_size;
            return r;
        }
    }

    void set(const std::string& _d)
    {
        data = _d;
        read_pos = 0;
    }

    void set(std::string&& _d)
    {
        data = std::move(_d);
        read_pos = 0;
    }

    void append(const std::string& d)
    {
        data += d;
    }

    void clear()
    {
        data.clear();
        read_pos = 0;
    }

    void reserve(std::size_t s)
    {
        data.reserve(s);
    }

    bool is_data_available(std::size_t bytes = 0) const
    {
        if (bytes != 0) return read_pos + bytes <= data.size();
        return read_pos < data.size();
    }

    std::size_t get_bytes_available() const
    {
        return data.size() - read_pos;
    }

    bool empty() const
    {
        return data.empty();
    }

    std::size_t size() const
    {
        return data.size();
    }

private:
    std::string data;
    mutable std::size_t read_pos = 0;

    #define FRAC_MAX 9223372036854775807LL /* 2**63 - 1 */

    struct dbl_packed
    {
        int exp;
        long long frac;
    };

    void pack(double x, dbl_packed *r)
    {
        double xf = fabs(frexp(x, &r->exp)) - 0.5;

        if (xf < 0.0)
        {
            r->frac = 0;
            return;
        }

        r->frac = 1 + (long long)(xf * 2.0 * (FRAC_MAX - 1));

        if (x < 0.0)
            r->frac = -r->frac;
    }

    double unpack(const dbl_packed *p) const
    {
        double xf, x;

        if (p->frac == 0)
            return 0.0;

        xf = ((double)(llabs(p->frac) - 1) / (FRAC_MAX - 1)) / 2.0;

        x = ldexp(xf + 0.5, p->exp);

        if (p->frac < 0)
            x = -x;

        return x;
    }

    template<class T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
    bool is_empty_tuple_item(const T& v)
    {
        return v == 0;
    }

    template<class T, std::enable_if_t<std::is_same<T, std::string>::value, bool> = true>
    bool is_empty_tuple_item(const T& v)
    {
        return v.empty();
    }

    template<class T, std::enable_if_t<is_streamer<T>::value, bool> = true>
    bool is_empty_tuple_item(const T& v)
    {
        return v.empty();
    }

    //

    /*template<class T, std::enable_if_t<std::is_same<T, streamer<8>>::value, bool> = true>
    bool is_empty_tuple_item(const T& v)
    {
        return v.empty();
    }*/

    template<class T, std::enable_if_t<std::is_same<T, date_time>::value, bool> = true>
    bool is_empty_tuple_item(const T& v)
    {
        return !v.is_valid_date() && !v.is_valid_time();
    }

#ifdef STREAMER_WITH_JSON
    template<class T, std::enable_if_t<std::is_same<T, Json::Value>::value, bool> = true>
    bool is_empty_tuple_item(const T& v)
    {
        return v.empty();
    }
#endif

    template<std::size_t I = 0, typename... Tp>
    inline typename std::enable_if<I == sizeof...(Tp), void>::type
    pack_tuple(const std::tuple<Tp...>& t, std::uint8_t& byte, streamer& bytes, streamer& data)
    {
        bytes << byte;
    }

    template<std::size_t I = 0, typename... Tp>
    inline typename std::enable_if<I < sizeof...(Tp), void>::type
    pack_tuple(const std::tuple<Tp...>& t, std::uint8_t& byte, streamer& bytes, streamer& data)
    {
        if (I != 0 && I % 8 == 0)
        {
            bytes << byte;
            byte = 0;
        }

        if (is_empty_tuple_item(std::get<I>(t)))
        {
            bitbase::setBit(byte, I % 8);
        }
        else
        {
            data << std::get<I>(t);
        }

        pack_tuple<I + 1, Tp...>(t, byte, bytes, data);
    }

    //----

    template<std::size_t I = 0, typename... Tp>
    inline typename std::enable_if<I == sizeof...(Tp), void>::type
    unpack_tuple(std::tuple<Tp...>& t, std::uint8_t bytes[], const streamer& data) const
    {

    }

    template<std::size_t I = 0, typename... Tp>
    inline typename std::enable_if<I < sizeof...(Tp), void>::type
    unpack_tuple(std::tuple<Tp...>& t, std::uint8_t bytes[], const streamer& data) const
    {
        if (!bitbase::isBitSetted(bytes[I / 8], I % 8))
        {
            data >> std::get<I>(t);
        }

        unpack_tuple<I + 1, Tp...>(t, bytes, data);
    }
};

#endif // STREAMER_H
