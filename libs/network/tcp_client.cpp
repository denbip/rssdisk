#include "tcp_client.hpp"

tcp_client::tcp_client(const std::string ip_, std::int32_t port_, int32_t timeout_sec_, const std::string &det_string_,
                       const std::string &auth_string_, const std::string &echo_string_, bool is_aes_, int32_t network_group_, bool __can_use_in_any_rp) :
    ip(ip_), port(port_), timeout_sec(timeout_sec_), det_string(det_string_), auth_string(auth_string_), echo_string(echo_string_),
    is_aes(is_aes_), network_group(network_group_), _can_use_in_any_rp(__can_use_in_any_rp), sock_sts(socket_status::not_connected)
{
    uip = network_std::inet_aton(ip);
}

tcp_client::~tcp_client()
{
    stop();
}

bool tcp_client::connect()
{
#ifdef tcp_client_log_on_connect
    basefunc_std::log("Conneting to " + ip, "tcp_client");
#endif

    set_socket_state(socket_status::not_connected);
    close(fd);
    ++connection_cnt;

    cl.clear(true);

    struct sockaddr_in client;
    bzero(&client, sizeof(client));

    inet_pton(AF_INET, ip.c_str(), &(client.sin_addr));
    client.sin_family = AF_INET;
    client.sin_port = htons(port);

    fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0)
    {
        basefunc_std::cout("Error creating socket: " + ip, "tcp_client::connect");
        return false;
    }

    /*int nodelay =1;
    if (setsockopt(fd, SOL_TCP, TCP_NODELAY, (void *)&nodelay, sizeof(nodelay)) < 0)
    {
        close(fd);
        basefunc_std::cout("Set Soct opt nodelay failed", "tcp_client::connect");
        return false;
    }*/

    timeval timeout;
    timeout.tv_sec = timeout_sec;
    timeout.tv_usec = 0;

    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char *) &timeout, sizeof(timeout)) < 0)
    {
        close(fd);
        basefunc_std::cout("Set Soct opt failed", "tcp_client::connect");
        return false;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char *) &timeout, sizeof(timeout)) < 0)
    {
        close(fd);
        basefunc_std::cout("Set Soct opt failed", "tcp_client::connect");
        return false;
    }

    if (::connect(fd, (struct sockaddr *)&client, sizeof(client)) < 0)
    {
        close(fd);

#ifndef disable_cout_for_debug
        basefunc_std::cout("Could not connect to Server URL: " + ip + ":" + std::to_string(port) + " error " + std::to_string(errno) + " " + strerror(errno), "tcp_client::connect", basefunc_std::COLOR::RED_COL);
#endif
        return false;
    }

    int val = fcntl(fd, F_GETFL, 0);
    if (fcntl(fd, F_SETFL, val | O_NONBLOCK) < 0)
    {
        basefunc_std::cout("Error set non_blocking socket: " + ip, "tcp_client::connect");
        return false;
    }

    if (is_aes)
    {
        std::string p_ = default_prime_number.empty() ? cryptopp_::generate_prime_number(1024) : default_prime_number;

        std::string a_;
        const static std::vector<std::string> gs { "2", "3", "5", "7" };
        for (int i = 0; i < 100; ++i) a_ += std::to_string(basefunc_std::rand(1000000000, std::numeric_limits<int>::max()));

        std::string g_ { gs[basefunc_std::rand(0, 3)] };

        DH_BIG::DH g(g_.c_str(), 10);
        DH_BIG::DH p(p_.c_str(), 10);
        DH_BIG::DH a(a_.c_str(), 10);

        DH_BIG::DH A = DH_BIG::DH::pow(g, a, p);

        std::string A_str = A.get_string();
        std::string sz_p = bitbase::numeric_to_chars(p_.size(), 4);

        sz_p += g_ + p_ + A_str;
        sz_p = basefunc_std::compress_number(sz_p);
        sz_p = commpression_zlib::compress_string(sz_p);

        if (send(sz_p, false, false) != status::ok) return false;

        cl.a = a_;
        cl.p = p_;

        set_socket_state(socket_status::waiting_for_encryption_established);

        //basefunc_std::cout("aes encryption started to creating", "tcp_client::connect");
    }
    else
    {
        if (!auth_string.empty())
        {
            status s = send(auth_string, false, false);
            if (s == status::ok)
            {
                set_socket_state(socket_status::connected);
                set_event_listener();
            }
            else
            {
                return false;
            }
        }
        else
        {
            set_socket_state(socket_status::connected);
            set_event_listener();
        }
    }

    return true;
}

void tcp_client::set_event_listener()
{
    if (is_event_listener)
    {
        status s = send("1300000" + std::to_string(read_past_data_sec) + "," + basefunc_std::get_string_from_set(event_listen_types), false, false);
        if (s != status::ok)
        {
            basefunc_std::cout("send signal to listen events has been failed", "tcp_client::connect", basefunc_std::COLOR::RED_COL);
            stop();
            start();
        }
        else
        {
            //basefunc_std::cout("send signal to listen events", "tcp_client::connect");
        }
    }
}

void tcp_client::start()
{
    if (!is_running.load(std::memory_order_acquire))
    {
        is_running.store(true, std::memory_order_release);
        read_thread = std::thread([this]()
        {
            read();
        });
        thread_async_send = std::thread([this]()
        {
            send_async_();
        });
    }
}

void tcp_client::stop()
{
    is_running.store(false, std::memory_order_release);
    read_thread_killer.kill();
    cond_async_send.notify_all();
    if (read_thread.joinable()) read_thread.join();
    if (thread_async_send.joinable()) thread_async_send.join();
    close(fd);
}

void tcp_client::restart()
{
    stop();
    start();
}

void tcp_client::read()
{
    connect();

    const std::int32_t buffer_read_size = 1024 * 100;
    char buffer[buffer_read_size];

    //int counter_check_ms { 0 };

    int is_echo_enabled = !echo_string.empty() ? 100 : -1;
    echo_guard.store(time(0), std::memory_order_release);

    while (is_running.load(std::memory_order_acquire))
    {
        if (is_echo_enabled != -1)
        {
            if (is_echo_enabled == 0)
            {
                is_echo_enabled = 100;
                if (time(0) - echo_guard.load(std::memory_order_acquire) > 60)
                {
                    basefunc_std::cout("Echo guard detected unalived connection, closing connection. " + ip + ":" + std::to_string(port) + ", close fd: " + std::to_string(fd), "tcp_client::read", basefunc_std::COLOR::RED_COL);
                    echo_guard.store(time(0), std::memory_order_release);
                    close(fd);
                }
                /*else
                {
                    if (ip == "192.168.212.27") std::cout << ip << " ------------------------------- " << (time(0) - echo_guard.load(std::memory_order_acquire)) << std::endl;
                }*/
            }
            else
            {
                --is_echo_enabled;
            }
        }

        //wait data on input available
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(fd, &read_fds);
        select(fd + 1, &read_fds, nullptr, nullptr, &tv);

        //read data
        //timer tm;
        bzero(buffer, buffer_read_size);
        ssize_t n = ::recv(fd, buffer, buffer_read_size, MSG_NOSIGNAL);
        //if (n > 100) tm.cout_micro("recv");
//if (ip == "10.153.23.62" && port == 3558) std::cout << (ip + ":" + std::to_string(port)) << " " << n << std::endl;
        if (n < 0)
        {
            int error = 0;
            socklen_t len = sizeof(error);
            int retval = getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len);
            if (retval != 0)
            {
                //basefunc_std::cout(std::string("getsockopt error getting ") + strerror(retval), "tcp_client::read", basefunc_std::COLOR::RED_COL);
                if (!connect()) read_thread_killer.wait_for(std::chrono::seconds(1));
                continue;
            }

            if (error != 0)
            {
                basefunc_std::cout(std::string("Socket error ") + strerror(error), "tcp_client::read", basefunc_std::COLOR::RED_COL);
                if (!connect()) read_thread_killer.wait_for(std::chrono::seconds(1));
                continue;
            }

            if (errno == EWOULDBLOCK)
            {
                /*if (counter_check_ms == 0) std::this_thread::sleep_for(std::chrono::milliseconds(10));
                else
                {
                    --counter_check_ms;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }*/
                continue;
            }

            if (!connect()) read_thread_killer.wait_for(std::chrono::seconds(1));
            continue;
        }
        else if (n == 0)
        {
            if (!connect()) read_thread_killer.wait_for(std::chrono::seconds(1));
        }
        else if (n > 0)
        {
            //timer tm;
            read_bytes(n, buffer);
            //if (n > 100) tm.cout_micro("read_bytes");
        }
    }
}

void tcp_client::read_bytes(ssize_t n, char *buffer)
{
    /*static thread_local thread_pool::thread_pool _t { 1, false };

    while (_t.get_count_works() != 0) std::this_thread::sleep_for(std::chrono::microseconds(10));

    char* b = new char[1024 * 100];
    memcpy(b, buffer, 1024 * 100);

    _t.push_back([this, n, b]()
    {
        std::function<void(std::int32_t)> f_read;
        f_read = [this, &n, &b, &f_read](std::int32_t i)
        {
            std::int32_t delimeter_pos = cl.seek_delimeter + 1;
            if (delimeter_pos < det_string.size())
            {
                char del_char = det_string[delimeter_pos];
                for (; i < n; ++i)
                {
                    char ch = b[i];
                    if (ch == del_char)
                    {
                        ++cl.seek_delimeter;
                        ++i;
                        cl.del_message.push_back(ch);
                        f_read(i);
                        break;
                    }
                    else
                    {
                        if (!cl.del_message.empty())
                        {
                            cl.message.insert(cl.message.end(), cl.del_message.begin(), cl.del_message.end());
                            cl.del_message.clear();
                            cl.seek_delimeter = -1;
                            del_char = det_string[0];
                        }

                        //check on delimeter
                        if (ch == del_char)
                        {
                            ++cl.seek_delimeter;
                            ++i;
                            cl.del_message.push_back(ch);
                            f_read(i);
                            break;
                        }
                        else
                        {
                            cl.message.push_back(ch);
                        }
                    }
                }
            }
            else //message has been complited
            {
                if (!cl.message.empty())
                {
                    std::string str(cl.message.begin(), cl.message.end());

                    bool ok { true };
                    if (is_aes)
                    {
                        if (!cl.is_aes)
                        {
                            if (cl.get_aes_key_iv().first.empty())
                            {
                                std::string B_ = basefunc_std::uncompress_number(str);

                                DH_BIG::DH p(cl.p.c_str(), 10);
                                DH_BIG::DH a(cl.a.c_str(), 10);
                                DH_BIG::DH B(B_.c_str(), 10);

                                DH_BIG::DH K = DH_BIG::DH::pow(B, a, p);
                                std::string key = MD5(K.get_string()).hexdigest();

                                cl.set_aes_key_iv(key, key.substr(0, 16));

                                if (send("ok_aes", true, false) != status::ok) return false;
                            }
                            else
                            {
                                auto k = cl.get_aes_key_iv();
                                str = CRYPTOPP.decrypt(str, k.first, k.second, 32, false);

                                if (str.compare("ok_aes") == 0)
                                {
                                    cl.is_aes = true;
                                    if (is_gost_enc)
                                    {
                                        cl.is_gost_enc = true;
                                        //basefunc_std::cout("Encryption is established", "tcp_client::gost");
                                    }
                                    //basefunc_std::cout("Encryption is established", "tcp_client::aes");
                                    if (!auth_string.empty())
                                    {
                                        status s = send(auth_string, false, false);
                                        if (s == status::ok)
                                        {
                                            set_socket_state(socket_status::connected);
                                            set_event_listener();
                                        }
                                        else
                                        {
                                            close(fd);
                                        }
                                    }
                                    else
                                    {
                                        set_socket_state(socket_status::connected);
                                        set_event_listener();
                                    }
                                }
                                else
                                {
                                    close(fd);
                                }
                            }

                            ok = false;
                        }
                        else
                        {
                            auto k = cl.get_aes_key_iv();
                            std::string dec_str = CRYPTOPP.decrypt(str, k.first, k.second, 32, false, !skip_aes.load(std::memory_order_acquire));

                            if (!str.empty() && dec_str.empty() && skip_aes.load(std::memory_order_acquire))
                            {

                            }
                            else
                            {
                                str = std::move(dec_str);
                                if (cl.is_gost_enc) str = gost::g89().decrypt(str, k.first + k.second);
                            }
                        }
                    }

                    if (ok)
                    {
                        bool is_echo { false };
                        if (!echo_string.empty())
                        {
                            if (echo_string.size() == str.size() && echo_string.compare(str) == 0) is_echo = true;
                        }

                        if (!is_echo)
                        {
                            if (enabled_send_recv_control)
                            {
                                --send_recv_control;
                            }

                            std::lock_guard<std::mutex> _{ lock_read_callback };
                            for (auto& c : r_callbacks) c.second(str);
                        }

                        if (!echo_string.empty()) //tcp alive control
                        {
                            echo_guard.store(time(0), std::memory_order_release);
                        }
                    }
                }
                cl.clear();
                f_read(i);
            }
        };

        f_read(0);
        delete[] b;
    });*/

    static thread_local std::string m;
    static thread_local std::size_t s = 0;
    m.append(buffer, n);

    auto pr = [this](std::string str) -> void
    {
        bool ok { true };
        if (is_aes)
        {
            if (!cl.is_aes)
            {
                if (cl.get_aes_key_iv().first.empty())
                {
                    std::string B_ = basefunc_std::uncompress_number(str);

                    DH_BIG::DH p(cl.p.c_str(), 10);
                    DH_BIG::DH a(cl.a.c_str(), 10);
                    DH_BIG::DH B(B_.c_str(), 10);

                    DH_BIG::DH K = DH_BIG::DH::pow(B, a, p);
                    std::string key = MD5(K.get_string()).hexdigest();

                    cl.set_aes_key_iv(key, key.substr(0, 16));

                    if (send("ok_aes", true, false) != status::ok) return;
                }
                else
                {
                    auto k = cl.get_aes_key_iv();
                    str = CRYPTOPP.decrypt(str, k.first, k.second, 32, false);

                    if (str.compare("ok_aes") == 0)
                    {
                        cl.is_aes = true;
                        if (is_gost_enc)
                        {
                            cl.is_gost_enc = true;
                            //basefunc_std::cout("Encryption is established", "tcp_client::gost");
                        }
                        //basefunc_std::cout("Encryption is established", "tcp_client::aes");
                        if (!auth_string.empty())
                        {
                            status s = send(auth_string, false, false);
                            if (s == status::ok)
                            {
                                set_socket_state(socket_status::connected);
                                set_event_listener();
                            }
                            else
                            {
                                close(fd);
                            }
                        }
                        else
                        {
                            set_socket_state(socket_status::connected);
                            set_event_listener();
                        }
                    }
                    else
                    {
                        close(fd);
                    }
                }

                ok = false;
            }
            else
            {
                auto k = cl.get_aes_key_iv();
                std::string dec_str = CRYPTOPP.decrypt(str, k.first, k.second, 32, false, !skip_aes.load(std::memory_order_acquire));

                if (!str.empty() && dec_str.empty() && skip_aes.load(std::memory_order_acquire))
                {

                }
                else
                {
                    str = std::move(dec_str);
                    if (cl.is_gost_enc) str = gost::g89().decrypt(str, k.first + k.second);
                }
            }
        }

        if (ok)
        {
            bool is_echo { false };
            if (!echo_string.empty())
            {
                if (echo_string.size() == str.size() && echo_string.compare(str) == 0) is_echo = true;
            }

            if (!is_echo)
            {
                if (enabled_send_recv_control)
                {
                    --send_recv_control;
                }

                std::lock_guard<std::mutex> _{ lock_read_callback };
                for (auto& c : r_callbacks) c.second(str);
            }

            if (!echo_string.empty()) //tcp alive control
            {
                echo_guard.store(time(0), std::memory_order_release);
            }
        }
    };

    auto check = [this, &m, &pr, &s]() -> bool
    {
        auto f_del = m.find(det_string, s);
        if (f_del != std::string::npos)
        {
            s = 0;

            if (f_del == 0)
            {
                m.erase(0, det_string.size());
            }
            else
            {
                if (f_del + det_string.size() == m.size()) //full message
                {
                    m.erase(f_del, det_string.size());
                    pr(m);

                    //std::cout << "one " << m.size() << std::endl;
                    m.clear();
                }
                else //+ next message
                {
                    pr(m.substr(0, f_del));
                    m.erase(0, f_del + det_string.size());

                    //std::cout << "next " << m << std::endl;
                }
            }

            return true;
        }

        if (m.size() > det_string.size()) s = m.size() - det_string.size();

        return false;
    };

    while (check());

    return;

    /*std::function<void(std::int32_t)> f_read;
    f_read = [this, &n, &buffer, &f_read](std::int32_t i)
    {
        std::int32_t delimeter_pos = cl.seek_delimeter + 1;
        if (delimeter_pos < det_string.size())
        {
            char del_char = det_string[delimeter_pos];
            for (; i < n; ++i)
            {
                char ch = buffer[i];
                if (ch == del_char)
                {
                    ++cl.seek_delimeter;
                    ++i;
                    cl.del_message.push_back(ch);
                    f_read(i);
                    break;
                }
                else
                {
                    if (!cl.del_message.empty())
                    {
                        cl.message.insert(cl.message.end(), cl.del_message.begin(), cl.del_message.end());
                        cl.del_message.clear();
                        cl.seek_delimeter = -1;
                        del_char = det_string[0];
                    }

                    //check on delimeter
                    if (ch == del_char)
                    {
                        ++cl.seek_delimeter;
                        ++i;
                        cl.del_message.push_back(ch);
                        f_read(i);
                        break;
                    }
                    else
                    {
                        cl.message.push_back(ch);
                    }
                }
            }
        }
        else //message has been complited
        {
            if (!cl.message.empty())
            {
                std::string str(cl.message.begin(), cl.message.end());

                bool ok { true };
                if (is_aes)
                {
                    if (!cl.is_aes)
                    {
                        if (cl.get_aes_key_iv().first.empty())
                        {
                            std::string B_ = basefunc_std::uncompress_number(str);

                            DH_BIG::DH p(cl.p.c_str(), 10);
                            DH_BIG::DH a(cl.a.c_str(), 10);
                            DH_BIG::DH B(B_.c_str(), 10);

                            DH_BIG::DH K = DH_BIG::DH::pow(B, a, p);
                            std::string key = MD5(K.get_string()).hexdigest();

                            cl.set_aes_key_iv(key, key.substr(0, 16));

                            if (send("ok_aes", true, false) != status::ok) return false;
                        }
                        else
                        {
                            auto k = cl.get_aes_key_iv();
                            str = CRYPTOPP.decrypt(str, k.first, k.second, 32, false);

                            if (str.compare("ok_aes") == 0)
                            {
                                cl.is_aes = true;
                                if (is_gost_enc)
                                {
                                    cl.is_gost_enc = true;
                                    //basefunc_std::cout("Encryption is established", "tcp_client::gost");
                                }
                                //basefunc_std::cout("Encryption is established", "tcp_client::aes");
                                if (!auth_string.empty())
                                {
                                    status s = send(auth_string, false, false);
                                    if (s == status::ok)
                                    {
                                        set_socket_state(socket_status::connected);
                                        set_event_listener();
                                    }
                                    else
                                    {
                                        close(fd);
                                    }
                                }
                                else
                                {
                                    set_socket_state(socket_status::connected);
                                    set_event_listener();
                                }
                            }
                            else
                            {
                                close(fd);
                            }
                        }

                        ok = false;
                    }
                    else
                    {
                        auto k = cl.get_aes_key_iv();
                        std::string dec_str = CRYPTOPP.decrypt(str, k.first, k.second, 32, false, !skip_aes.load(std::memory_order_acquire));

                        if (!str.empty() && dec_str.empty() && skip_aes.load(std::memory_order_acquire))
                        {

                        }
                        else
                        {
                            str = std::move(dec_str);
                            if (cl.is_gost_enc) str = gost::g89().decrypt(str, k.first + k.second);
                        }
                    }
                }

                if (ok)
                {
                    bool is_echo { false };
                    if (!echo_string.empty())
                    {
                        if (echo_string.size() == str.size() && echo_string.compare(str) == 0) is_echo = true;
                    }

                    if (!is_echo)
                    {
                        if (enabled_send_recv_control)
                        {
                            --send_recv_control;
                        }

                        std::lock_guard<std::mutex> _{ lock_read_callback };
                        for (auto& c : r_callbacks) c.second(str);
                    }

                    if (!echo_string.empty()) //tcp alive control
                    {
                        echo_guard.store(time(0), std::memory_order_release);
                    }
                }
            }
            cl.clear();
            f_read(i);
        }
    };

    f_read(0);*/
}

tcp_client::status tcp_client::send(const std::string& msg, bool aes_en, bool check_state)
{
    if (check_state)
    {
        socket_status state = get_socket_state();
        if (state == socket_status::not_connected) return status::not_connected;
        else if (state == socket_status::waiting_for_encryption_established) return status::waiting_for_encryption_established;
        else if (state == socket_status::waiting_for_auth) return status::waiting_for_auth;
    }

    std::uint64_t con_cnt = connection_cnt.load(std::memory_order_acquire);

    std::lock_guard<std::mutex> _{ lock_send };

    auto f_send = [this](const std::string& d) -> bool
    {
        auto sz = d.size();
        std::int32_t sz_sended { 0 };

        while (sz > 0)
        {
            std::string s = d.substr(sz_sended);
            ssize_t n = ::send(fd, s.data(), s.size(), MSG_NOSIGNAL);

            if (n == 0) //
            {
                break;
            }
            else if (n < 0)
            {
                if (errno == EWOULDBLOCK || errno == EAGAIN)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                else
                {
                    break;
                }
            }

            sz -= n;
            sz_sended += n;
        }

        return sz == 0;
    };

    bool ok { true };
    //ok = f_send(det_string);
    if (ok)
    {
        if (cl.is_aes || aes_en)
        {
            std::pair<std::string, std::string> aes_s = cl.get_aes_key_iv();
            if (cl.is_gost_enc)
            {
                std::string msg_gost;
                msg_gost.reserve(msg.size());
                msg_gost = gost::g89().crypt(msg, aes_s.first + aes_s.second);
                ok = f_send(det_string + CRYPTOPP.encrypt(msg_gost, aes_s.first, aes_s.second, 32, false) + det_string);
            }
            else
            {
                ok = f_send(det_string + CRYPTOPP.encrypt(msg, aes_s.first, aes_s.second, 32, false) + det_string);
            }
        }
        else
        {
            ok = f_send(det_string + msg + det_string);
        }
    }
    //if (ok) ok = f_send(det_string);

    if (ok && con_cnt == connection_cnt.load(std::memory_order_acquire)) return status::ok;

    socket_status state = get_socket_state();
    if (state == socket_status::not_connected) return status::not_connected;
    else if (state == socket_status::waiting_for_encryption_established) return status::waiting_for_encryption_established;
    else if (state == socket_status::waiting_for_auth) return status::waiting_for_auth;
    else return status::error;
}

void tcp_client::set_socket_state(socket_status s_)
{
    socket_status old;
    {
        std::lock_guard<std::mutex> _{ lock_state };
        old = sock_sts;
        sock_sts = s_;
    }

    if (old != s_)
    {
        std::lock_guard<std::mutex> _{ lock_status_callback };
        for (auto& c : s_callbacks)
        {
            c.second(ip, old, s_);
        }
    }
}

void tcp_client::set_use_gost_encryption(bool _en)
{
    is_gost_enc = _en;
}

bool tcp_client::can_use_in_any_rp() const
{
    return _can_use_in_any_rp;
}

tcp_client::socket_status tcp_client::get_socket_state()
{
    std::lock_guard<std::mutex> _{ lock_state };
    return sock_sts;
}

std::uint64_t tcp_client::set_read_callback(read_callback c_)
{
    std::lock_guard<std::mutex> _{ lock_read_callback };
    std::pair<std::map<std::uint64_t, read_callback>::iterator, bool> i;
    do
    {
        ++counter_r_callbacks;
        i = r_callbacks.insert( { counter_r_callbacks, c_ } );
    }
    while (!i.second);
    return counter_r_callbacks;
}

bool tcp_client::timeout_detector::is_timeout_detected() const
{
    std::lock_guard<std::mutex> _{ _timeout_lock };
    return std::chrono::steady_clock::now() < _timeout_disable_until;
}

void tcp_client::timeout_detector::timeout_detected()
{
    std::int32_t r = _timeout_detector.fetch_add(1, std::memory_order_relaxed);
    if (r >= count_timeouts)
    {
        _timeout_disable_until = std::chrono::steady_clock::now() + std::chrono::minutes(minutes_timeout);
        _timeout_detector.store(0, std::memory_order_relaxed);

        if (c != nullptr) c();
    }
}

void tcp_client::timeout_detector::reset_timeout()
{
    _timeout_detector.store(0, std::memory_order_relaxed);
}

void tcp_client::unset_read_callback(std::uint64_t index)
{
    std::lock_guard<std::mutex> _{ lock_read_callback };
    r_callbacks.erase(index);
}

std::int32_t tcp_client::get_identy()
{
    static std::int32_t identy = 10000;
    static std::mutex l;
    std::lock_guard<std::mutex> _{ l };
    ++identy;
    if (identy >= 100000) identy = 10000;
    return identy;
}

std::uint64_t tcp_client::set_status_callback(status_callback c_)
{
    std::lock_guard<std::mutex> _{ lock_status_callback };
    std::pair<std::map<std::uint64_t, status_callback>::iterator, bool> i;
    do
    {
        ++counter_s_callbacks;
        i = s_callbacks.insert( { counter_s_callbacks, c_ } );
    }
    while (!i.second);
    return counter_s_callbacks;
}

void tcp_client::unset_status_callback(std::uint64_t index)
{
    std::lock_guard<std::mutex> _{ lock_status_callback };
    s_callbacks.erase(index);
}

void tcp_client::close(int32_t& f_)
{
    if (f_ != 0) ::close(f_);
    f_ = 0;
}

void tcp_client::send_async(std::shared_ptr<async_send_item> i)
{
    if (enabled_send_recv_control)
    {
        if (send_recv_control != 0)
        {
            i->execute_callback(status::waiting_for_previous_responce);
            return;
        }

        ++send_recv_control;
    }

    std::lock_guard<std::mutex> _{ lock_async_send };
    deque_async_send.emplace_back(i);
    cond_async_send.notify_all();
}

void tcp_client::send_async_()
{
    while (is_running.load(std::memory_order_release))
    {
        std::unique_lock<std::mutex> lock { lock_async_send };
        if (cond_async_send.wait_for(lock, std::chrono::milliseconds(100), [this](){ return !deque_async_send.empty() || !is_running.load(std::memory_order_release); }))
        {
            if (!deque_async_send.empty())
            {
                std::shared_ptr<async_send_item> i = deque_async_send.front();
                deque_async_send.pop_front();
                lock.unlock();

                if (i->get_awaiting_send())
                {
                    tcp_client::status sts = send(i->get_msg());
                    /*sts = send(i->get_msg());
                    sts = send(i->get_msg());
                    sts = send(i->get_msg());
                    sts = send(i->get_msg());
                    sts = send(i->get_msg());
                    sts = send(i->get_msg());
                    sts = send(i->get_msg());
                    sts = send(i->get_msg());*/
                    if (i->get_awaiting_send())
                    {
                        i->execute_callback(sts);
                    }
                }
            }
        }
    }
}
