#ifndef DATE_TIME_H
#define DATE_TIME_H

#include <cstdio>
#include <string>
#include <cmath>
#include <ctime>
#include "basefunc_std.h"

class date_time
{
public:
    enum class date_time_sel
    {
        date_only,
        time_only,
        date_and_time,
        none
    };

    class time_span
    {
    public:
        time_span() : total_secs(0) { }

        long total_secs;
        long total_days;
    };

    class date__
    {
    public:
        date__() : year(0), month(0), day(0), day_of_year(0), is_valid_date_(false) { }
        date__(const date__& copy) : year(copy.year), month(copy.month), day(copy.day), day_of_year(copy.day_of_year), is_valid_date_(copy.is_valid_date_) { }
        date__(date__&& copy) : year(copy.year), month(copy.month), day(copy.day), day_of_year(copy.day_of_year), is_valid_date_(copy.is_valid_date_) { }
        date__& operator=(const date__& other) // copy assignment
        {
            year = other.year;
            month = other.month;
            day = other.day;
            is_valid_date_ = other.is_valid_date_;
            day_of_year = other.day_of_year;
            return *this;
        }
        date__& operator=(date__&& other) // move assignment
        {
            year = other.year;
            month = other.month;
            day = other.day;
            is_valid_date_ = other.is_valid_date_;
            day_of_year = other.day_of_year;
            return *this;
        }
        inline bool operator==(const date__& other) const noexcept
        {
            return year == other.year && month == other.month && day == other.day;
        }
        inline bool operator!=(const date__& other) const noexcept
        {
            return !(*this == other);
        }
        inline bool operator<(const date__& other) const noexcept
        {
            return year < other.year || (year == other.year && month < other.month) || (year == other.year && month == other.month && day < other.day);
        }
        inline bool operator>(const date__& other) const noexcept
        {
            return !(*this < other) && !(*this == other);
        }
        inline bool operator<=(const date__& other) const noexcept
        {
            return !(*this > other);
        }
        inline bool operator>=(const date__& other) const noexcept
        {
            return !(*this < other);
        }

        void reset()
        {
            year = 0;
            month = 0;
            day = 0;
            day_of_year = 0;
            is_valid_date_ = false;
        }

        int year;
        int month;
        int day;

        int day_of_year;

        bool is_valid_date_;
    } date_;

    class time__
    {
    public:
        time__() : hour(0), minute(0), second(0), thousend_second(0), is_valid_time_(false) { }
        time__(const time__& copy) : hour(copy.hour), minute(copy.minute), second(copy.second), thousend_second(copy.thousend_second), is_valid_time_(copy.is_valid_time_) { }
        time__(time__&& copy) : hour(copy.hour), minute(copy.minute), second(copy.second), thousend_second(copy.thousend_second), is_valid_time_(copy.is_valid_time_) { }
        time__& operator=(const time__& other) // copy assignment
        {
            hour = other.hour;
            minute = other.minute;
            second = other.second;
            thousend_second = other.thousend_second;
            is_valid_time_ = other.is_valid_time_;
            return *this;
        }
        time__& operator=(time__&& other) // move assignment
        {
            hour = other.hour;
            minute = other.minute;
            second = other.second;
            thousend_second = other.thousend_second;
            is_valid_time_ = other.is_valid_time_;
            return *this;
        }
        inline bool operator==(const time__& other) const noexcept
        {
            return hour == other.hour && minute == other.minute && second == other.second;
        }
        inline bool operator!=(const time__& other) const noexcept
        {
            return !(*this == other);
        }
        inline bool operator<(const time__& other) const noexcept
        {
            return hour < other.hour || (hour == other.hour && minute < other.minute) ||
                  (hour == other.hour && minute == other.minute && second < other.second) ||
                  (hour == other.hour && minute == other.minute && second == other.second && thousend_second < other.thousend_second);
        }
        inline bool operator>(const time__& other) const noexcept
        {
            return !(*this < other) && !(*this == other);
        }
        inline bool operator<=(const time__& other) const noexcept
        {
            return !(*this > other);
        }
        inline bool operator>=(const time__& other) const noexcept
        {
            return !(*this < other);
        }

        void reset()
        {
            hour = 0;
            minute = 0;
            second = 0;
            thousend_second = 0;
            is_valid_time_ = false;
        }

        inline unsigned get_total_seconds()
        {
           return hour * 3600 + minute * 60 + second;
        }


        int hour;
        int minute;
        int second;
        int thousend_second;

        bool is_valid_time_;
    } time_;

    date_time()
    {

    }

    date_time(const std::string& str_, std::string format = "yyyy-MM-dd HH:mm:ss", date_time_sel sel_ = date_time_sel::date_and_time)
    {
        if (format.size() > str_.size())
        {
            format = format.substr(0, str_.size());
        }

        parse_date_time(str_, format, sel_);
    }

    date_time(const char* str_, std::string format = "yyyy-MM-dd HH:mm:ss", date_time_sel sel_ = date_time_sel::date_and_time)
    {
        if (format.size() > strlen(str_))
        {
            format = format.substr(0, strlen(str_));
        }

        parse_date_time(str_, format, sel_);
    }

    // copy assignment
    date_time(const date_time& copy, std::int32_t days_added = 0) noexcept : date_(copy.date_), time_(copy.time_)
    {
        if (days_added != 0) add_days(days_added);
    }

    // move assignment
    date_time(date_time&& copy, std::int32_t days_added = 0) noexcept : date_(copy.date_), time_(copy.time_)
    {
        if (days_added != 0) add_days(days_added);
    }

    date_time& operator=(const date_time& other) noexcept // copy assignment
    {
        date_ = other.date_;
        time_ = other.time_;
        return *this;
    }

    date_time& operator=(date_time&& other) noexcept // move assignment
    {
        date_ = other.date_;
        time_ = other.time_;
        return *this;
    }

    inline bool operator==(const date_time& other) const noexcept
    {
        return date_ == other.date_ && time_ == other.time_;
    }

    inline bool operator!=(const date_time& other) const noexcept
    {
        return !(*this == other);
    }

    inline bool operator<(const date_time& other) const noexcept
    {
        return date_ < other.date_ || (date_ == other.date_ && time_ < other.time_);
    }
    inline bool operator>(const date_time& other) const noexcept
    {
        return !(*this < other) && !(*this == other);
    }

    inline bool operator<=(const date_time& other) const noexcept
    {
        return !(*this > other);
    }
    inline bool operator>=(const date_time& other) const noexcept
    {
        return !(*this < other);
    }

    inline bool is_date_equels(const date_time& other) const noexcept
    {
        return date_ == other.date_;
    }

    inline bool is_time_equels(const date_time& other) const noexcept
    {
        return time_ == other.time_;
    }

    time_span operator-(const date_time& other) const
    {
        time_span sp;

        time_t raw;
        time(&raw);

        struct tm t1 = *gmtime(&raw), t2 = t1;

        t1.tm_year = date_.year - 1900;
        t1.tm_mon = date_.month - 1;
        t1.tm_mday = date_.day;
        t1.tm_hour = time_.hour;
        t1.tm_min = time_.minute;
        t1.tm_sec = time_.second;

        t2.tm_year = other.date_.year - 1900;
        t2.tm_mon = other.date_.month - 1;
        t2.tm_mday = other.date_.day;
        t2.tm_hour = other.time_.hour;
        t2.tm_min = other.time_.minute;
        t2.tm_sec = other.time_.second;

        time_t tt1, tt2;
        tt1 = mktime(&t1);
        tt2 = mktime(&t2);

        sp.total_secs = tt1 - tt2;
        if (sp.total_secs > 0) sp.total_days = (int)floor((double)sp.total_secs / (double)86400);
        else sp.total_days = (int)ceil((double)sp.total_secs / (double)86400);

        //basefunc_std::cout("From " + get_date_time() + " to " + other.get_date_time() + " secs " + std::to_string(sp.total_secs));

        return sp;
    }

    long secs_to(const date_time& other) const
    {
        return (other - *this).total_secs;
    }

    long days_to(const date_time& other) const
    {
        return (other - *this).total_days;
    }

    const date__& get_date_() const
    {
        return this->date_;
    }

    std::string get_miliseconds_from_epoch() const
    {
        return std::to_string(get_time_from_epoch()) + "000";
    }

    time_t get_time_from_epoch() const
    {
        if (!is_valid_date()) return 0;

        struct tm now;
        memset(&now, 0, sizeof(tm));

        //time_t t = time(0);
        //localtime_r(&t, &now);

        now.tm_year = date_.year - 1900;
        now.tm_mon = date_.month - 1;
        now.tm_mday = date_.day;

        now.tm_hour = time_.hour;
        now.tm_min = time_.minute;
        now.tm_sec = time_.second;

        return std::mktime(&now); //thread-safe? mktime local time (tm) to utc
    }

    static date_time current_date_time(time_t t_ = 0)
    {
        date_time dt;

        time_t t = t_ == 0 ? time(0) : t_; //utc time
        struct tm now;
        localtime_r(&t, &now); //convert to local time with gmt offset

        dt.date_.year = now.tm_year + 1900;
        dt.date_.month = now.tm_mon + 1;
        dt.date_.day = now.tm_mday;
        dt.date_.day_of_year = now.tm_yday;

        dt.time_.hour = now.tm_hour;
        dt.time_.minute = now.tm_min;
        dt.time_.second = now.tm_sec;

        dt.date_.is_valid_date_ = true;
        dt.time_.is_valid_time_ = true;

        return dt;
    }

    static date_time current_utc_date_time(time_t t_ = 0)
    {
        date_time dt;

        time_t t = t_ == 0 ? time(0) : t_; //utc time
        struct tm now;
        gmtime_r(&t, &now); //convert to local time with gmt offset

        dt.date_.year = now.tm_year + 1900;
        dt.date_.month = now.tm_mon + 1;
        dt.date_.day = now.tm_mday;
        dt.date_.day_of_year = now.tm_yday;

        dt.time_.hour = now.tm_hour;
        dt.time_.minute = now.tm_min;
        dt.time_.second = now.tm_sec;

        dt.date_.is_valid_date_ = true;
        dt.time_.is_valid_time_ = true;

        return dt;
    }

    inline bool is_valid_date() const noexcept
    {
        return date_.is_valid_date_;
    }

    inline bool is_valid_time() const noexcept
    {
        return time_.is_valid_time_;
    }

    inline void set_date_valid(bool valid = true)
    {
        date_.is_valid_date_ = valid;
    }

    inline void set_time_valid(bool valid = true)
    {
        time_.is_valid_time_ = valid;
    }

    void set_date(unsigned year_, unsigned month_, unsigned day_)
    {
        date_.year = year_;
        date_.month =month_;
        date_.day = day_;

        incr_all(date_time_sel::date_only); //SET VALID
    }

    void set_time(unsigned hour_, unsigned minute_, unsigned second_, unsigned thousend_second_ = 0)
    {
        time_.hour = hour_;
        time_.minute = minute_;
        time_.second = second_;
        time_.thousend_second = thousend_second_;

        incr_all(date_time_sel::time_only); //SET VALID
    }

    bool set_time_short(const short time__)
    {
        if (time__ > 2359 || time__ < 0) return false;
        int new_min = time__ % 100;
        if (new_min > 59) return false;
        time_.minute = new_min;
        time_.hour = (time__ - new_min) / 100;
        time_.second = 0;
        time_.thousend_second = 0;
        time_.is_valid_time_ = true;
        return true;
    }

    void set_date_time(unsigned year_, unsigned month_, unsigned day_, unsigned hour_, unsigned minute_, unsigned second_, unsigned thousend_second_ = 0)
    {
        date_.year = year_;
        date_.month =month_;
        date_.day = day_;

        time_.hour = hour_;
        time_.minute = minute_;
        time_.second = second_;
        time_.thousend_second = thousend_second_;

        incr_all(date_time_sel::date_and_time); //SET VALID
    }

    std::int32_t get_seconds_of_day() const
    {
        return time_.hour * 3600 + time_.minute * 60 + time_.second;
    }

    bool parse_date_time(const std::string& dt_, std::string format = "yyyy-MM-dd HH:mm:ss", date_time_sel sel_ = date_time_sel::date_and_time) //yyyy-MM-dd HH:mm:ss.t - full format
    {
        if (dt_.size() != format.size())
        {
            if (dt_.size() == 10)
            {
                format = "yyyy-MM-dd";
                sel_ = date_time_sel::date_only;
            }
            else if (dt_.size() == 8)
            {
                format = "HH:mm:ss";
                sel_ = date_time_sel::time_only;
            }
        }

        int* arr[7];

        bool parsed[7] { false, false, false, false, false, false, false };
        int sz = format.size();
        std::string scan_format = "";
        int queue = 0;
        for (int i = 0; i < sz; ++i)
        {
            switch (format.at(i))
            {
            case 'y':
                if (parsed[0]) continue;
                arr[queue] = &date_.year;
                scan_format += "%d";
                parsed[0] = true;
                ++queue;
                break;
            case 'M':
                if (parsed[1]) continue;
                arr[queue] = &date_.month;
                scan_format += "%d";
                parsed[1] = true;
                ++queue;
                break;
            case 'd':
                if (parsed[2]) continue;
                arr[queue] = &date_.day;
                scan_format += "%d";
                parsed[2] = true;
                ++queue;
                break;
            case 'h':
            case 'H':
                if (parsed[3]) continue;
                arr[queue] = &time_.hour;
                scan_format += "%d";
                parsed[3] = true;
                ++queue;
                break;
            case 'm':
                if (parsed[4]) continue;
                arr[queue] = &time_.minute;
                scan_format += "%d";
                parsed[4] = true;
                ++queue;
                break;
            case 's':
                if (parsed[5]) continue;
                arr[queue] = &time_.second;
                scan_format += "%d";
                parsed[5] = true;
                ++queue;
                break;
            case 't':
                if (parsed[6]) continue;
                arr[queue] = &time_.thousend_second;
                scan_format += "%d";
                parsed[6] = true;
                ++queue;
                break;
            case '/':
            case '-':
            case '\\':
            case ' ':
            case ',':
            case ':':
            case ';':
            case '.':
                scan_format += format.at(i);
                break;
            }
        }

        //basefunc_std::cout(dt_ + " " + scan_format + " " + std::to_string(queue));

        int r = -1;

        switch(queue)
        {
        case 1:
            r = std::sscanf(dt_.c_str(), scan_format.c_str(), arr[0]); break;
        case 2:
            r = std::sscanf(dt_.c_str(), scan_format.c_str(), arr[0], arr[1]); break;
        case 3:
            r = std::sscanf(dt_.c_str(), scan_format.c_str(), arr[0], arr[1], arr[2]); break;
        case 4:
            r = std::sscanf(dt_.c_str(), scan_format.c_str(), arr[0], arr[1], arr[2], arr[3]); break;
        case 5:
            r = std::sscanf(dt_.c_str(), scan_format.c_str(), arr[0], arr[1], arr[2], arr[3], arr[4]); break;
        case 6:
            r = std::sscanf(dt_.c_str(), scan_format.c_str(), arr[0], arr[1], arr[2], arr[3], arr[4], arr[5]); break;
        case 7:
            r = std::sscanf(dt_.c_str(), scan_format.c_str(), arr[0], arr[1], arr[2], arr[3], arr[4], arr[5], arr[6]); break;
        }

        if (r == queue)
        {
            incr_all(sel_); //SET VALID
            return true;
        }
        else
        {
            //basefunc_std::log("input " + dt_ + " scan_format " + scan_format + " queue " + std::to_string(queue), "class_date_time");
            return false;
        }
    }

    std::string get_date(const std::string& delimeter = "-") const
    {
        std::string ret = std::to_string(date_.year);
        ret += delimeter;
        if (date_.month < 10) ret += "0";
        ret += std::to_string(date_.month);
        ret += delimeter;
        if (date_.day < 10) ret += "0";
        ret += std::to_string(date_.day);

        return ret;
    }

    std::string get_date2(const std::string& delimeter = ".") const
    {
        std::string ret;
        if (date_.day < 10) ret += "0";
        ret += std::to_string(date_.day);
        ret += delimeter;
        if (date_.month < 10) ret += "0";
        ret += std::to_string(date_.month);
        ret += delimeter;
        ret += std::to_string(date_.year);

        return ret;
    }

    std::string get_time_hh_mm(const std::string& delimeter = ":") const
    {
        std::string ret = "";
        if (time_.hour < 10) ret += "0";
        ret += std::to_string(time_.hour);
        ret += delimeter;
        if (time_.minute < 10) ret += "0";
        ret += std::to_string(time_.minute);

        return ret;
    }

    std::string get_time(const std::string& delimeter = ":", bool thousend_sec = false) const
    {
        std::string ret = "";
        if (time_.hour < 10) ret += "0";
        ret += std::to_string(time_.hour);
        ret += delimeter;
        if (time_.minute < 10) ret += "0";
        ret += std::to_string(time_.minute);
        ret += delimeter;
        if (time_.second < 10) ret += "0";
        ret += std::to_string(time_.second);

        if (thousend_sec)
        {
            ret += ".";
            if (time_.thousend_second < 1000) ret += "0";
            if (time_.thousend_second < 100) ret += "0";
            if (time_.thousend_second < 10) ret += "0";
            ret += std::to_string(time_.thousend_second);
        }

        return ret;
    }

    inline short get_time_short() const
    {
        return time_.hour * 100 + time_.minute;
    }

    inline std::string get_time_short_str() const
    {
        return std::to_string(time_.hour * 100 + time_.minute);
    }

    inline std::string get_date_time() const
    {
        return get_date() + " " + get_time();
    }

    inline std::string get_date_time_iso_8601(std::int8_t tm_offset = 0) const
    {
        std::string tm { std::to_string(tm_offset) };
        if (tm.size() == 1) tm = "0" + tm;

        return get_date() + "T" + get_time() + "+" + tm + ":00";
    }

    static int count_days_in_month(int month_, int year_)
    {
        switch (month_)
        {
            case 4:
            case 6:
            case 9:
            case 11:
            {
                return 30;
            }
            case 2:
            {
                bool isLeapYear = (year_ % 4 == 0 && year_ % 100 != 0) || (year_ % 400 == 0);

                if (isLeapYear) return 29;
                else return 28;
            }
        }

        return 31;
    }

    date_time& add_msecs(int ms_)
    {
        time_.thousend_second += ms_;
        incr_all();
        return *this;
    }

    date_time& add_secs(int sec_)
    {
        time_.second += sec_;
        incr_all();
        return *this;
    }

    date_time& add_mins(int min_)
    {
        time_.minute += min_;
        incr_all();
        return *this;
    }

    date_time& add_hours(int hour_)
    {
        time_.hour += hour_;
        incr_all();
        return *this;
    }

    date_time& add_days(int day_)
    {
        date_.day += day_;
        incr_all();
        return *this;
    }

    date_time& add_month(int month_)
    {
        date_.month += month_;
        if (month_ > 0)
        {
            while (date_.month > 12)
            {
                date_.month -= 12;
                ++date_.year;
            }
        }
        else
        {
            while (date_.month < 1)
            {
                date_.month += 12;
                --date_.year;
            }
        }

        incr_all();

        return *this;
    }

    date_time& add_year(int year_)
    {
        date_.year += year_;
        incr_all();
        return *this;
    }

    /**
     * return day of week of date from 1 (monday) to 7 (sunday)
     */
    bool get_day_of_week(unsigned& day_) const
    {
        if (!is_valid_date()) return false;

        try
        {
            std::tm tm;
            if (strptime(get_date().c_str(), "%Y-%m-%d", &tm))
            {
                day_ = tm.tm_wday;
                if (day_ == 0) day_ = 7;
                return true;
            }
            else
            {
                basefunc_std::log("get_day_of_week failed from " + get_date(), "date_time", true, basefunc_std::COLOR::RED_COL);
                return false;
            }
        }
        catch(std::exception& ex)
        {
            basefunc_std::log("get_day_of_week failed from " + get_date() + " exception: " + std::string(ex.what()), "date_time", true, basefunc_std::COLOR::RED_COL);
            return false;
        }
    }

    void reset()
    {
        date_.reset();
        time_.reset();
    }

    friend std::ostream& operator<<(std::ostream& io, const date_time& o)
    {
        return io << o.get_date_time();
    }

private:

    void decr(int& from, int& to, int max)
    {
        int new_val = max + from % max;
        if (new_val == max) new_val = 0;
        int add_min = (from - new_val) / max;
        if (add_min != 0)
        {
            from = new_val;
            to += add_min;
        }
    }

    void decr_month()
    {
        if (date_.day < 1)
        {
            if (date_.month > 1) --date_.month;
            else
            {
                --date_.year;
                date_.month = 12;
            }

            int max_day_in_m = count_days_in_month(date_.month, date_.year);
            date_.day += max_day_in_m;
            decr_month();
        }
    }

    void incr_all(date_time_sel sel_ = date_time_sel::none)
    {
        if (time_.thousend_second > 0) incr(time_.thousend_second, time_.second, 1000); //ms
        else decr(time_.thousend_second, time_.second, 1000); //ms

        if (time_.second > 0) incr(time_.second, time_.minute, 60); //sec
        else decr(time_.second, time_.minute, 60); //sec

        if (time_.minute > 0) incr(time_.minute, time_.hour, 60); //min
        else decr(time_.minute, time_.hour, 60); //min

        if (time_.hour > 0) incr(time_.hour, date_.day, 24); //hour
        else decr(time_.hour, date_.day, 24); //hour

        if (date_.day > 0) incr_month(); //day month year
        else decr_month(); //day month year

        if (sel_ == date_time_sel::date_and_time || sel_ == date_time_sel::date_only) date_.is_valid_date_ = true;
        if (sel_ == date_time_sel::date_and_time || sel_ == date_time_sel::time_only) time_.is_valid_time_ = true;

        //make day of year
        date_.day_of_year = 0;
        for (int i = 1; i < date_.month; ++i) //full monthes
        {
            date_.day_of_year += date_time::count_days_in_month(i, date_.year);
        }
        date_.day_of_year += date_.day;
    }

    void incr(int& from, int& to, int max)
    {
        int add_min = (from - from % max) / max;
        if (add_min != 0)
        {
            from = from % max;
            to += add_min;
        }
    }

    void incr_month()
    {
        int max_day_in_m = count_days_in_month(date_.month, date_.year);
        if (date_.day > max_day_in_m)
        {
            if (date_.month < 12) ++date_.month;
            else
            {
                ++date_.year;
                date_.month = 1;
            }
            date_.day -= max_day_in_m;
            incr_month();
        }
    }

};

#endif // DATE_TIME_H
