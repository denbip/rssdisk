#ifndef BASEFUNC_STD_H
#define BASEFUNC_STD_H

#define RESET   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */
#define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */

#include <stdarg.h>
#include <dirent.h>
#include <sys/stat.h>
#include <mutex>
#include <time.h>
#include <vector>
#include <sstream>
#include <iostream>
#include <thread>
#include <map>
#include <atomic>
#include <chrono>
#include <vector>
#include <algorithm>
#include <cstring>
#include <iomanip>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <dirent.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <thread>
#include <cctype>
#include <sstream>
#include <string>
#include <regex>
#include <random>
#include <sys/time.h>
#include <sys/resource.h>
#include <fstream>
#include <type_traits>
#include <set>

class basefunc_std
{
public:
    basefunc_std();

    /**
     * COUT
     */
    enum class COLOR
    {
        NONE,
        RED_COL,
        GREEN_COL,
        YELLOW_COL,
        CYAN_COL,
        BLUE_COL,
        MAGENTA_COL
    };

    enum class split_option
    {
        cut_empty_line,
        include_empty_line
    };

    enum class implode_option
    {
        skip_empty_line,
        include_empty_line
    };

    static void check_system(bool exit_ = false);

    static bool is_number(const std::string& s)
    {
        return !s.empty() && std::find_if(s.begin(),
            s.end(), [](unsigned char c) { return !std::isdigit(c); }) == s.end();
    }

    static std::string execCMD(const std::string& cmd);

    static void set_path_log(const std::string& p, bool start_thread_log_flush = true) noexcept;
    static std::string get_path_log() noexcept;
    static void stop_log_thread();

    /**
     * ФЛУШ ЛОГА НА ДИСК
     */
    static void log_flush();

    static void log(const std::string& text, std::string filename, bool flush_im = true, COLOR col = COLOR::NONE) noexcept;
    static void log(const std::string& txt, std::string header, std::string fileName, bool flush_im = true, COLOR col = COLOR::NONE) noexcept;
    static void replaceAll(std::string &s, const std::string &search, const std::string &replace) noexcept;
    static void replaceAll(std::string &s, const char &search, const char &replace) noexcept;
    static void removeAll(std::string &s, const char search) noexcept;

    static bool is_file(const std::string& path);
    static bool is_directory(const std::string& path);
    static std::string read_file(const std::string& path);

    static std::string generate_pass(int len, bool use_lower_case);
    static std::string generate_pass_wb(int len);
    static std::string generate_uuid_v4();

    inline static int rand(int min = 0, int max = std::numeric_limits<int>::max())
    {
        if (min >= max) return min;

        static thread_local std::random_device rd;
        static thread_local std::mt19937 generator(rd());

        std::uniform_int_distribution<int> d(min, max);
        return d(generator);
    }

    static bool write_file_to_disk(const std::string& file_name, const std::string& content)
    {
        try
        {
            std::fstream file(file_name, std::ios::out | std::ios::binary);
            if (!file) return false;
            file << content; //first two symbols - version of db
        }
        catch(std::exception& ex)
        {
            std::cout << ex.what() << std::endl;
            return false;
        }

        return true;
    }

    static bool write_file_to_disk_a(const std::string& file_name, const std::string& content)
    {
        try
        {
            std::fstream file(file_name, std::ios::app);
            if (!file) return false;
            file << content; //first two symbols - version of db
        }
        catch(std::exception& ex)
        {
            std::cout << ex.what() << std::endl;
            return false;
        }

        return true;
    }

    // string ends with suffix
    static inline bool ends_with(const std::string& str, const std::string& suffix)
    {
        return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
    }

    // string starts from preffix
    static inline bool starts_with(const std::string& str, const std::string& prefix)
    {
        return str.size() >= prefix.size() && 0 == str.compare(0, prefix.size(), prefix);
    }

    /**
     * СДЕЛАТЬ ВРЕМЯ
     */
    inline static std::string make_time_from_short(const short TIME_) noexcept
    {
        std::string tm_ = std::to_string(TIME_);
        while (tm_.size() < 4) tm_ = "0" + tm_;
        tm_ = tm_.insert(2, ":");
        return tm_;
    }

    template<typename T>
    static std::vector<T> slice(std::vector<T> const &v, size_t m, size_t n)
    {
        auto first = v.cbegin() + m;
        auto last = v.cbegin() + n + 1;

        std::vector<T> vec(first, last);
        return vec;
    }

    template<typename T>
    static std::unordered_set<T> split_n(const std::string &s, char delim, T default_val = 0)
    {
        std::unordered_set<T> ret;

        std::stringstream ss(s);
        std::string item;
        while (std::getline(ss, item, delim))
        {
            T n { default_val };
            if (split_n_p(item, n)) ret.emplace(n);
        }
        return ret;
    }

    /**
     * EXPLODE BY CHAR CUTTED EMPTY
     */
    static std::vector<std::string> split(const std::string &s, char delim)
    {
        std::vector<std::string> elems;
        split(s, delim, elems);
        return elems;
    }

    /**
     * JOIN COLLECTION TO STRING WITH DELIM
     */
    template <typename Iterator>
    inline static std::string join(Iterator begin, Iterator end, const std::string &separator) noexcept
    {
        std::stringstream ss; bool first = true;
        for (auto it = begin; it != end; ++it)
        {
            ss << (!first ? separator : "") << *it;
            first = false;
        }
        return ss.str();
    }

    /**
     * IMPLODE VECTOR TO STRING WITH DELIM
     */
    inline static std::string implode(const std::string &del, const std::vector<std::string> &array, implode_option skip_empty_path = implode_option::include_empty_line) noexcept
    {
        std::stringstream ss; bool first = true;
        for (auto it = array.begin(); it != array.end(); ++it)
        {
            if (skip_empty_path == implode_option::skip_empty_line && (*it).empty()) continue;
            else ss << (!first ? del : "") << *it;
            first = false;
        }
        return ss.str();
    }

    /**
     * EXPLODE BY STRING INCLUDED EMPTY (~20% slower than split by char)
     */
    static std::vector<std::string> split_by_string(const std::string& str, const std::string& delimiter, split_option skip_empty_path = split_option::cut_empty_line)
    {
        std::vector<std::string> strings;

        std::string::size_type pos = 0;
        std::string::size_type prev = 0;

        if (skip_empty_path == split_option::include_empty_line)
        {
            while ((pos = str.find(delimiter, prev)) != std::string::npos)
            {
                strings.push_back(str.substr(prev, pos - prev));
                prev = pos + delimiter.size();
            }

            // To get the last substring (or only, if delimiter is not found)
            strings.push_back(str.substr(prev));
        }
        else
        {
            while ((pos = str.find(delimiter, prev)) != std::string::npos)
            {
                std::string p = str.substr(prev, pos - prev);
                if (!p.empty()) strings.push_back(std::move(p));
                prev = pos + delimiter.size();
            }

            std::string p = str.substr(prev, pos - prev);

            // To get the last substring (or only, if delimiter is not found)
            if (!p.empty()) strings.push_back(str.substr(prev));
        }

        return strings;
    }

    /**
     * EXPLODE
     */
    static std::vector<std::string> split_regex(const std::string &s, const std::string& delim) noexcept;

    static std::ostream& cout(const std::string& text, const std::string& header = "", COLOR col = COLOR::GREEN_COL, bool flush = true) noexcept;

    /**
     * СУЩЕСТВЕТ ЛИ ФАЙЛ
     */
    static bool fileExists(const std::string& name);

    static void create_folder_recursive(const std::string& fileName, unsigned start_from, bool create_last = true);

    static void chmod_recursive(const std::string& fileName, const std::string &mode);

    static std::vector<std::string> read_files_in_folder(const std::string& folder);

    template<class T>
    static std::string get_string_from_set(const T& s, char j = ',')
    {
        std::string ret;

        for (const auto& it : s)
        {
            if (!ret.empty()) ret += j;
            ret += _get_string(it);
        }

        return ret;
    }

    template<class T>
    static std::string get_string_from_map_first(const T& s)
    {
        std::string ret;

        for (const auto& it : s)
        {
            if (!ret.empty()) ret += ",";
            ret += _get_string(it.first);
        }

        return ret;
    }

    template<class T, class T_val, template<class...> class T_MAP>
    static void get_map_from_string_delimeter(const std::string& str, T_MAP<T, T_val>& s, char delimeter = ';', char delimeter_pair = ':', bool clear = true)
    {
        if (clear) s.clear();

        auto exp = split(str, delimeter);
        for (const auto& it : exp)
        {
            auto exp2 = split(it, delimeter_pair);
            if (exp2.size() >= 2)
            {
                T key;
                basefunc_std::stoi(exp2[0], key);
                T_val val;
                basefunc_std::stoi(exp2[1], val);

                s.insert( { key, val } );
            }
        }
    }

    template<class T, template<class...> class T_MAP>
    static void get_set_from_string_enum(const std::string& str, T_MAP<T>& s, bool clear = true, typename std::enable_if<std::is_same<std::underlying_type_t<T>, std::int32_t>::value>::type* n = 0)
    {
        if (clear) s.clear();

        auto exp = split(str, ',');
        for (const auto& it : exp)
        {
            std::int32_t _t { 0 };
            if (basefunc_std::stoi(it, _t))
            {
                T _tt = static_cast<T>(_t);

                s.emplace(_tt);
            }
        }
    }

    template<class T, template<class...> class T_MAP>
    static void get_set_from_string(const std::string& str, T_MAP<T>& s, bool clear = true)
    {
        if (clear) s.clear();

        auto exp = split(str, ',');
        for (const auto& it : exp)
        {
            T _t { 0 };
            if (basefunc_std::stoi(it, _t))
            {
                s.emplace(_t);
            }
        }
    }

    template<class T, template<class...> class T_MAP>
    static void split_set(const std::string& str, T_MAP<T>& s, bool clear = true)
    {
        if (clear) s.clear();

        auto exp = split(str, ',');
        for (const auto& it : exp)
        {
            s.emplace(it);
        }
    }

    template<class T>
    static void get_vector_from_string(const std::string& str, std::vector<T>& s, bool clear = true)
    {
        if (clear) s.clear();

        auto exp = split(str, ',');
        for (const auto& it : exp)
        {
            T _t { 0 };
            if (basefunc_std::stoi(it, _t))
            {
                s.push_back(_t);
            }
        }
    }

    template<class t>
    static std::string _get_string(const t& _k, typename std::enable_if<std::is_same<std::string, t>::value>::type* n = 0)
    {
        return _k;
    }

    template<class t>
    static std::string _get_string(const t& _k, typename std::enable_if<std::is_arithmetic<t>::value>::type* n = 0)
    {
        return std::to_string(_k);
    }

    /**
     * СОЗДАТЬ ПАПКУ
     */
    static void createFolder(const char *path);

    static std::string uncompress_number(const std::string& k);
    static std::string compress_number(const std::string& k);

    static std::string format(const std::string fmt_str, ...);

    static void cout_mem_usage();

    static void process_mem_usage(long& vm_usage, long& resident_set);

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

    static inline bool parse_bool(std::string str)
    {
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        std::istringstream is(str);
        bool b;
        is >> std::boolalpha >> b;
        return b;
    }

    // trim from start
    static inline std::string &ltrim(std::string &s)
    {
         s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
         return s;
    }

    // trim from end
    static inline std::string &rtrim(std::string &s)
    {
         s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
         return s;
    }

    // trim from both ends
    static inline std::string &trim(std::string &s)
    {
         return ltrim(rtrim(s));
    }

    // remove all specific symbols like \n, \r ...
    static inline void remove_all_specific_symbols(std::string &s)
    {
         s.erase(std::remove(s.begin(), s.end(), '\n'), s.cend());
         s.erase(std::remove(s.begin(), s.end(), '\r'), s.cend());
    }

    /**
     * УРЛ ДЕКОДЕ
     */
    static std::string url_decode(const std::string &SRC) noexcept;
    static std::string url_encode(const std::string& value);

    /**
     * TO_STRING DOUBLE
     */
    template<class T>
    static inline std::string to_string_double(T number, int precision = 14) noexcept
    {
        std::ostringstream strs;
        strs << std::setprecision(precision) << std::fixed << number;
        return strs.str();
    }


    /**
     * REPLACE \ TO / AND " TO EMPTY
     */
    static inline void replaceSlashes(std::string& str)
    {
        replaceAll(str, "\"", "");
        replaceAll(str, "\\", "/");
    }

    /**
     * ОКРУГЛИТЬ double
     */
    static inline double round_double(double num, int precision) noexcept
    {
        return floorf(num * pow(10.0f, precision) + .5f) / pow(10.0f, precision);
    }

    static int read_settings(std::string fileName, std::string sect_name, std::string parm_name, std::string& save_buffer, bool exit_ = false, bool cout_ = true);
    static void set_timer_to_kill(std::int32_t sec_to_wait);
    static bool killbrothers(const int flag);
    static void kill_check(int argc, char *argv[]);
    static void readname(char *name, pid_t pid);

    template<class t>
    static bool stoi(const std::string& str_, t& l_, typename std::enable_if<std::is_same<unsigned long, t>::value>::type* = 0)
    {
        try
        {
            l_ = std::stoul(str_);
        }
        catch(...) { return false; }
        return true;
    }


    template<class t>
    static bool stoi(const std::string& str_, t& l_, typename std::enable_if<std::is_same<long, t>::value>::type* = 0)
    {
        try
        {
            l_ = std::stol(str_);
        }
        catch(...) { return false; }
        return true;
    }

    template<class t>
    static bool stoi(const std::string& str_, t& l_, typename std::enable_if<std::is_same<short, t>::value>::type* = 0)
    {
        try
        {
            l_ = std::stoi(str_);
        }
        catch(...) { return false; }
        return true;
    }

    template<class t>
    static bool stoi(const std::string& str_, t& l_, typename std::enable_if<std::is_same<unsigned, t>::value>::type* = 0)
    {
        try
        {
            l_ = std::stoi(str_);
        }
        catch(...) { return false; }
        return true;
    }

    template<class t>
    static bool stoi(const std::string& str_, t& l_, typename std::enable_if<std::is_same<int, t>::value>::type* = 0)
    {
        try
        {
            l_ = std::stoi(str_);
        }
        catch(...) { return false; }
        return true;
    }

    template<class t>
    static bool stoi(const std::string& str_, t& l_, typename std::enable_if<std::is_same<bool, t>::value>::type* = 0)
    {
        try
        {
            l_ = std::stoi(str_) >= 1;
        }
        catch(...) { return false; }
        return true;
    }

    template<class t>
    static bool stoi(const std::string& str_, t& l_, typename std::enable_if<std::is_same<double, t>::value>::type* = 0)
    {
        try
        {
            l_ = std::stod(str_);
        }
        catch(...) { return false; }
        return true;
    }

    static inline bool stod(const std::string& str_, double& l_)
    {
        try
        {
            l_ = std::stod(str_);
        }
        catch(...) { return false; }
        return true;
    }

    static inline bool stof(const std::string& str_, float& l_)
    {
        try
        {
            l_ = std::stof(str_);
        }
        catch(...) { return false; }
        return true;
    }

    /**
     * РАССТОЯНИЕ МЕЖДУ ТОЧКАМИ В МЕТРАХ
     */
    static int distance_map(double a1, double b1, double a2, double b2);

    /**
     * RELEASE MAP MEMORY
     */
    template <class Key, class Value, class T_hash = std::hash<Key>>
    static inline void clear_map(std::unordered_map<Key, Value, T_hash>& map)
    {
        std::unordered_map<Key, Value, T_hash> temp(map.begin(), map.end(), 0);
        std::swap(temp, map);
    }

    template <class Key, class Value>
    static inline void clear_map(std::map<Key, Value>& map)
    {

    }

    template <class T>
    static void clear_container(T& c)
    {
        c.clear();
        T n;
        std::swap(c, n);
    }

    template <class Value>
    static inline void clear_set(std::unordered_set<Value>& map)
    {
        std::unordered_set<Value> temp(map.begin(), map.end(), 0);
        std::swap(temp, map);
    }

    static std::wstring utf8_to_utf16(const std::string& utf8);

    template <class Key, class Value>
    inline static unsigned long get_map_size(const std::map<Key,Value> &map) noexcept
    {
        unsigned long size = sizeof(map);
        for(typename std::map<Key,Value>::const_iterator it = map.begin(); it != map.end(); ++it){
            size += it->first.size();
            size += it->second.size();
        }
        return size;
    }

    template <class Key, class Value>
    inline static unsigned long get_map_capacity(const std::map<Key,Value> &map) noexcept
    {
        unsigned long cap = sizeof(map);
        for(typename std::map<Key,Value>::const_iterator it = map.begin(); it != map.end(); ++it){
            cap += it->first.capacity();
            cap += it->second.capacity();
        }
        return cap;
    }
    
    template<class Map, class Key>
    static typename Map::const_iterator greatest_less(const Map& m, const Key& k)
    {
        if (!m.empty())
        {
            typename Map::const_iterator it = m.lower_bound(k);
            if(it != m.begin())
            {
                return --it;
            }
        }
        return m.end();
    }

    template<class Map, class Key>
    static typename Map::mapped_type greatest_less_value(const Map& m, const Key& k)
    {
        if (!m.empty())
        {
            typename Map::const_iterator it = m.lower_bound(k);
            if(it != m.begin())
            {
                --it;
                return it->second;
            }
        }
        return 0;
    }

    template<class Map, class Key>
    static typename Map::const_iterator great_equal_less(const Map& m, const Key& k)
    {
        if (!m.empty())
        {
            typename Map::const_iterator it = m.lower_bound(k);
            if(it != m.begin())
            {
                if (it->first == k) return it;
                return --it;
            }
            else if (it->first == k) return it;
        }

        return m.end();
    }

private:
    static std::mutex mutex_log;
    static std::string db_path_log;
    static std::thread thread_log_flush;

    static std::atomic<bool> is_log_thread_started;
    static std::map<std::string, std::vector<std::string> > map_log;
    static std::vector<std::string> created_patheth;

    static std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems)
    {
        std::stringstream ss(s);
        std::string item;
        while (std::getline(ss, item, delim))
        {
            elems.push_back(item);
        }
        return elems;
    }

    inline static bool split_n_p(const std::string& s, std::int16_t& i) { return basefunc_std::stoi(s, i); }

    inline static bool split_n_p(const std::string& s, std::int32_t& i) { return basefunc_std::stoi(s, i); }

    inline static bool split_n_p(const std::string& s, std::int64_t& i) { return basefunc_std::stoi(s, i); }

    template < typename C, C beginVal, C endVal>
    class enum_iterator {
      typedef typename std::underlying_type<C>::type val_t;
      int val;
    public:
      enum_iterator(const C & f) : val(static_cast<val_t>(f)) {}
      enum_iterator() : val(static_cast<val_t>(beginVal)) {}
      enum_iterator operator++() {
        ++val;
        return *this;
      }
      C operator*() { return static_cast<C>(val); }
      enum_iterator begin() { return *this; } //default ctor is good
      enum_iterator end() {
          static const enum_iterator endIter=++enum_iterator(endVal); // cache it
          return endIter;
      }
      bool operator!=(const enum_iterator& i) { return val != i.val; }
    };
};

#endif // BASEFUNC_STD_H
