#ifndef CURL_HTTP_H
#define CURL_HTTP_H

//#define NO_CURL_HTTP_USE_SSL

#include <stdio.h>
#include <curl/curl.h>
#include <curl/easy.h>
#include <string>
#include <string.h>
#include <pthread.h>
#include <iostream>
#include <ctype.h>
#include <cstring>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sstream>
#include <fstream>
#include <thread>
#include <cctype>
#include <mutex>
#include <chrono>
#include <unordered_map>
#include <functional>

#include "basefunc_std.h"
#include "ts_deque.h"
#include "timer.h"

#ifdef UNUSED
#elif defined(__GNUC__)
# define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#elif defined(__LCLINT__)
# define UNUSED(x) /*@unused@*/ x
#else
# define UNUSED(x) x
#endif

#define DEBUG_QRY2
#define CHECK_IP_TIME 300

#ifndef NO_USE_SSL
    #include <openssl/err.h>
#endif

#define MUTEX_TYPE       pthread_mutex_t
#define MUTEX_SETUP(x)   pthread_mutex_init(&(x), NULL)
#define MUTEX_CLEANUP(x) pthread_mutex_destroy(&(x))
#define MUTEX_LOCK(x)    pthread_mutex_lock(&(x))
#define MUTEX_UNLOCK(x)  pthread_mutex_unlock(&(x))
#define THREAD_ID        pthread_self()

/*#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>
#include <curlpp/Infos.hpp>

#include "TOpenSSL.h"

class WriterMemoryClass2
{
public:
    size_t WriteMemoryCallback(char* ptr, size_t size, size_t nmemb)
    {
        size_t realsize = size * nmemb;
        ret.append((char*)ptr, 0, realsize);
        return realsize;
    }

    std::string get()
    {
        return ret;
    }

    std::string ret;
};

class curl_pool
{
public:
    struct guard
    {
        guard(curl_pool* _p, CURL* _c, std::size_t _i) : p(_p), c(_c), i(_i) {  }
        ~guard() { p->release(i); }
        CURL* get() const { return c; }
    private:
        curl_pool* p;
        CURL* c;
        std::size_t i;
    };

    guard get()
    {
        std::lock_guard<std::mutex> _{ lock };
        if (!index.empty())
        {
            auto i = *(index.begin());
            index.erase(index.begin());

            return { this, data[i], i };
        }

        CURL* curl { curl_easy_init() };
        data.push_back(curl);
        return { this, curl, data.size() - 1 };
    }

private:
    std::mutex lock;
    std::vector<CURL*> data;
    std::unordered_set<std::size_t> index;

    friend class guard;

    void release(std::size_t i)
    {
        std::lock_guard<std::mutex> _{ lock };
        index.emplace(i);
    }
};*/

class ip_array_getter
{
public:
    struct ip_port
    {
        ip_port() = default;
        ip_port(const std::string& ip_, unsigned port_) : ip(ip_), port(port_), is_inited(true) { }

        bool is_available() const noexcept
        {
            std::chrono::steady_clock::time_point now_ = std::chrono::steady_clock::now();
            if (unavailable_before <= now_) return true;
            return false;
        }

        std::string ip;
        unsigned port = 0;

        bool is_inited = false;
        std::size_t index = 0;

        std::chrono::steady_clock::time_point unavailable_before = std::chrono::steady_clock::now();
    };

    void add(const std::string& ip_port_)
    {
        std::vector<std::string> exp = basefunc_std::split(ip_port_, ':');
        if (exp.size() == 2)
        {
            int port { 0 };
            if (basefunc_std::stoi(exp[1], port))
            {
                if (port > 0)
                {
                    std::lock_guard<std::mutex> _{ lock };
                    ips.push_back( { exp[0], (unsigned)port } );
                    size_ips.store(ips.size(), std::memory_order_release);
                    ips[ips.size() - 1].index = ips.size() - 1;
                }
            }
        }
    }

    ip_port get() const
    {
        int max_index = size_ips.load(std::memory_order_acquire) - 1;

        if (max_index == -1) return ip_port();

        int rand = max_index == 0 ? 0 : basefunc_std::rand(0, max_index);

        //search randomly
        ip_port ret;
        {
            std::lock_guard<std::mutex> _{ lock };
            ret = ips[rand];
        }

        if (ret.is_available()) return ret;

        //search any available
        std::lock_guard<std::mutex> _{ lock };
        for (const ip_port& ip : ips)
        {
            if (ip.is_available()) return ip;
        }

        //no availables - return default unavailable
        return ips[0];
    }

    void set_unavailability(std::size_t index, std::int32_t unavailable_minutes)
    {
        std::chrono::steady_clock::time_point now_ = std::chrono::steady_clock::now() + std::chrono::minutes(unavailable_minutes);
        std::lock_guard<std::mutex> _{ lock };
        if (index < ips.size()) ips[index].unavailable_before = now_;
    }

    bool empty() const
    {
        return ips.empty();
    }

    std::vector<ip_port> get_all() const
    {
        return ips;
    }

private:
    mutable std::mutex lock;
    std::vector<ip_port> ips;
    std::atomic_int size_ips = ATOMIC_VAR_INIT(0);
};

class ip_array_item
{
public:
    std::string ip;
    std::chrono::steady_clock::time_point time_checked;
};

class curl_http
{

public:
    curl_http();

    enum HEADER
    {
        FORM,
        JSON,
        NONE,
        XFORM,
        OCTET_STREAM,
        NO_CONTENT_TYPE
    };

    enum METHOD
    {
        POST,
        PUT,
        PATCH
    };

    enum SSL_CERT_TYPE
    {
        P12
    };

    /* This array will store all of the mutexes available to OpenSSL. */
    static MUTEX_TYPE *mutex_buf;

#ifndef NO_CURL_HTTP_USE_SSL

    static void locking_function(int mode, int n, const char* UNUSED(file), int UNUSED(line))
    {
      if(mode & CRYPTO_LOCK)
        MUTEX_LOCK(mutex_buf[n]);
      else
        MUTEX_UNLOCK(mutex_buf[n]);
    }

    static unsigned long id_function(void)
    {
      return ((unsigned long)THREAD_ID);
    }

    static int thread_setup(void)
    {
      int i;

      mutex_buf = (MUTEX_TYPE*)malloc(CRYPTO_num_locks() * sizeof(MUTEX_TYPE));
      if(!mutex_buf)
        return 0;
      //std::cout << "[Count crypto locks] " << CRYPTO_num_locks() << std::endl;
      for(i = 0;  i < CRYPTO_num_locks();  i++)
        MUTEX_SETUP(mutex_buf[i]);
      CRYPTO_set_id_callback(id_function);
      CRYPTO_set_locking_callback(locking_function);
      return 1;
    }

    static int thread_cleanup(void)
    {
      int i;

      if(!mutex_buf)
        return 0;
      CRYPTO_set_id_callback(NULL);
      CRYPTO_set_locking_callback(NULL);
      for(i = 0;  i < CRYPTO_num_locks();  i++)
        MUTEX_CLEANUP(mutex_buf[i]);
      free(mutex_buf);
      mutex_buf = NULL;
      return 1;
    }

#endif

    static std::pair<std::string, std::string> explode_uri(const std::string& uri);

    static std::string get_ip_from_array(const std::string& url)
    {
        unsigned sz = url.size();
        std::string mass = "";
        bool is_mass = false;
        bool find_first_digit = false;
        std::string prev_mass = "";
        std::string post_mass = "";

        for (unsigned i = 0; i < sz; ++i)
        {
            if (!find_first_digit && std::isdigit(url[i]))
            {
                find_first_digit = true;

                if (i > 0)
                {
                    if (url[i - 1] == '[')
                    {
                        is_mass = true;
                        prev_mass = url.substr(0, i - 1);
                    }
                    else
                    {
                        break;
                    }
                }
            }

            if (is_mass)
            {
                if (url[i] == ']')
                {
                    if (i + 1 < sz) post_mass = url.substr(i + 1);
                    break;
                }
                else
                {
                    mass += url[i];
                }
            }
        }

        if (is_mass)
        {
            std::string ip_worked = "";
            bool finded_worked = false;

            std::string key = prev_mass + mass;

            {
                std::lock_guard<std::mutex> lock_(curl_http::LOCK_IP_ARRAYS);
                auto find = IP_ARRAYS.find(key);
                if (find != IP_ARRAYS.end())
                {
                    ip_worked = find->second.ip;

                    if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - find->second.time_checked).count() < CHECK_IP_TIME)
                    {
                        finded_worked = true;
                    }
                }
            }

            if (finded_worked) return prev_mass + ip_worked + post_mass;
            else if (!ip_worked.empty())
            {
                std::string url_m = prev_mass + ip_worked;
                std::string resp;
                long http_code = get2(url_m, resp, 3);
                if (http_code != -1)
                {
                    //basefunc_std::log(url_m, "finded_continue", true, basefunc_std::COLOR::GREEN_COL);

                    std::lock_guard<std::mutex> lock_(curl_http::LOCK_IP_ARRAYS);
                    IP_ARRAYS[key].time_checked = std::chrono::steady_clock::now();

                    return url_m + post_mass;
                }
            }

            std::vector<std::string> exp = basefunc_std::split(mass, ',');
            unsigned sz_exp = exp.size();
            for (unsigned i = 0; i < sz_exp; ++i)
            {
                std::string url_m = prev_mass + exp[i];
                std::string resp;
                long http_code = get2(url_m, resp, 3);
                if (http_code != -1)
                {
                    ip_array_item item;
                    item.ip = exp[i];
                    item.time_checked = std::chrono::steady_clock::now();

                    //basefunc_std::log(url_m, "finded", true, basefunc_std::COLOR::GREEN_COL);

                    std::lock_guard<std::mutex> lock_(curl_http::LOCK_IP_ARRAYS);
                    IP_ARRAYS[key] = item;

                    return url_m + post_mass;
                }
                else
                {
                    basefunc_std::log("Server not seems to worked: " + url_m, "WORKED_SERVER_ERROR", true, basefunc_std::COLOR::RED_COL);
                }
            }

            basefunc_std::log("Cant find worked server: " + url, "WORKED_SERVER_ERROR", true, basefunc_std::COLOR::RED_COL);

            if (sz_exp > 0) //set first to worked
            {
                ip_array_item item;
                item.ip = exp[0];
                item.time_checked = std::chrono::steady_clock::now();

                std::lock_guard<std::mutex> lock_(curl_http::LOCK_IP_ARRAYS);
                IP_ARRAYS[key] = item;

                return prev_mass + exp[0] + post_mass;
            }

            //something wrong
            return url;
        }
        else
        {
            return url;
        }
    }

    static std::string simpleGet_old(std::string url, int timeOut = 5, int timeUotUSec = 0);
    inline static void simpleGetUsync(std::string url, int timeOut = 5, int timeUotUSec = 0)
    {
        std::thread t([=]
        {
#ifdef DEBUG_QRY
            basefunc_std::cout(simpleGet(url, timeOut, timeUotUSec), "curl_http::simpleGetUsync", basefunc_std::COLOR::YELLOW_COL);
#else
            simpleGet(url, timeOut, timeUotUSec);
#endif
        });
        t.detach();
    }

    struct WriteThis
    {
        const char *readptr;
        long sizeleft;
    };

    static size_t read_callback(void *ptr, size_t size, size_t nmemb, void *userp)
    {
        struct WriteThis *pooh = (struct WriteThis *)userp;

        if(size*nmemb < 1) return 0;

        if(pooh->sizeleft)
        {
            *(char *)ptr = pooh->readptr[0]; /* copy one single byte */
            pooh->readptr++;                 /* advance pointer */
            pooh->sizeleft--;                /* less data left */
            return 1;                        /* we return 1 byte at a time! */
        }

        return 0;                            /* no more data left to deliver */
    }

    static size_t write_to_string(void *ptr, size_t size, size_t count, void *stream)
    {
        ((std::string*)stream)->append((char*)ptr, size*count); //, 0
        return size*count;
    }

    /**
     * return 0 if success, -1 if fail
     */
    static int post(const std::string& url_, const std::string& postData, std::string& response, long timeout = 5, HEADER header_ = HEADER::JSON, std::string custom_headers = "")
    {
        long http_code = post_start(url_, postData, response, timeout, header_, custom_headers, nullptr, nullptr, std::map<std::string, std::string>());
        if (http_code == 200 || http_code == 202 || http_code == 204) return 0;
        return -1;
    }

    static long patch(const std::string& url_, const std::string& postData, std::string& response, long timeout = 5, HEADER header_ = HEADER::JSON, std::string custom_headers = "")
    {
        long http_code = post_start(url_, postData, response, timeout, header_, custom_headers, nullptr, nullptr, std::map<std::string, std::string>(), METHOD::PATCH);
        if (http_code == 200 || http_code == 202 || http_code == 204) return 0;
        return -1;
    }

    static long post(const std::string& url_, const std::string& postData, std::string& response, std::vector<std::string>& coockies, long timeout = 5, HEADER header_ = HEADER::JSON, std::string custom_headers = "", std::map<std::string, std::string> send_cookie = std::map<std::string, std::string>())
    {
        return post_start(url_, postData, response, timeout, header_, custom_headers, nullptr, [&coockies](CURL* curl)
        {
            struct curl_slist *cookies { nullptr };
            struct curl_slist *nc { nullptr };

            if(curl_easy_getinfo(curl, CURLINFO_COOKIELIST, &cookies) == CURLE_OK)
            {
                nc = cookies;
                while(nc)
                {
                    coockies.push_back(nc->data);
                    nc = nc->next;
                }
            }

            curl_slist_free_all(cookies);
        }, send_cookie);
    }

    static long post(const std::string& url_, const std::string& postData, std::string& response, SSL_CERT_TYPE ssl_cert_type, std::string cert_path, std::string cert_pass = "", long timeout = 5, HEADER header_ = HEADER::JSON, std::string custom_headers = "")
    {
        return post_start(url_, postData, response, timeout, header_, custom_headers, [&](CURL* curl)
        {
            curl_easy_setopt(curl, CURLOPT_SSLCERT, cert_path.c_str());

            switch (ssl_cert_type)
            {
                case SSL_CERT_TYPE::P12: curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "P12"); break;
            }

            if (!cert_pass.empty())
                curl_easy_setopt(curl, CURLOPT_KEYPASSWD, cert_pass.c_str());
        },
        nullptr, std::map<std::string, std::string>());
    }

    static long post(const std::string& url_, const std::string& postData, std::string& response, std::function<void(CURL *curl)> f_setup_curl, long timeout = 5, HEADER header_ = HEADER::JSON, std::string custom_headers = "")
    {
        return post_start(url_, postData, response, timeout, header_, custom_headers, f_setup_curl, nullptr, std::map<std::string, std::string>());
    }

    struct exploded_item
    {
        std::string headers, url, data;
        bool success = false;
    };

    /**
     * return 0 if success, -1 if fail
     */
    static int post_string(const std::string& url_string, std::string& response, long timeout = 5)
    {
        exploded_item it = curl_http::explode_url(url_string);
        return post(it.url, it.data, response, timeout, HEADER::NONE, it.headers);
    }

    static exploded_item explode_url(const std::string& url_string)
    {
        //parse string
        exploded_item it;

        enum class sw
        {
            start,
            reading_option,
            reading_option_full,
            reading_value_to_space,
            reading_value_to_qoute
        };

        enum class sw_read
        {
            header,
            url,
            data,
            none
        };

        sw swithc_ = sw::start;
        sw_read swithc_read = sw_read::url;
        std::string readed;
        for (auto i = 0u; i < url_string.size(); ++i)
        {
            const char c = url_string[i];
            switch (c)
            {
                case '-':
                {
                    if (swithc_ == sw::start) swithc_ = sw::reading_option;
                    else if (swithc_ == sw::reading_option) swithc_ = sw::reading_option_full;
                    else if (swithc_ == sw::reading_option_full)
                    {
                        basefunc_std::log("sw::reading_option_full " + url_string, "curl_http_parse_string", false, basefunc_std::COLOR::RED_COL);
                        return it;
                    }
                    else if (swithc_ == sw::reading_value_to_qoute || swithc_ == sw::reading_value_to_space) readed += c;

                    break;
                }
                case ' ':
                {
                    if (swithc_ == sw::reading_option) //mean that we have read option value
                    {
                        if (readed.compare("H") == 0) swithc_read = sw_read::header;
                        else if (readed.compare("d") == 0) swithc_read = sw_read::data;
                        else
                        {
                            basefunc_std::log("sw::reading_option " + url_string + " at " + std::to_string(i) + " readed: " + readed, "curl_http_parse_string", false, basefunc_std::COLOR::RED_COL);
                            return it;
                        }

                        readed.clear();
                        swithc_ = sw::start;
                    }
                    else if (swithc_ == sw::reading_option_full) //mean that we have read option value
                    {
                        if (readed.compare("header") == 0) swithc_read = sw_read::header;
                        else if (readed.compare("data") == 0) swithc_read = sw_read::data;
                        else
                        {
                            basefunc_std::log("sw::reading_option_full " + url_string, "curl_http_parse_string", false, basefunc_std::COLOR::RED_COL);
                            return it;
                        }

                        readed.clear();
                        swithc_ = sw::start;
                    }
                    else if (swithc_ == sw::reading_value_to_qoute) //add to reading value
                    {
                        readed += c;
                    }
                    else if (swithc_ == sw::reading_value_to_space) //ending to reading value
                    {
                        switch (swithc_read)
                        {
                            case sw_read::header: it.headers += '\n' + readed; break;
                            case sw_read::data: it.data = std::move(readed); break;
                            case sw_read::url: it.url = std::move(readed); break;
                            default: break;
                        }

                        readed.clear();
                        swithc_ = sw::start;
                        swithc_read = sw_read::url;
                    }

                    break;
                }
                case '"':
                {
                    if (swithc_ == sw::start)
                    {
                        swithc_ = sw::reading_value_to_qoute;
                    }
                    else if (swithc_ == sw::reading_value_to_qoute)
                    {
                        if (i > 0)
                        {
                            const char prev_char = url_string[i - 1];
                            if (prev_char != '\\')
                            {
                                switch (swithc_read)
                                {
                                    case sw_read::header: it.headers += '\n' + readed; break;
                                    case sw_read::data: it.data = std::move(readed); break;
                                    case sw_read::url: it.url = std::move(readed); break;
                                    default: break;
                                }

                                readed.clear();
                                swithc_ = sw::start;
                                swithc_read = sw_read::url;
                            }
                            else
                            {
                                if (!readed.empty()) readed[readed.size() - 1] = c; //replace escaping to qoute
                            }
                        }
                        else
                        {
                            basefunc_std::log("sw::reading_value_to_qoute " + url_string, "curl_http_parse_string", false, basefunc_std::COLOR::RED_COL);
                            return it;
                        }
                    }
                    else if (swithc_ == sw::reading_value_to_space)
                    {
                        readed += c;
                    }

                    break;
                }
                default:
                {
                    if (swithc_ == sw::start)
                    {
                        swithc_ = sw::reading_value_to_space;
                        readed += c;
                    }
                    else
                    {
                        readed += c;
                    }
                    break;
                }
            }
        }

        if (swithc_ == sw::reading_value_to_space) //ending to reading value
        {
            switch (swithc_read)
            {
                case sw_read::header: it.headers += '\n' + readed; break;
                case sw_read::data: it.data = std::move(readed); break;
                case sw_read::url: it.url = std::move(readed); break;
                default: break;
            }
        }

        it.success = true;
        return it;
    }

    /**
     * return 0 if success, -1 if fail
     */
    static int put(const std::string& url_, const std::string& postData, std::string& response, long timeout = 5, HEADER header_ = HEADER::JSON, std::string custom_headers = "")
    {
        long http_code = post_start(url_, postData, response, timeout, header_, custom_headers, nullptr, nullptr, std::map<std::string, std::string>(), METHOD::PUT);
        if (http_code == 200 || http_code == 202 || http_code == 204) return 0;
        return -1;
    }

    static long put(const std::string& url_, const std::string& postData, std::string& response, SSL_CERT_TYPE ssl_cert_type, std::string cert_path, std::string cert_pass = "", long timeout = 5, HEADER header_ = HEADER::JSON, std::string custom_headers = "")
    {
        return post_start(url_, postData, response, timeout, header_, custom_headers, [&](CURL* curl)
        {
            curl_easy_setopt(curl, CURLOPT_SSLCERT, cert_path.c_str());

            switch (ssl_cert_type)
            {
                case SSL_CERT_TYPE::P12: curl_easy_setopt(curl, CURLOPT_SSLCERTTYPE, "P12"); break;
            }

            if (!cert_pass.empty())
                curl_easy_setopt(curl, CURLOPT_KEYPASSWD, cert_pass.c_str());
        },
        nullptr, std::map<std::string, std::string>(), METHOD::PUT);
    }

    /**
     * return 0 if success, -1 if fail
     */
    static int get_repeate(const std::string& url_, std::string& response, long timeout, unsigned cnt_repeate, unsigned pause_seconds)
    {
        int ret;
        while (cnt_repeate > 0)
        {
            ret = get(url_, response, timeout);
            if (ret == 0) return ret;
            std::this_thread::sleep_for(std::chrono::seconds(pause_seconds));
            --cnt_repeate;
        }

        return ret;
    }

    /**
     * return 0 if success, -1 if fail
     */
    static int get(const std::string& url_, std::string& response, long timeout = 5, std::string custom_headers = "")
    {
        long http_code = get_start(url_, response, timeout, custom_headers, std::map<std::string, std::string>());
        if (http_code == 200 || http_code == 202 || http_code == 204) return 0;
        return -1;
    }

    static long get2(const std::string& url_, std::string& response, long timeout = 5, std::map<std::string, std::string> send_cookie = std::map<std::string, std::string>(), std::string custom_headers = "")
    {
        return get_start(url_, response, timeout, custom_headers, send_cookie);
    }


    /**
     * DEPRICATED
     */
    static std::string simpleGet(const std::string& url_, long timeout = 5, int UNUSED(timeUotUSec) = 0)
    {
        std::string url = get_ip_from_array(url_);

        basefunc_std::replaceAll(url, " ", "%20");

        init_curl();

        CURL *curl;
        CURLcode res;

        std::string response = "";

        curl = curl_easy_init();
        if(curl)
        {
            //set no signal (if curl cant resolve dns - it sends a siglnal sigalrm and then esche kakujuto hujnu)
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            /* example.com is redirected, so we tell libcurl to follow redirection */
            //curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
            curl_easy_setopt(curl, CURLOPT_HEADER, 0);

            /* complete within 5 seconds */
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);

            /* complete connection within 5 seconds */
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeout);

            //WRITE RESPONCE
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

            curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

            /* Perform the request, res will get the return code */
            res = curl_easy_perform(curl);
            /* Check for errors */
            if(res != CURLE_OK)
            {
                basefunc_std::cout(url + " curl_easy_perform() failed: " + curl_easy_strerror(res), "curl_easy_perform_failed_get", basefunc_std::COLOR::RED_COL);
            }

            /* always cleanup */
            curl_easy_cleanup(curl);

            //trim header
            auto f = response.find("\r\n\r\n");
            if (f != std::string::npos)
            {
                response = response.substr(f + 4);
            }
        }

        return response;
    }

    ///
    /// THREADED DEQUE
    ///

    struct threaded_deque_item
    {
        enum class type
        {
            GET,
            POST
        };

        threaded_deque_item() = default;
        threaded_deque_item(const std::string& url_, const std::string& qry_, type TYPE_, unsigned timeout_) : url(url_), qry(qry_), TYPE(TYPE_), timeout(timeout_) { }

        std::string url = "";
        std::string qry = "";
        type TYPE;
        unsigned timeout = 5;

        std::function<void(const std::string&, const std::string&, const std::string&)> callback = nullptr;
        std::function<void(int)> callback_fail = nullptr;
        curl_http::HEADER header = curl_http::HEADER::JSON;
    };

    static void init_threaded_deque()
    {
        DEQUE_THREAD = std::thread([]()
        {
            while (curl_http::DEUQE_IS_RUNNING.load(std::memory_order_acquire))
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));

                threaded_deque_item item;
                if (curl_http::DEQUE_MSG.pop_front(item))
                {
                    switch (item.TYPE)
                    {
                        case threaded_deque_item::type::GET:
                        {
                            std::string resp = "";
                            int  r = curl_http::get(item.url + "?" + item.qry, resp, item.timeout);
                            if (r == 0)
                            {
                                if (item.callback) item.callback(item.url, item.qry, resp);
                            }
                            else
                            {
                                if (item.callback_fail) item.callback_fail(r);
                            }

                            break;
                        }
                        case threaded_deque_item::type::POST:
                        {
                            std::string resp = "";
                            int  r = curl_http::post(item.url, item.qry, resp, item.timeout, item.header);
                            if (r == 0)
                            {
                                if (item.callback) item.callback(item.url, item.qry, resp);
                            }
                            else
                            {
                                if (item.callback_fail) item.callback_fail(r);
                            }

                            break;
                        }
                        default: break;
                    }
                }
            }
        });
    }

    static void stop_threaded_deque(bool wait)
    {
        if (curl_http::DEUQE_IS_RUNNING.load(std::memory_order_acquire))
        {
            curl_http::DEUQE_IS_RUNNING.store(false, std::memory_order_release);
            if (wait && curl_http::DEQUE_THREAD.joinable()) curl_http::DEQUE_THREAD.join();
        }
    }

    static ts_deque<threaded_deque_item> DEQUE_MSG;

    static void cleanup_curl()
    {
        if (curl_http::is_inited)
        {
            curl_global_cleanup();
        }
    }

    /**
     * return http code if success, -1 if fail
     */
    static long get_start(const std::string& url_, std::string& response, long timeout, const std::string& custom_headers, const std::map<std::string, std::string>& send_cookie, std::function<void(CURL* _curl)> f_setup_curl = nullptr)
    {
        std::string url = get_ip_from_array(url_);

        basefunc_std::replaceAll(url, " ", "%20");
        //basefunc_std::cout(url, "url");

        long ret_res = -1;

        init_curl();

        CURL *curl;
        CURLcode res;

        curl = curl_easy_init();
        if(curl)
        {
            //set no signal (if curl cant resolve dns - it sends a siglnal sigalrm and then esche kakujuto hujnu)
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            /* example.com is redirected, so we tell libcurl to follow redirection */
            //curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

            //curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_1);

            curl_easy_setopt(curl, CURLOPT_HEADER, 0);

            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
            //curl_easy_setopt(curl, CURLOPT_SSL_VERIFYSTATUS, 0);

            struct curl_slist *headers = NULL;

            if (!custom_headers.empty())
            {
                std::vector<std::string> exp = basefunc_std::split(custom_headers, '\n');
                for (const auto& it : exp)
                {
                    if (it.empty()) continue;
                    headers = curl_slist_append(headers, it.c_str());
                }

                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            }

            if (!send_cookie.empty())
            {
                std::string s_cookie;
                for (const auto& it : send_cookie)
                {
                    s_cookie += it.first + "=" + it.second + "; ";
                }

                curl_easy_setopt(curl, CURLOPT_COOKIE, s_cookie.c_str());
            }

            /* complete within 5 seconds */
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);

            /* complete connection within 5 seconds */
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeout);

            progress_data pd { timeout * 1000 };
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &pd);
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ff);

      #ifdef DEBUG_QRY
            /* get verbose debug output please */
            curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
      #endif
            //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

            //WRITE RESPONCE
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

            curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
            //curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
            curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 50L);
            curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);
            curl_easy_setopt(curl, CURLOPT_FTP_SKIP_PASV_IP, 1L);
            curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);

            if (f_setup_curl != nullptr) f_setup_curl(curl);

            /* Perform the request, res will get the return code */
            res = curl_easy_perform(curl);
            /* Check for errors */
            if(res != CURLE_OK)
            {
                basefunc_std::cout(url + " curl_easy_perform() failed: " + curl_easy_strerror(res), "curl_easy_perform_failed_get", basefunc_std::COLOR::RED_COL);
            }
            else
            {
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &ret_res);
            }

            /* always cleanup */
            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);

            //trim header
            auto f = response.find("\r\n\r\n");
            if (f != std::string::npos)
            {
                response = response.substr(f + 4);
            }
        }

        return ret_res;
    }
public:

//    static long post_test4(const std::string& url_, const std::string& postData, std::string& response, long timeout, HEADER header_, const std::string& custom_headers)
//    {
//        std::string t_pocket;
//        {
//            std::stringstream stream;
//            stream<<"POST / HTTP/1.1\r\n"
//                  <<"Host: 192.168.223.144\r\n"
//                  <<"Accept: */*\r\n"
//                  <<"Connection: keep-alive\r\n"
//                  <<"Content-Length:" << postData.size() << "\r\n"
//                  <<"\r\n"
//                 << postData;
//            t_pocket = stream.str();
//        }

//        s_devices::TOpenSSL t_connect(16
//                                      ,"192.168.223.144"
//                                      ,443
//                                      ,true );

//        t_connect.connect();
//        t_connect.write( t_pocket );
//        response = t_connect.read();

//        return 0;
//    }


    /*static long post_test2(const std::string& url_, const std::string& postData, std::string& response, long timeout, HEADER header_, const std::string& custom_headers)
    {
        try {
            curlpp::Easy request;

            request.setOpt(new curlpp::options::Url(url_));
            request.setOpt(new curlpp::options::Verbose(false));

            std::list<std::string> header;

            auto exp = basefunc_std::split(custom_headers, '\n');
            for (const auto s : exp)
            {
                if (!s.empty()) header.push_back(s);
            }

            request.setOpt(new curlpp::options::HttpHeader(header));

            WriterMemoryClass2 mWriterChunk;
            using namespace std::placeholders;
            curlpp::types::WriteFunctionFunctor functor = std::bind(&WriterMemoryClass2::WriteMemoryCallback, &mWriterChunk, _1, _2, _3);

            curlpp::options::WriteFunction *test = new curlpp::options::WriteFunction(functor);
            request.setOpt(test);

            request.setOpt(new curlpp::options::PostFields(postData));
            request.setOpt(new curlpp::options::PostFieldSize(postData.size()));
            request.setOpt(new curlpp::options::NoSignal(true));
            //request.setOpt(new curlpp::options::(true));

            request.perform();
            response = mWriterChunk.get();
          }
          catch ( curlpp::LogicError & e ) {
            std::cout << e.what() << std::endl;
          }
          catch ( curlpp::RuntimeError & e ) {
            std::cout << e.what() << std::endl;
          }

        return 0;
    }*/

    static long post_test(const std::string& url_, const std::string& postData, std::string& response, long timeout, HEADER header_, const std::string& custom_headers)
    {
        std::string url = get_ip_from_array(url_);

        long ret_res = -1;

        init_curl();

        CURL *curl = nullptr;
        CURLcode res;

        /* get a curl handle */
        if (curl == nullptr) curl = curl_easy_init();

        if (curl)
        {
            struct WriteThis pooh;

            pooh.readptr = postData.c_str();
            pooh.sizeleft = (long)strlen(postData.c_str());

            struct curl_slist *headers = NULL;

            if (header_ != HEADER::NONE || !custom_headers.empty())
            {
                //headers = curl_slist_append(headers, "Accept: application/json");
                switch (header_)
                {
                    case HEADER::FORM: headers = curl_slist_append(headers, "Content-Type: multipart/form-data"); break;
                    case HEADER::JSON: headers = curl_slist_append(headers, "Content-Type: application/json"); break;
                    case HEADER::XFORM: headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded"); break;
                    default: headers = curl_slist_append(headers, "Content-Type: application/json"); break;
                }
                //headers = curl_slist_append(headers, "charsets: utf-8");

                std::vector<std::string> exp = basefunc_std::split(custom_headers, '\n');
                for (const auto& it : exp)
                {
                    if (it.empty()) continue;
                    headers = curl_slist_append(headers, it.c_str());
                }
            }

            headers = curl_slist_append(headers, "Expect:");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            //set no signal (if curl cant resolve dns - it sends a siglnal sigalrm and then esche kakujuto hujnu)
            //curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);

            /* First set the URL that is about to receive our POST. */
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);

            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
            //curl_easy_setopt(curl, CURLOPT_SSL_VERIFYSTATUS, 0);

            //curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_1);

            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            /* we want to use our own read function */
            //curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);

            curl_easy_setopt(curl, CURLOPT_HEADER, 0);

            /* complete within 5 seconds */
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);

            /* complete connection within 5 seconds */
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeout);

            curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);

            /* pointer to pass to our read function */
            //curl_easy_setopt(curl, CURLOPT_READDATA, &pooh);


            /* Now specify the POST data */
            //curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "name=wer");
      #ifdef DEBUG_QRY
            /* get verbose debug output please */
            curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
      #endif

            //WRITE RESPONCE
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

          /*
            If you use POST to a HTTP 1.1 server, you can send data without knowing
            the size before starting the POST if you use chunked encoding. You
            enable this by adding a header like "Transfer-Encoding: chunked" with
            CURLOPT_HTTPHEADER. With HTTP 1.0 or without chunked transfer, you must
            specify the size in the request.
          */
      #ifdef USE_CHUNKED
          {
            struct curl_slist *chunk = NULL;

            chunk = curl_slist_append(chunk, "Transfer-Encoding: chunked");
            res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
            /* use curl_slist_free_all() after the *perform() call to free this
               list again */
          }
      #else
          /* Set the expected POST size. If you want to POST large amounts of data,
             consider CURLOPT_POSTFIELDSIZE_LARGE */
          //curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, pooh.sizeleft);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());

            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)postData.size());
            curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 50L);
            curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);
            curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
            curl_easy_setopt(curl, CURLOPT_FTP_SKIP_PASV_IP, 1L);
            curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);

      #endif

      #ifdef DISABLE_EXPECT
          /*
            Using POST with HTTP 1.1 implies the use of a "Expect: 100-continue"
            header.  You can disable this header with CURLOPT_HTTPHEADER as usual.
            NOTE: if you want chunked transfer too, you need to combine these two
            since you can only set one list of headers with CURLOPT_HTTPHEADER. */

          /* A less good option would be to enforce HTTP 1.0, but that might also
             have other implications. */
          {
            struct curl_slist *chunk = NULL;

            chunk = curl_slist_append(chunk, "Expect:");
            res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
            /* use curl_slist_free_all() after the *perform() call to free this
               list again */
          }
      #endif

            //if (f_read_cookies != nullptr) curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");

            /* Perform the request, res will get the return code */
            res = curl_easy_perform(curl);

            /* Check for errors */
            if (res != CURLE_OK)
            {
                basefunc_std::cout(url + " curl_easy_perform() failed: " + curl_easy_strerror(res), "curl_easy_perform_failed_post", basefunc_std::COLOR::RED_COL);
            }
            else
            {
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &ret_res);
            }

            /* always cleanup */
            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);

        }

        //trim header
        auto f = response.find("\r\n\r\n");
        if (f != std::string::npos)
        {
            response = response.substr(f + 4);
        }

        return ret_res;
    }

    struct progress_data
    {
        progress_data(std::int64_t _timeout) : timeout(_timeout) { }
        timer tm;
        std::int64_t timeout;
    };

    static std::size_t ff(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
    {
        progress_data* pd = (progress_data*)clientp;
        if (pd->tm.elapsed_mili(false) >= pd->timeout) return 1;
        return 0;
    }

    /**
     * return http code if success, -1 if fail
     */
    static long post_start(const std::string& url_, const std::string& postData, std::string& response, long timeout, HEADER header_, const std::string& custom_headers, std::function<void(CURL *curl)> f_setup_curl, std::function<void(CURL *curl)> f_read_cookies, const std::map<std::string, std::string>& send_cookie, METHOD method = METHOD::POST)
    {
        std::string url = get_ip_from_array(url_);

        long ret_res = -1;

        init_curl();

        CURL *curl = nullptr;
        CURLcode res;

        /* get a curl handle */
        curl = curl_easy_init();

        if (curl)
        {
            struct WriteThis pooh;

            pooh.readptr = postData.c_str();
            pooh.sizeleft = (long)strlen(postData.c_str());

            struct curl_slist *headers = NULL;

            if (header_ != HEADER::NONE || !custom_headers.empty())
            {
                //headers = curl_slist_append(headers, "Accept: application/json");
                switch (header_)
                {
                    case HEADER::FORM: headers = curl_slist_append(headers, "Content-Type: multipart/form-data"); break;
                    case HEADER::JSON: headers = curl_slist_append(headers, "Content-Type: application/json"); break;
                    case HEADER::XFORM: headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded"); break;
                    case HEADER::OCTET_STREAM: headers = curl_slist_append(headers, "Content-Type: application/octet-stream"); break;
                    case HEADER::NO_CONTENT_TYPE: break;
                    default: headers = curl_slist_append(headers, "Content-Type: application/json"); break;
                }
                //headers = curl_slist_append(headers, "charsets: utf-8");

                std::vector<std::string> exp = basefunc_std::split(custom_headers, '\n');
                for (const auto& it : exp)
                {
                    if (it.empty()) continue;
                    headers = curl_slist_append(headers, it.c_str());
                }
            }

            headers = curl_slist_append(headers, "Expect:");
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            if (!send_cookie.empty())
            {
                std::string s_cookie;
                for (const auto& it : send_cookie)
                {
                    s_cookie += it.first + "=" + it.second + "; ";
                }
                curl_easy_setopt(curl, CURLOPT_COOKIE, s_cookie.c_str());
            }

            /* Now specify we want to POST data */
            switch (method)
            {
                case PUT: curl_easy_setopt(curl, CURLOPT_PUT, 1L);
                case PATCH: curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
                default: curl_easy_setopt(curl, CURLOPT_POST, 1L);
            }

            //set no signal (if curl cant resolve dns - it sends a siglnal sigalrm and then esche kakujuto hujnu)
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);

            /* First set the URL that is about to receive our POST. */
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
            //curl_easy_setopt(curl, CURLOPT_SSL_VERIFYSTATUS, 0);

            //curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_1);

            /* we want to use our own read function */
            //curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_callback);

            curl_easy_setopt(curl, CURLOPT_HEADER, 0);

            /* complete within 5 seconds */
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);

            /* complete connection within 5 seconds */
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeout);

            progress_data pd { timeout * 1000 };
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &pd);
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ff);

            /* pointer to pass to our read function */
            //curl_easy_setopt(curl, CURLOPT_READDATA, &pooh);

            if (f_setup_curl != nullptr) f_setup_curl(curl);

            /* Now specify the POST data */
            //curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "driver_document_number=123");
      #ifdef DEBUG_QRY
            /* get verbose debug output please */

      #endif

            //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);

            //WRITE RESPONCE
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_string);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

            curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
            //curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
            curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 50L);
            curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_2TLS);
            //curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
            curl_easy_setopt(curl, CURLOPT_FTP_SKIP_PASV_IP, 1L);
            curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);

          /*
            If you use POST to a HTTP 1.1 server, you can send data without knowing
            the size before starting the POST if you use chunked encoding. You
            enable this by adding a header like "Transfer-Encoding: chunked" with
            CURLOPT_HTTPHEADER. With HTTP 1.0 or without chunked transfer, you must
            specify the size in the request.
          */
      #ifdef USE_CHUNKED
          {
            struct curl_slist *chunk = NULL;

            chunk = curl_slist_append(chunk, "Transfer-Encoding: chunked");
            res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
            /* use curl_slist_free_all() after the *perform() call to free this
               list again */
          }
      #else
          /* Set the expected POST size. If you want to POST large amounts of data,
             consider CURLOPT_POSTFIELDSIZE_LARGE */
          //curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, pooh.sizeleft);
          curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
          curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)postData.size());
      #endif

      #ifdef DISABLE_EXPECT
          /*
            Using POST with HTTP 1.1 implies the use of a "Expect: 100-continue"
            header.  You can disable this header with CURLOPT_HTTPHEADER as usual.
            NOTE: if you want chunked transfer too, you need to combine these two
            since you can only set one list of headers with CURLOPT_HTTPHEADER. */

          /* A less good option would be to enforce HTTP 1.0, but that might also
             have other implications. */
          {
            struct curl_slist *chunk = NULL;

            chunk = curl_slist_append(chunk, "Expect:");
            res = curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
            /* use curl_slist_free_all() after the *perform() call to free this
               list again */
          }
      #endif



            if (f_read_cookies != nullptr) curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");

            /* Perform the request, res will get the return code */
            res = curl_easy_perform(curl);

            /* Check for errors */
            if (res != CURLE_OK)
            {
                basefunc_std::cout(url + " curl_easy_perform() failed: " + curl_easy_strerror(res), "curl_easy_perform_failed_post", basefunc_std::COLOR::RED_COL);
            }
            else
            {
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &ret_res);
            }

            if (f_read_cookies != nullptr) f_read_cookies(curl);

            /* always cleanup */
            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);

        }

        //trim header
        auto f = response.find("\r\n\r\n");
        if (f != std::string::npos)
        {
            response = response.substr(f + 4);
        }

        return ret_res;
    }

    static void init_curl()
    {
        std::call_once(load_curl, [&]()
        {
            CURLcode res;

            /* In windows, this will init the winsock stuff */
            res = curl_global_init(CURL_GLOBAL_DEFAULT);
            /* Check for errors */
            if(res != CURLE_OK)
            {
                fprintf(stderr, "curl_global_init() failed: %s\n", curl_easy_strerror(res));
            }
            else
            {
                curl_http::is_inited = true;
            }

#ifndef NO_CURL_HTTP_USE_SSL
            int res_init = thread_setup();
            if (res_init == 0)
            {
                basefunc_std::log("Error init multithreaded ssl", "curl_ssl", true, basefunc_std::COLOR::RED_COL);
            }
#endif

        });
    }
    static std::once_flag load_curl;
    static bool is_inited;

    static std::unordered_map<std::string, ip_array_item> IP_ARRAYS;
    static std::mutex LOCK_IP_ARRAYS;

    static std::thread DEQUE_THREAD;
    static std::atomic<bool> DEUQE_IS_RUNNING;
};

#endif // CURL_HTTP_H
