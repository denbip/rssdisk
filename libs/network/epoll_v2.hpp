#ifndef epoll_v2_H
#define epoll_v2_H

#include <netinet/tcp.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>

#include <iostream>

#include <atomic>
#include <mutex>
#include <unordered_map>
#include <thread>
#include "../basefunc_std.h"
#include "../date_time.h"
#include "../thread_worker.h"
#include "../cryptopp_.h"
#include "../DH/dh.h"
#include "../bitbase.h"
#include "../md5.h"
#include "../gost_28147_89/gost_89.h"
#include "../commpression_zlib.h"
#include <deque>
#include <functional>
#include "../timer.h"

#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>

typedef boost::shared_mutex Lock;
typedef boost::unique_lock< Lock >  WriteLock;
typedef boost::shared_lock< Lock >  ReadLock;

#define epoll_multithreaded
#define split_short_messages
#define epoll_mt_parser
//#define debug_epoll_v2

class epoll_v2
{
public:
    struct event_edb
    {
        std::string data;
        std::int32_t type = 0;
    };

    struct message_helper
    {
        std::vector<char> message;
        std::vector<char> del_message;
        std::int32_t seek_delimeter = -1;

        void clear()
        {
            message.clear();
            del_message.clear();
            seek_delimeter = -1;
        }
    };

    struct secure
    {
        bool is_aes = false;
        bool is_gost_enc = false;
        std::string aes_key;
        std::string aes_iv;
    };

    struct socket_client
    {
        int fd = 0;
        std::string ip;
        int time_to_send_test_ping = 0;
        int time_to_auth = 20000;
        bool is_authorized = false;

        std::uint64_t count_accepted_on = 0;

        secure _s;
        message_helper _m;
    };

    class responser
    {
    public:
        struct item
        {
            struct sub
            {
                sub() = default;
                explicit sub(const std::string& _str) : str(_str) { }
                explicit sub(std::string&& _str) : str(std::move(_str)) { }
                explicit sub(const std::string& _str, bool _is_crypted) : str(_str), is_crypted(_is_crypted) { }
                explicit sub(std::string&& _str, bool _is_crypted) : str(std::move(_str)), is_crypted(_is_crypted) { }

                std::string str;
                bool is_crypted = false;
                bool is_key_generation = false;
            };

            std::map<std::uint64_t, std::vector<sub>> data;
        };

        void set_eventfd_fd(std::int32_t eventfd_fd_)
        {
            eventfd_fd = eventfd_fd_;
        }

        template<class T>
        void add(std::int32_t fd, std::uint64_t count_accepted_on, T&& msg)
        {
            add_impl(fd, count_accepted_on, nullptr, std::forward<T>(msg));
        }

        template<class T>
        void add(std::int32_t fd, std::uint64_t count_accepted_on, const epoll_v2::secure* _s, T&& msg, bool is_crypted = false)
        {
            add_impl(fd, count_accepted_on, _s, std::forward<T>(msg), is_crypted);
        }

        std::unordered_map<std::int32_t, item> get_and_clear()
        {
            std::unordered_map<std::int32_t, item> ret;
            {
                std::lock_guard<std::mutex> _{ lock };
                std::swap(ret, r);
                is_empty.store(true, std::memory_order_release);
            }

            return ret;
        }

        std::vector<event_edb> get_events()
        {
            std::vector<event_edb> ret;
            {
                std::lock_guard<std::mutex> _{ lock };
                std::swap(ret, events);
            }

            return ret;
        }

        void add_event(const event_edb& ev)
        {
            std::lock_guard<std::mutex> _{ lock };
            events.push_back(ev);
            is_empty.store(false, std::memory_order_release);
            eventfd_write(eventfd_fd, 1);
        }

        std::unordered_map<std::int32_t, std::unordered_set<std::int32_t>> get_new_events_listeners()
        {
            std::unordered_map<std::int32_t, std::unordered_set<std::int32_t>> ret;
            {
                std::lock_guard<std::mutex> _{ lock };
                std::swap(ret, to_add_events_fd);
            }

            return ret;
        }

        void listen_events(std::int32_t fd, const std::unordered_set<std::int32_t>& types)
        {
            std::lock_guard<std::mutex> _{ lock };
            to_add_events_fd.insert( { fd, types } );
        }

        inline bool empty() const { return is_empty.load(std::memory_order_acquire); }

    private:
        std::mutex lock;
        std::unordered_map<std::int32_t, item> r;
        std::unordered_map<std::int32_t, std::unordered_set<std::int32_t>> to_add_events_fd;
        std::atomic_bool is_empty = ATOMIC_VAR_INIT(true);
        std::vector<event_edb> events;

        void add_impl(std::int32_t fd, std::uint64_t count_accepted_on, const epoll_v2::secure* _s, std::string&& msg, bool is_crypted = false)
        {
            if (!is_crypted && _s != nullptr)
            {
                if (_s->is_aes && !_s->aes_key.empty())
                {
                    if (_s->is_gost_enc) msg = gost::g89().crypt(msg, _s->aes_key + _s->aes_iv);
                    msg = cryptopp_::encrypt(msg, _s->aes_key, _s->aes_iv, 32, false);
                    is_crypted = true;
                }
            }

            std::lock_guard<std::mutex> _{ lock };
            r[fd].data[count_accepted_on].push_back(std::move(item::sub(std::move(msg), is_crypted)));
            is_empty.store(false, std::memory_order_release);
            eventfd_write(eventfd_fd, 1);
        }

        void add_impl(std::int32_t fd, std::uint64_t count_accepted_on, const epoll_v2::secure* _s, const std::string& msg, bool is_crypted = false)
        {
            std::string msg2 { msg };
            add_impl(fd, count_accepted_on, _s, std::move(msg2), is_crypted);
        }

        void add_impl(std::int32_t fd, std::uint64_t count_accepted_on, const epoll_v2::secure* _s, item::sub&& msg)
        {
            std::lock_guard<std::mutex> _{ lock };
            r[fd].data[count_accepted_on].push_back(std::move(msg));
            is_empty.store(false, std::memory_order_release);
            eventfd_write(eventfd_fd, 1);
        }

        void add_impl(std::int32_t fd, std::uint64_t count_accepted_on, const epoll_v2::secure* _s, const item::sub& msg)
        {
            item::sub msg2 { msg };
            add_impl(fd, count_accepted_on, _s, std::move(msg2));
        }

        std::int32_t eventfd_fd = 0;
    };

    typedef std::function<void(socket_client& client_, std::string &&message_, thread_pool::thread_pool& t_pool_, responser& responser_)> callback;

    epoll_v2(std::int32_t t_pool_size_, std::string det_string_ = "\n", std::string echo_string_ = "00", bool is_aes_ = false, bool is_gost_enc_ = false);
    ~epoll_v2();

    bool listen(const std::string& ip, int port, std::atomic_bool& is_running);

    responser& get_responser() { return responser_; }

    std::size_t get_count_connected() const { return count_connected.load(std::memory_order_relaxed); }

    callback on_message = nullptr;

#ifdef debug_epoll_v2
    bool debug = false;
    int debug_fd = 0;
#endif

private:
    const int MAX_EVENTS = 4096;

    struct m_write
    {
        m_write() = default;
        m_write(const std::string& s) : msg(s) { }
        m_write(std::string&& s) : msg(std::move(s)) { }
        m_write(std::string&& s, std::size_t _t_size_send) : msg(std::move(s)), t_size_send(_t_size_send) { }

        std::size_t t_size_send = 0;
        std::string msg;
        bool is_on_waiting_epollout = false;
    };

    bool create_socket(int& sock);
    bool bind(int sock, const std::string& ip, int port);
    bool set_nonblocking(int fd);
    bool socket_check(int fd);
    void remove(int epoll_fd, int fd, bool need_del = true);
    void clear();

    template<class T>
    void push_message(int fd, T&& msg)
    {
        push_message_impl(fd, std::forward<T>(msg));
    }

    void push_message_impl(int fd, responser::item::sub&& msg);
    void push_message_impl(int fd, const responser::item::sub& msg);

    static std::vector<std::string> process_read(message_helper& _m, const char *buf, ssize_t nbytes, const std::string& _det_string);
    static std::vector<std::string> process_read2(message_helper& _m, const char *buf, ssize_t nbytes, const std::string& _det_string);
    void process_message(socket_client &c, std::string&& str);

    int write_to_fd(int epoll_fd, int fd);

    //single thread
    std::unordered_map<int, socket_client> authorized_clients;
    std::unordered_set<int> awaiting_authorize_clients;
    std::unordered_map<std::int32_t, std::unordered_set<std::int32_t>> events_clients;
    std::unordered_map<int, std::deque<m_write*> > awaiting_for_write;

#ifdef epoll_multithreaded
#ifdef epoll_v2_control_activity
    struct
    {
        const std::int32_t period_check = 3600;
        const std::int32_t max_time_inactive = 3600;
        std::time_t last_check = time(0);
        std::unordered_map<int, std::time_t> data;
    } last_activity;
#endif
#endif

    std::atomic_size_t count_connected = ATOMIC_VAR_INIT(0);

    //threaded
    thread_pool::thread_pool t_pool;
    responser responser_;

    std::string det_string;
    std::string echo_string;
    std::uint64_t count_accepted = 0;
    bool is_aes = false;
    bool is_gost_enc = false;

    struct epoll_event event;

#ifdef epoll_multithreaded
    struct e_multithreaded
    {
        enum class todo
        {
            ok,
            ok_read,
            ok_parsed,
            remove,
            set_epoll_out
        };

        struct responce
        {
            responce() = default;
            responce(const responce& _d) = delete;
            responce& operator=(const responce& _d) = delete;

            responce(responce&& _d)
            {
                status = _d.status;
                rest_of_message = std::move(_d.rest_of_message);
            }

            responce& operator=(responce&& _d)
            {
                status = _d.status;
                rest_of_message = std::move(_d.rest_of_message);

                return *this;
            }

            struct message
            {
                message() = delete;
                message(const message& _d) = delete;
                message& operator=(const message& _d) = delete;

                message(message&& _d)
                {
                    d = std::move(_d.d);
                    cursor = _d.cursor;
                }

                message& operator=(message&& _d)
                {
                    d = std::move(_d.d);
                    cursor = _d.cursor;

                    return *this;
                }

                message(std::string&& _d) : d(std::move(_d)) { }
                message(std::string&& _d, std::size_t _cursor) : d(std::move(_d)), cursor(_cursor) { }

                std::string d;
                std::size_t cursor = 0;
            };

            todo status;
            std::vector<message> rest_of_message;
        };

        std::string det_string;

        std::unordered_set<int> fd_in_process;
        std::unordered_set<int> fd_check_data_incoming;

        mutable Lock lock;
        std::unordered_map<int, responce> actions;
        std::unordered_map<int, message_helper> messages;
        std::unordered_map<int, secure> encryption_data;

        void get_actions(std::unordered_map<int, e_multithreaded::responce>& _actions)
        {
            WriteLock _{ lock };
            std::swap(actions, _actions);
        }

        bool is_action_ready() const
        {
            ReadLock _{ lock };
            return !actions.empty();
        }

        std::unordered_set<int> get_fd_check_data_incoming()
        {
            std::unordered_set<int> _fd_check_data_incoming;
            std::swap(fd_check_data_incoming, _fd_check_data_incoming);
            return _fd_check_data_incoming;
        }

        bool is_in_process(int fd) const
        {
            return fd_in_process.count(fd) != 0;
        }

        void process_ended(int fd)
        {
            fd_in_process.erase(fd);
            fd_check_data_incoming.emplace(fd);
        }

        bool add_to_process(int epoll_fd, int fd)
        {
            if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr) == -1)
            {
                basefunc_std::log("Error to epoll_ctl EPOLL_CTL_DEL, errno: " + std::to_string(errno) + " error_string: " + strerror(errno), "epoll_v2", true, basefunc_std::COLOR::RED_COL);

                responce r;
                r.status = todo::remove;
                WriteLock _{ lock };
                actions[fd] = std::move(r);

                return false;
            }

            fd_in_process.emplace(fd);
            fd_check_data_incoming.erase(fd);

            return true;
        }

        void process_finished(int fd, todo status, std::string&& rest_of_message, std::size_t t_size_send = 0)
        {
            responce r;

#ifdef epoll_mt_parser
            if (status == todo::ok_read)
            {
                secure cr;
                message_helper _m;
                bool is_from_cache { false };
                {
                    ReadLock _{ lock };
                    auto f = encryption_data.find(fd);
                    if (f != encryption_data.end())
                    {
                        cr = f->second;
                    }
                    auto fm = messages.find(fd);
                    if (fm != messages.end())
                    {
                        _m = fm->second;
                        is_from_cache = true;
                    }
                }

                if (!cr.aes_key.empty())
                {
                    std::vector<std::string> _compressed = epoll_v2::process_read2(_m, rest_of_message.data(), rest_of_message.size(), det_string);
                    for (auto& str : _compressed)
                    {
                        str = cryptopp_::decrypt(str, cr.aes_key, cr.aes_iv, 32, false);
                        if (cr.is_gost_enc) str = gost::g89().decrypt(str, cr.aes_key + cr.aes_iv);

                        responce::message m { std::move(str) };

                        r.rest_of_message.push_back(std::move(m));
                    }

                    if (!_m.del_message.empty() || !_m.message.empty())
                    {
                        WriteLock _{ lock };
                        messages[fd] = std::move(_m);
                    }
                    else if (is_from_cache)
                    {
                        WriteLock _{ lock };
                        messages.erase(fd);
                    }

                    status = todo::ok_parsed;
                }
            }
#endif

            r.status = status;

            if (status != todo::ok_parsed)
            {
                responce::message m { std::move(rest_of_message), t_size_send };

                r.rest_of_message.push_back(std::move(m));
            }

            {
                WriteLock _{ lock };
                if (status == todo::remove) actions[fd] = std::move(r);
                else actions.insert( std::move(std::pair<int, responce>(fd, std::move(r))) );
            }

            eventfd_write(eventfd_fd, 1);
        }

        void erase(int fd)
        {
            WriteLock _{ lock };
            actions.erase(fd);
            messages.erase(fd);
            encryption_data.erase(fd);
        }

        void clear()
        {
            WriteLock _{ lock };
            actions.clear();
            messages.clear();
            encryption_data.clear();
        }

        void set_encryption_data(int fd, const secure& _s)
        {
            WriteLock _{ lock };
            encryption_data[fd] = _s;
        }

        void set_eventfd_fd(std::int32_t eventfd_fd_)
        {
            eventfd_fd = eventfd_fd_;
        }

    private:
        std::int32_t eventfd_fd = 0;

    } multithreaded;
#endif
};

#endif // epoll_v2_H
