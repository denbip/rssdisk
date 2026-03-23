#ifndef tcp_client_H
#define tcp_client_H

#include <netinet/tcp.h>
#include <string>
#include <thread>
#include <atomic>
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
#include <functional>
#include <deque>
#include <condition_variable>

#include "../basefunc_std.h"
#include "../cryptopp_.h"
#include "../DH/dh.h"
#include "../bitbase.h"
#include "../timer.h"
#include "../commpression_zlib.h"
#include "../md5.h"
#include "../thread_timer.h"
#include "../gost_28147_89/gost_89.h"
#include "../network/network_std.h"
#include "../thread_worker.h"

class tcp_client
{
public:
    enum class socket_status
    {
        connected,
        not_connected,
        waiting_for_encryption_established,
        waiting_for_auth
    };
    enum class status
    {
        ok,
        not_connected,
        waiting_for_encryption_established,
        waiting_for_auth,
        waiting_for_previous_responce,
        error
    };

    typedef std::function<void(const std::string& str)> read_callback;
    typedef std::function<void(const std::string& ip, const socket_status& old_status, const socket_status& new_status)> status_callback;
    typedef std::function<void(std::int32_t identy, const status& sts)> send_callback;

    class async_send_item
    {
    public:
        async_send_item(const std::string& msg_) : msg(msg_) { }
        async_send_item(const std::string& msg_, std::int32_t identy_, send_callback c_) : msg(msg_), identy(identy_), c(c_) { }
        async_send_item(std::string&& msg_, std::int32_t identy_, send_callback c_) : msg(std::move(msg_)), identy(identy_), c(c_) { }

        void unset_callback()
        {
            std::lock_guard<std::mutex> _{ lock };
            c = nullptr;
        }

        void execute_callback(const status& sts)
        {
            std::lock_guard<std::mutex> _{ lock };
            if (c) c(identy, sts);
        }

        void unset_awaiting_send()
        {
            awaiting_send.store(false, std::memory_order_release);
        }

        bool get_awaiting_send()
        {
            return awaiting_send.load(std::memory_order_acquire);
        }

        inline std::string get_msg() const { return msg; }
        inline std::int32_t get_identy() const { return identy; }

    private:
        send_callback c = nullptr;

        std::mutex lock;
        std::string msg;
        std::int32_t identy = 0;
        std::atomic_bool awaiting_send = ATOMIC_VAR_INIT(true);
    };

    tcp_client(const std::string ip_, std::int32_t port_, std::int32_t timeout_sec_, const std::string& det_string_,
               const std::string& auth_string_, const std::string& echo_string_, bool is_aes_, std::int32_t network_group_, bool __can_use_in_any_rp);
    virtual ~tcp_client();

    void start();
    void stop();

    void restart();

    std::uint64_t set_read_callback(read_callback c_);
    void unset_read_callback(uint64_t index);

    std::uint64_t set_status_callback(status_callback c_);
    void unset_status_callback(std::uint64_t index);

    bool can_use_in_any_rp() const;
    void set_use_gost_encryption(bool _en);

    tcp_client::status send(const std::string& msg, bool aes_en = false, bool check_state = true);

    void send_async(std::shared_ptr<async_send_item> i);

    tcp_client::socket_status get_socket_state();

    std::int32_t get_identy();
    std::int32_t get_network_group() const { return network_group; }
    std::string get_ip() const { return ip; }
    std::uint32_t get_uip() const { return uip; }

    void set_enabled_send_recv_control(bool _b) { enabled_send_recv_control = _b; }

    void set_default_prime_number(const std::string& _s) { default_prime_number = _s; }

    void set_skip_aes(bool s) { skip_aes.store(s, std::memory_order_release); }

    void listen_events(const std::unordered_set<std::int32_t>& _event_listen_types, std::int32_t _read_past_data_sec)
    {
        is_event_listener = true;
        event_listen_types = _event_listen_types;
        read_past_data_sec = _read_past_data_sec;
        stop();
        start();
    }

    //timeout detector
    struct timeout_detector
    {
        typedef std::function<void()> state_blocked_callback;

        bool is_timeout_detected() const;
        void timeout_detected();
        void reset_timeout();

        void set_state_blocked_callback_callback(const state_blocked_callback _c) { c = _c; }
        void set_count_timeouts(std::int32_t _count_timeouts) { count_timeouts = _count_timeouts; }
        void set_minutes_timeout(std::int32_t _minutes_timeout) { minutes_timeout = _minutes_timeout; }

    private:
        std::atomic_int _timeout_detector = ATOMIC_VAR_INIT(0);
        mutable std::mutex _timeout_lock;
        std::chrono::steady_clock::time_point _timeout_disable_until = std::chrono::steady_clock::now();

        std::int32_t count_timeouts = 10;
        std::int32_t minutes_timeout = 60;

        state_blocked_callback c = nullptr;

    } timeout;

protected:
    virtual void read_bytes(ssize_t n, char* buffer);

private:
    class socket_client
    {
    public:
        std::vector<char> message;
        std::vector<char> del_message;
        std::int32_t seek_delimeter = -1;

        bool is_aes = false;
        bool is_gost_enc = false;
        std::string a;
        std::string p;

        void clear(bool all = false)
        {
            message.clear();
            del_message.clear();
            seek_delimeter = -1;

            if (all)
            {
                std::lock_guard<std::mutex> _{ lock };
                aes_key_iv.first.clear();
                aes_key_iv.second.clear();

                is_aes = false;
                is_gost_enc = false;
                a.clear();
                p.clear();
            }
        }

        std::pair<std::string, std::string> get_aes_key_iv()
        {
            std::lock_guard<std::mutex> _{ lock };
            return aes_key_iv;
        }

        void set_aes_key_iv(const std::string& key, const std::string& iv)
        {
            std::lock_guard<std::mutex> _{ lock };
            aes_key_iv.first = key;
            aes_key_iv.second = iv;
        }

    private:
        std::mutex lock;
        std::pair<std::string, std::string> aes_key_iv;
    };

    std::string det_string;
    std::string auth_string;
    std::string echo_string;
    std::int32_t network_group;
    bool is_aes = false;
    bool is_gost_enc = false;
    bool _can_use_in_any_rp = false;

    std::atomic_bool skip_aes = ATOMIC_VAR_INIT(false);

    std::atomic<time_t> echo_guard;

    std::atomic_bool is_running = ATOMIC_VAR_INIT(false);
    std::int32_t fd = 0;
    std::int32_t port = 0;
    std::string ip;
    std::uint32_t uip = 0;
    std::int32_t timeout_sec = 0;
    std::thread read_thread;
    ::thread::timer read_thread_killer;
    std::mutex lock_send;
    socket_client cl;

    std::thread thread_async_send;
    std::mutex lock_async_send;
    std::condition_variable cond_async_send;
    std::deque<std::shared_ptr<async_send_item> > deque_async_send;

    std::mutex lock_read_send;

    void set_socket_state(socket_status s_);

    std::mutex lock_state;
    socket_status sock_sts;

    std::mutex lock_read_callback;
    std::map<std::uint64_t, read_callback> r_callbacks;
    std::uint64_t counter_r_callbacks = 0;

    std::mutex lock_status_callback;
    std::map<std::uint64_t, status_callback> s_callbacks;
    std::uint64_t counter_s_callbacks = 0;

    std::atomic<std::uint64_t> connection_cnt = ATOMIC_VAR_INIT(0);

    std::atomic<std::uint64_t> send_recv_control = ATOMIC_VAR_INIT(0);
    bool enabled_send_recv_control = false;

    bool connect();
    void read();
    void close(int32_t &f_);
    void send_async_();
    void set_event_listener();

    cryptopp_ CRYPTOPP;

    bool is_event_listener = false;
    std::unordered_set<std::int32_t> event_listen_types;
    std::int32_t read_past_data_sec = 0;

    std::string default_prime_number;
};

#endif // tcp_client_H
