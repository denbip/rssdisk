#include "epoll_v2.hpp"

epoll_v2::epoll_v2(int32_t t_pool_size_, string det_string_, std::string echo_string_, bool is_aes_, bool is_gost_enc_) :
    t_pool(t_pool_size_), det_string(det_string_), echo_string(echo_string_), is_aes(is_aes_), is_gost_enc(is_gost_enc_)
{
#ifdef epoll_multithreaded
    multithreaded.det_string = det_string;
#endif
}

epoll_v2::~epoll_v2()
{
    t_pool.wait();
}

bool epoll_v2::listen(const std::string& ip, int port, std::atomic_bool& is_running)
{
    int sock;
    if (!create_socket(sock)) return false;
    if (!bind(sock, ip, port)) return false;
    if (!set_nonblocking(sock)) return false;

    if (::listen(sock, SOMAXCONN) < 0)
    {
        basefunc_std::log("Cant listen, errno: " + std::to_string(errno) + " error_string: " + strerror(errno), "epoll_v2", true, basefunc_std::COLOR::RED_COL);
        return false;
    }

    // create the epoll socket
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
    {
        basefunc_std::log("Cant create epoll_fd, errno: " + std::to_string(errno) + " error_string: " + strerror(errno), "epoll_v2", true, basefunc_std::COLOR::RED_COL);
        return false;
    }

    // mark the server socket for reading, and become edge-triggered
    memset(&event, 0, sizeof(event));
    event.data.fd = sock;
    event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock, &event) == -1)
    {
        basefunc_std::log("Cant epoll_ctl server socket, errno: " + std::to_string(errno) + " error_string: " + strerror(errno), "epoll_v2", true, basefunc_std::COLOR::RED_COL);
        return false;
    }

    //create eventfd
    int _eventfd = eventfd(0, EFD_NONBLOCK);
    struct epoll_event evnt = {0};
    evnt.data.fd = _eventfd;
    evnt.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, _eventfd, &evnt) == -1)
    {
        basefunc_std::log("Cant epoll_ctl eventfd, errno: " + std::to_string(errno) + " error_string: " + strerror(errno), "epoll_v2", true, basefunc_std::COLOR::RED_COL);
        return false;
    }

    responser_.set_eventfd_fd(_eventfd);
#ifdef epoll_multithreaded
    multithreaded.set_eventfd_fd(_eventfd);
#endif

    struct epoll_event *events =(epoll_event*) calloc(MAX_EVENTS, sizeof(event));

    basefunc_std::cout("Listening on port " + std::to_string(port), "epoll_v2");

    while (is_running.load(std::memory_order_acquire))
    {
        int nevents;
        int ms;

        for (ms = 0; ms < 100; ++ms)
        {
            nevents = epoll_wait(epoll_fd, events, MAX_EVENTS, 100);

#ifdef epoll_multithreaded
            if (nevents != 0 || !responser_.empty() || multithreaded.is_action_ready()) break;
#else
            if (nevents != 0 || !responser_.empty()) break;
#endif
        }

        //std::cout << "epoll_wait nevents " << nevents << std::endl;

        ++ms;
        //ms /= 100;

        if (nevents == -1)
        {
            basefunc_std::log("Error to epoll_wait, errno: " + std::to_string(errno) + " error_string: " + strerror(errno), "epoll_v2", true, basefunc_std::COLOR::RED_COL);
            free(events);
            return false;
        }

#ifdef epoll_multithreaded

        std::time_t t_now { time(0) };

        std::unordered_map<int, e_multithreaded::responce> mt_actions;
        multithreaded.get_actions(mt_actions);
        for (auto& it : mt_actions)
        {
            int fd_r = it.first;
            multithreaded.process_ended(fd_r);

#ifdef debug_epoll_v2
            if (debug_fd == fd_r) timer::print_current_time(std::string("read action " + std::to_string(static_cast<int>(it.second.status))).c_str());
#endif

            switch (it.second.status)
            {
                case e_multithreaded::todo::ok:
                case e_multithreaded::todo::ok_read:
                case e_multithreaded::todo::ok_parsed:
                {
                    std::deque<m_write*>& msgs = awaiting_for_write[fd_r];

                    event.data.fd = fd_r;
                    event.events = EPOLLIN | EPOLLET;
                    if (!msgs.empty())
                    {
                        event.events |= EPOLLOUT;
                    }

                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd_r, &event) == -1)
                    {
                        basefunc_std::log("MT Error to epoll_ctl ADD for fd_client, errno: " + std::to_string(errno) + " error_string: " + strerror(errno), "epoll_v2", true, basefunc_std::COLOR::RED_COL);
                        remove(epoll_fd, fd_r);
                    }

                    if (it.second.status == e_multithreaded::todo::ok_parsed || it.second.status == e_multithreaded::todo::ok_read)
                    {
                        auto f_connection = authorized_clients.find(fd_r);
                        if (f_connection != authorized_clients.end())
                        {
#ifdef debug_epoll_v2
                            if (debug_fd == fd_r) timer::print_current_time(std::string("process_read " + std::to_string(it.second.rest_of_message.size())).c_str());
#endif

                            if (it.second.status == e_multithreaded::todo::ok_parsed) //parsed + decrypted
                            {
                                if (on_message)
                                {
                                    for (auto& it : it.second.rest_of_message)
                                    {
                                        on_message(f_connection->second, std::move(it.d), t_pool, responser_);
                                    }
                                }
                            }
                            else
                            {
                                std::vector<std::string> parsed = process_read2(f_connection->second._m, it.second.rest_of_message[0].d.data(), it.second.rest_of_message[0].d.size(), det_string);

                                for (auto& it : parsed)
                                {
                                    process_message(f_connection->second, std::move(it));
                                }
                            }
#ifdef epoll_v2_control_activity
                            last_activity.data[fd_r] = t_now;
#endif
                        }
                    }

                    break;
                }
                case e_multithreaded::todo::set_epoll_out:
                {
                    std::deque<m_write*>& msgs = awaiting_for_write[fd_r];
                    m_write* front_msg = new m_write(std::move(it.second.rest_of_message[0].d), it.second.rest_of_message[0].cursor);

                    front_msg->is_on_waiting_epollout = true;
                    msgs.emplace_front(front_msg);

                    event.data.fd = fd_r;
                    event.events = EPOLLOUT | EPOLLET;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd_r, &event) == -1)
                    {
                        basefunc_std::log("MT Error to epoll_ctl ADD with OUT for write, errno: " + std::to_string(errno) + " error_string: " + strerror(errno), "epoll_v2", true, basefunc_std::COLOR::RED_COL);
                        remove(epoll_fd, fd_r);
                    }

                    break;
                }
                case e_multithreaded::todo::remove:
                {
                    remove(epoll_fd, fd_r, false);
                    break;
                }
                default: break;
            }
#ifdef debug_epoll_v2
            if (debug_fd == fd_r) timer::print_current_time("end read action");
#endif
        }

        auto f_mt_read = [this, epoll_fd](int read_from_fd) -> void
        {
            if (!multithreaded.is_in_process(read_from_fd))
            {
                if (multithreaded.add_to_process(epoll_fd, read_from_fd))
                {
                    t_pool.push_back([this, read_from_fd]()
                    {
                        e_multithreaded::todo status { e_multithreaded::todo::ok_read };
                        std::string readed_str;

                        const std::int32_t buf_size = 1024;
                        char buf[buf_size];
                        memset(buf, 0x0, buf_size);
                        while (true)
                        {
                            ssize_t nbytes = ::read(read_from_fd, buf, sizeof(buf));
#ifdef debug_epoll_v2
                            if (debug_fd == read_from_fd) timer::print_current_time(std::string("::read nbytes " + std::to_string(nbytes) + " errno " + std::to_string(errno)).c_str());
#endif
                            if (nbytes == -1)
                            {
                                if (errno == EAGAIN || errno == EWOULDBLOCK)  //finished reading data from client
                                {

                                }
                                else
                                {
                                    status = e_multithreaded::todo::remove;
                                    basefunc_std::log("Error to read from client, errno: " + std::to_string(errno) + " error_string: " + strerror(errno), "epoll_v2", true, basefunc_std::COLOR::RED_COL);
                                }

                                break;
                            }
                            else if (nbytes == 0) //client exit
                            {
                                basefunc_std::cout("0 bytes recieved", "epoll_v2", basefunc_std::COLOR::YELLOW_COL);
                                status = e_multithreaded::todo::remove;
                                break;
                            }
                            else //nbytes readed to buf
                            {
                                readed_str.append(buf, nbytes);
                            }
                        }

                        multithreaded.process_finished(read_from_fd, status, std::move(readed_str));
                    });
                }
            }
            else
            {
                basefunc_std::cout("Read is_in_process", "multithreaded", basefunc_std::COLOR::YELLOW_COL);
            }
        };

        //check data incoming while thread-readed
        std::unordered_set<int> check_incoming = multithreaded.get_fd_check_data_incoming();
        for (int fd : check_incoming)
        {
            char b;
            int bt_available = ::recv(fd, &b, 1, MSG_PEEK);
            if (bt_available > 0) //data is available
            {
#ifndef disable_cout_for_debug
                basefunc_std::cout("Data is available for read", "multithreaded");
#endif
#ifdef debug_epoll_v2
                if (debug_fd == fd) timer::print_current_time("read");
#endif
                f_mt_read(fd);
            }
        }
#endif

        for (int i = 0; i < nevents; i++)
        {
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN) && !(events[i].events & EPOLLOUT)))
            {
                basefunc_std::cout("Error poll, closing the connection", "epoll_v2");
                remove(epoll_fd, events[i].data.fd);
                continue;
            }

            if (events[i].data.fd == sock) // server socket; call accept as many times as we can
            {
                while (true)
                {
                    struct sockaddr in_addr;
                    socklen_t in_addr_len = sizeof(in_addr);

                    const static bool use_accept4 { true };

                    int fd_client;
                    if (use_accept4) fd_client = accept4(sock, &in_addr, &in_addr_len, SOCK_NONBLOCK | SOCK_CLOEXEC);
                    else fd_client = accept(sock, &in_addr, &in_addr_len);

                    basefunc_std::cout(std::to_string(fd_client), "accept");

                    if (fd_client == -1)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) // we have processed all of the connections
                        {
                            break;
                        }
                        else
                        {
                            basefunc_std::log("Error to accept, errno: " + std::to_string(errno) + " error_string: " + strerror(errno), "epoll_v2", true, basefunc_std::COLOR::RED_COL);
                            free(events);
                            return false;
                        }
                    }
                    else
                    {
                        /*int nodelay =1;
                        if (setsockopt(fd_client, SOL_TCP, TCP_NODELAY, (void *)&nodelay, sizeof(nodelay)) < 0)
                        {
                            basefunc_std::cout("Set Soct opt nodelay failed", "epoll_v2::connect");
                            break;
                        }*/

                        std::string ip;
                        if (in_addr.sa_family == AF_INET)
                        {
                            sockaddr_in* addr_in = (sockaddr_in*) (&in_addr);
                            ip = inet_ntoa(addr_in->sin_addr);
                        }

                        basefunc_std::cout("Accepted the connection on fd " + std::to_string(fd_client) + " ip " +  ip, "epoll_v2");
                        if (!use_accept4) set_nonblocking(fd_client);
                        event.data.fd = fd_client;
                        event.events = EPOLLIN | EPOLLET;
                        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd_client, &event) == -1)
                        {
                            basefunc_std::log("Error to epoll_ctl for fd_client, errno: " + std::to_string(errno) + " error_string: " + strerror(errno), "epoll_v2", true, basefunc_std::COLOR::RED_COL);
                            free(events);
                            return false;
                        }

#ifdef debug_epoll_v2
                        if (ip == "192.168.201.27")
                        {
                            debug_fd = fd_client;
                        }
#endif

                        socket_client soc_cl;
                        soc_cl.ip = std::move(ip);
                        soc_cl.fd = fd_client;
                        soc_cl.count_accepted_on = ++count_accepted;
                        authorized_clients[fd_client] = std::move(soc_cl);
                        count_connected.store(authorized_clients.size(), std::memory_order_relaxed);

                        awaiting_authorize_clients.emplace(fd_client);

#ifdef epoll_v2_control_activity
                        last_activity.data[fd_client] = t_now;
#endif
                    }
                }
            }
            else
            {
                if ((events[i].events & EPOLLIN) && (events[i].events & EPOLLOUT))
                {
                    basefunc_std::cout("Two events type " + std::to_string(events[i].data.fd), "epoll_v2");
                }

                if (events[i].events & EPOLLIN)
                {
                    if (events[i].data.fd == _eventfd)
                    {
                        eventfd_t val;
                        eventfd_read(_eventfd, &val);
                    }
                    else if (!socket_check(events[i].data.fd))
                    {
                        basefunc_std::log("Error to check socket for reading", "epoll_v2", true, basefunc_std::COLOR::RED_COL);
                        remove(epoll_fd, events[i].data.fd);
                    }
                    else //read to end
                    {
    #ifdef epoll_multithreaded
                        f_mt_read(events[i].data.fd);
    #else
                        const std::int32_t buf_size = 1024;
                        char buf[buf_size];
                        memset(buf, 0x0, buf_size);
                        while (true)
                        {
                            ssize_t nbytes = read(events[i].data.fd, buf, sizeof(buf));
                            if (nbytes == -1)
                            {
                                if (errno == EAGAIN || errno == EWOULDBLOCK)  //finished reading data from client
                                {

                                }
                                else
                                {
                                    remove(epoll_fd, events[i].data.fd);
                                    basefunc_std::log("Error to read from client, errno: " + std::to_string(errno) + " error_string: " + strerror(errno), "epoll_v2", true, basefunc_std::COLOR::RED_COL);
                                }

                                break;
                            }
                            else if (nbytes == 0) //client exit
                            {
                                basefunc_std::cout("0 bytes recieved", "epoll_v2", basefunc_std::COLOR::YELLOW_COL);
                                remove(epoll_fd, events[i].data.fd);
                                break;
                            }
                            else //nbytes readed to buf
                            {
                                auto f_connection = authorized_clients.find(events[i].data.fd);
                                if (f_connection != authorized_clients.end())
                                {
                                    process_read(f_connection->second._m, buf, nbytes, det_string);
                                }
                            }
                        }
    #endif
                    }
                }

                if (events[i].events & EPOLLOUT)
                {
                    if (!socket_check(events[i].data.fd))
                    {
                        basefunc_std::log("Error to check socket for writing", "epoll_v2", true, basefunc_std::COLOR::RED_COL);
                        remove(epoll_fd, events[i].data.fd);
                    }
                    else //write to end and remove EPOLLOUT
                    {
                        std::deque<m_write*>& msgs = awaiting_for_write[events[i].data.fd];
                        if (!msgs.empty())
                        {
                            msgs[0]->is_on_waiting_epollout = false;

                            event.data.fd = events[i].data.fd;
                            event.events = EPOLLIN | EPOLLET;
                            if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, events[i].data.fd, &event) == -1)
                            {
                                basefunc_std::log("Error to epoll_ctl from write, errno: " + std::to_string(errno) + " error_string: " + strerror(errno), "epoll_v2", true, basefunc_std::COLOR::RED_COL);
                            }
                        }
                    }
                }
            }
        }

        //write to awaiting_for_write whatever you want ot send to fd

        //get new events listeners
        std::unordered_map<std::int32_t, std::unordered_set<std::int32_t>> new_events_listeners = responser_.get_new_events_listeners();
        for (const auto& i : new_events_listeners)
        {
            auto f = authorized_clients.find(i.first);
            if (f != authorized_clients.end()) //client are still connected
            {
                events_clients.insert(i);
            }
        }

        //send events
        std::vector<event_edb> new_events = responser_.get_events();
        for (const event_edb& ev : new_events)
        {
            for (const auto& i : events_clients)
            {
                if (i.second.empty() || i.second.count(ev.type) != 0)
                {
                    responser::item::sub s { ev.data };
                    push_message(i.first, std::move(s));
                }
            }
        }

        //send responces
        std::unordered_map<std::int32_t, responser::item> messages_to_send = responser_.get_and_clear();
        for (auto& it : messages_to_send)
        {
            int fd_item = it.first;
            for (auto& it2 : it.second.data)
            {
                std::uint64_t counter = it2.first;
                auto f = authorized_clients.find(fd_item);
                if (f != authorized_clients.end()) //client are still connected
                {
                    socket_client& c = f->second;
                    if (counter == c.count_accepted_on)
                    {
                        for (auto& it3 : it2.second)
                        {
                            if (it3.is_key_generation)
                            {
                                c._s.aes_key = it3.str;
                                c._s.aes_iv = c._s.aes_key.substr(0, 16);
                            }
                            else
                            {
                                push_message(fd_item, std::move(it3));
                            }
                        }
                    }
                }
            }
        }

        //kick unautorized
        if (!awaiting_authorize_clients.empty())
        {
            std::vector<int> for_kick;
            std::vector<int> ok_auth;
            for (int fd : awaiting_authorize_clients)
            {
                auto f = authorized_clients.find(fd);
                if (f != authorized_clients.end())
                {
                    if (f->second.is_authorized)
                    {
                        ok_auth.push_back(fd);
                    }
                    else
                    {
                        f->second.time_to_auth -= ms;
                        if (f->second.time_to_auth <= 0)
                        {
                            for_kick.push_back(fd);
                        }
                    }
                }
                else
                {
                    for_kick.push_back(fd);
                }
            }
            if (!ok_auth.empty())
            {
                for (int f : ok_auth) awaiting_authorize_clients.erase(f);
            }
            if (!for_kick.empty())
            {
                for (int f : for_kick)
                {
#ifdef epoll_multithreaded
                    if (multithreaded.is_in_process(f)) continue;
#endif

                    remove(epoll_fd, f);
                }
            }
        }

        //echo service
        if (!echo_string.empty())
        {
            for (auto& it : authorized_clients)
            {
                int fd_item = it.first;
                socket_client& item = it.second;

                item.time_to_send_test_ping += ms;
                if (item.time_to_send_test_ping > 30000)
                {
#ifdef epoll_multithreaded
                    if (multithreaded.is_in_process(fd_item)) continue;
#endif

                    item.time_to_send_test_ping = 0;
                    std::deque<m_write*>& wr_ = awaiting_for_write[fd_item];
                    if (wr_.empty())
                    {
                        responser::item::sub s { echo_string };
                        push_message(fd_item, std::move(s)); //ping
                    }
                }
            }
        }

        //write whatever you want and set EPOLLOUT if errno == EAGAIN || errno == EWOULDBLOCK
        std::vector<int> clear_writer;
        std::vector<int> write_writer;
        for (auto& it : awaiting_for_write)
        {
            int fb_to_write = it.first;
#ifdef epoll_multithreaded
            if (multithreaded.is_in_process(fb_to_write)) continue;
#endif

            std::deque<m_write*>& msgs = it.second;
            if (!msgs.empty())
            {
                auto f = authorized_clients.find(fb_to_write);
                if (f != authorized_clients.end()) //client are still connected
                {
                    //check status of writer
                    const m_write* first = msgs[0];
                    if (!first->is_on_waiting_epollout)
                    {
                        write_writer.push_back(fb_to_write);
                    }
                }
                else
                {
                    clear_writer.push_back(fb_to_write);
                }
            }
            else
            {
                clear_writer.push_back(fb_to_write);
            }
        }

        for (int f : clear_writer)
        {
            auto fn = awaiting_for_write.find(f);
            if (fn != awaiting_for_write.end())
            {
                for (auto* item : fn->second)
                {
                    delete item;
                }

                awaiting_for_write.erase(f);
            }
        }
        for (int f : write_writer)
        {
            write_to_fd(epoll_fd, f);
#ifdef debug_epoll_v2
            if (debug_fd == f) timer::print_current_time("write");
#endif
        }

#ifdef epoll_multithreaded
#ifdef epoll_v2_control_activity
        if (t_now - last_activity.last_check > last_activity.period_check)
        {
            last_activity.last_check = t_now;

            std::unordered_set<int> to_remove;

            for (const auto& it : last_activity.data)
            {
                if (t_now - it.second > last_activity.max_time_inactive)
                {
                    to_remove.emplace(it.first);
                    basefunc_std::log("Inactive connection. Remove fd " + std::to_string(it.first), "epoll_v2", true, basefunc_std::COLOR::MAGENTA_COL);
                }
            }

            for (const auto& it : to_remove)
            {
                remove(epoll_fd, it);
            }
        }
#endif
#endif

    }

    basefunc_std::log("exit", "epoll_v2", true);

    t_pool.wait();
    close(sock);
    clear();
    free(events);

    return true;
}

int epoll_v2::write_to_fd(int epoll_fd, int fd)
{
    std::deque<m_write*>& msgs = awaiting_for_write[fd];

    //split messages
#ifdef split_short_messages
    std::string one;
    int remove_i = 0;
    for (; remove_i < msgs.size(); ++remove_i)
    {
        m_write* w = msgs[remove_i];

        if (w->t_size_send != 0) break;

        if (w->msg.size() < 1024 * 1024 && one.size() < 1024 * 1024) one += w->msg;
        else break;
    }

    if (remove_i > 1)
    {
        for (auto it = msgs.begin(); it != msgs.begin() + remove_i; ++it)
        {
            delete (*it);
        }

        msgs.erase(msgs.begin(), msgs.begin() + remove_i);
        msgs.emplace_front(new m_write(std::move(one)));
    }
#endif

#ifdef epoll_multithreaded

    if (!msgs.empty())
    {
        if (!multithreaded.is_in_process(fd))
        {
            if (multithreaded.add_to_process(epoll_fd, fd))
            {
                m_write* w = msgs.front();
                msgs.pop_front();

                t_pool.push_back([this, w, fd]()
                {
                    e_multithreaded::todo status { e_multithreaded::todo::ok };

                    {
                        int b_written;

                        while (true)
                        {
                            if (w->msg.empty()) break;

                            size_t to_send { w->msg.size() - w->t_size_send };

                            b_written = ::send(fd, &w->msg[w->t_size_send], to_send, MSG_NOSIGNAL);

                            if (b_written > 0) w->t_size_send += b_written;

                            if (b_written == -1)
                            {
                                if (errno == EAGAIN || errno == EWOULDBLOCK)  //set epollout
                                {
                                    status = e_multithreaded::todo::set_epoll_out;
                                    break;
                                }
                                else
                                {
                                    status = e_multithreaded::todo::remove;
                                    break;
                                }
                            }
                            else if (b_written == 0) //client exit
                            {
                                status = e_multithreaded::todo::remove;
                                break;
                            }
                            else if (w->t_size_send == w->msg.size()) //all message has been written
                            {
                                break;
                            }
                            else if (w->t_size_send < w->msg.size())
                            {
                                status = e_multithreaded::todo::set_epoll_out;
                                break;
                            }
                            else //must never happen
                            {
                                basefunc_std::cout("must never happen but it did " + std::to_string(__LINE__), "multithreaded", basefunc_std::COLOR::YELLOW_COL);
                                status = e_multithreaded::todo::remove;
                                break;
                            }
                        }
                    }

                    //if (t_size_send != w->msg.size()) w->msg = w->msg.substr(t_size_send);

                    multithreaded.process_finished(fd, status, std::move(w->msg), w->t_size_send);

                    delete w;
                });
            }
        }
        else
        {
            basefunc_std::cout("Write is_in_process", "multithreaded", basefunc_std::COLOR::YELLOW_COL);
        }
    }

#else
    int remove_count_msg { 0 };
    for (int i = 0; i < msgs.size(); ++i)
    {
        m_write* w = msgs[i];

        while (true)
        {
            int b_written = ::send(fd, w->msg.data(), w->msg.size(), MSG_NOSIGNAL);
            if (b_written == -1)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)  //set epollout
                {
                    event.data.fd = fd;
                    event.events = EPOLLOUT | EPOLLET;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event) == -1)
                    {
                        basefunc_std::log("Error to epoll_ctl for write, errno: " + std::to_string(errno) + " error_string: " + strerror(errno), "epoll_v2", true, basefunc_std::COLOR::RED_COL);
                    }

                    w->is_on_waiting_epollout = true;

                    i = msgs.size();
                    break;
                }
                else
                {
                    remove(epoll_fd, fd);
                    i = msgs.size();
                    break;
                }
            }
            else if (b_written == 0) //client exit
            {
                remove(epoll_fd, fd);
                i = msgs.size();
                break;
            }
            else if (b_written == w->msg.size()) //all message has been written
            {
                ++remove_count_msg;
                break;
            }
            else if (b_written < w->msg.size())
            {
                w->msg = w->msg.substr(b_written);
            }
            else //must never happen
            {
                remove(epoll_fd, fd);
                i = msgs.size();
                break;
            }
        }
    }

    for (int i = 0; i < remove_count_msg; ++i)
    {
        if (!msgs.empty())
        {
            m_write* w = msgs.front();
            msgs.pop_front();
            delete w;
        }
    }
#endif
}

void epoll_v2::remove(int epoll_fd, int fd, bool need_del)
{
    int bytes { 0 };
    auto f_w = awaiting_for_write.find(fd);
    if (f_w != awaiting_for_write.end())
    {
        for (auto& it : f_w->second)
        {
            bytes += it->msg.size();
            delete it;
        }

        f_w->second.clear();
    }

    basefunc_std::log("Remove fd " + std::to_string(fd) + " with awaiting_for_write bytes " + std::to_string(bytes), "epoll_v2", true, basefunc_std::COLOR::MAGENTA_COL);

    awaiting_for_write.erase(fd);
    authorized_clients.erase(fd);
    count_connected.store(authorized_clients.size(), std::memory_order_relaxed);
    events_clients.erase(fd);
    awaiting_authorize_clients.erase(fd);

#ifdef epoll_multithreaded
    multithreaded.erase(fd);
#ifdef epoll_v2_control_activity
    last_activity.data.erase(fd);
#endif
#endif

    if(need_del && epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0) //NULL requeries linux kernel 2.6.9 and above
    {
        basefunc_std::log("Error to epoll_ctl EPOLL_CTL_DEL, errno: " + std::to_string(errno) + " error_string: " + strerror(errno), "epoll_v2", true, basefunc_std::COLOR::RED_COL);
    }

    close(fd);
}

bool epoll_v2::socket_check(int fd)
{
   int ret;
   int code;
   int len = sizeof(int);

   ret = getsockopt(fd, SOL_SOCKET, SO_ERROR, &code,(socklen_t*) &len);

   if ((ret || code)!= 0)
      return false;

   return true;
}

bool epoll_v2::set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
    {
        basefunc_std::log("Cant get flags nonblocking, errno: " + std::to_string(errno) + " error_string: " + strerror(errno), "epoll_v2", true, basefunc_std::COLOR::RED_COL);
        return false;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        basefunc_std::log("Cant set flags nonblocking, errno: " + std::to_string(errno) + " error_string: " + strerror(errno), "epoll_v2", true, basefunc_std::COLOR::RED_COL);
        return false;
    }

    return true;
}

bool epoll_v2::bind(int sock, const std::string& ip, int port)
{
    struct sockaddr_in sa;
    if (!inet_aton(ip.c_str(), &sa.sin_addr))
    {
        basefunc_std::log("Cant create sockaddr_in, errno: " + std::to_string(errno) + " error_string: " + strerror(errno), "epoll_v2", true, basefunc_std::COLOR::RED_COL);
        return false;
    }
    sa.sin_port = htons(port);
    sa.sin_family = AF_INET;

    if (::bind(sock, (struct sockaddr *)&sa, sizeof(sa)) < 0)
    {
        basefunc_std::log("Cant bind socket, errno: " + std::to_string(errno) + " error_string: " + strerror(errno), "epoll_v2", true, basefunc_std::COLOR::RED_COL);
        return false;
    }

    return true;
}

bool epoll_v2::create_socket(int& sock)
{
    // create the server socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1)
    {
        basefunc_std::log("Cant create socket, errno: " + std::to_string(errno) + " error_string: " + strerror(errno), "epoll_v2", true, basefunc_std::COLOR::RED_COL);
        return false;
    }
    int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) == -1)
    {
        basefunc_std::log("Cant setsockopt socket, errno: " + std::to_string(errno) + " error_string: " + strerror(errno), "epoll_v2", true, basefunc_std::COLOR::RED_COL);
        return false;
    }

    return true;
}

void epoll_v2::clear()
{
    events_clients.clear();
    authorized_clients.clear();
    count_connected.store(authorized_clients.size(), std::memory_order_relaxed);

    for (auto& it : awaiting_for_write)
    {
        for (auto* item : it.second)
        {
            delete item;
        }
    }

    awaiting_for_write.clear();
    awaiting_authorize_clients.clear();
#ifdef epoll_multithreaded
    multithreaded.clear();
#endif
}

void epoll_v2::push_message_impl(int fd, responser::item::sub&& msg)
{
    auto f = authorized_clients.find(fd);
    if (f != authorized_clients.end())
    {
        const socket_client& c = f->second;
        if (!msg.is_crypted)
        {
            if (c._s.is_aes)
            {
                if (c._s.is_gost_enc) msg.str = gost::g89().crypt(msg.str, c._s.aes_key + c._s.aes_iv);
                msg.str = cryptopp_::encrypt(msg.str, c._s.aes_key, c._s.aes_iv, 32, false);
            }
        }

        std::deque<m_write*>& wr_ = awaiting_for_write[fd];
        wr_.push_back(new m_write(det_string));
        wr_.push_back(new m_write(std::move(msg.str)));
        wr_.push_back(new m_write(det_string));
    }
}

void epoll_v2::push_message_impl(int fd, const responser::item::sub& msg)
{
    auto f = authorized_clients.find(fd);
    if (f != authorized_clients.end())
    {
        const socket_client& c = f->second;
        std::deque<m_write*>& wr_ = awaiting_for_write[fd];
        wr_.push_back(new m_write(det_string));
        if (!c._s.is_aes || msg.is_crypted)
        {
            wr_.push_back(new m_write(msg.str));
        }
        else
        {
            if (c._s.is_gost_enc)
            {
                std::string msg2;
                msg2.reserve(msg.str.size());
                msg2 = gost::g89().crypt(msg.str, c._s.aes_key + c._s.aes_iv);
                wr_.push_back(new m_write(std::move(cryptopp_::encrypt(msg2, c._s.aes_key, c._s.aes_iv, 32, false))));
            }
            else
            {
                wr_.push_back(new m_write(std::move(cryptopp_::encrypt(msg.str, c._s.aes_key, c._s.aes_iv, 32, false))));
            }
        }
        wr_.push_back(new m_write(det_string));
    }
}

std::vector<std::string> epoll_v2::process_read(message_helper& _m, const char* buf, ssize_t nbytes, const std::string& _det_string)
{
    std::vector<std::string> ret;

    std::function<void(std::int32_t)> f_read;
    int depth { 0 };
    f_read = [&](std::int32_t i)
    {
        ++depth;

        std::int32_t delimeter_pos = _m.seek_delimeter + 1;
        if (delimeter_pos < _det_string.size())
        {
            char del_char = _det_string[delimeter_pos];
            for (; i < nbytes; ++i)
            {
                char ch = buf[i];
                if (ch == del_char)
                {
                    ++_m.seek_delimeter;
                    ++i;
                    _m.del_message.push_back(ch);
                    f_read(i);
                    break;
                }
                else
                {
                    if (!_m.del_message.empty())
                    {
                        if (depth > 0)
                        {
                            //std::cout << "debug_read " << depth << std::flush << std::endl;
                        }
                        if (depth > 30000) return;

                        _m.message.insert(_m.message.end(), _m.del_message.begin(), _m.del_message.end());

                        _m.del_message.clear();
                        _m.seek_delimeter = -1;
                        del_char = _det_string[0];
                    }

                    //check on delimeter
                    if (ch == del_char)
                    {
                        ++_m.seek_delimeter;
                        ++i;
                        _m.del_message.push_back(ch);
                        f_read(i);
                        break;
                    }
                    else
                    {
                        _m.message.push_back(ch);
                    }
                }
            }
        }
        else //message has been complited
        {
            if (!_m.message.empty())
            {
                ret.push_back({ _m.message.begin(), _m.message.end() });
                //process_message(c);
            }

            _m.clear();
            f_read(i);
        }
    };

    f_read(0);

    return ret;
}

std::vector<std::string> epoll_v2::process_read2(message_helper& _m, const char* buf, ssize_t nbytes, const std::string& _det_string)
{
    std::vector<std::string> ret;

    auto f_add_message = [&]() -> void
    {
        if (!_m.message.empty()) ret.push_back({ _m.message.begin(), _m.message.end() });
        _m.clear();
    };

    std::int32_t i { 0 };
    while (i < nbytes)
    {
        std::int32_t delimeter_pos = _m.seek_delimeter + 1;
        if (delimeter_pos < _det_string.size())
        {
            char del_char = _det_string[delimeter_pos];
            for (; i < nbytes; ++i)
            {
                char ch = buf[i];
                if (ch == del_char)
                {
                    ++_m.seek_delimeter;
                    ++i;
                    _m.del_message.push_back(ch);
                    //f_read(i);
                    break;
                }
                else
                {
                    if (!_m.del_message.empty())
                    {
                        _m.message.insert(_m.message.end(), _m.del_message.begin(), _m.del_message.end());

                        _m.del_message.clear();
                        _m.seek_delimeter = -1;
                        del_char = _det_string[0];
                    }

                    //check on delimeter
                    if (ch == del_char)
                    {
                        ++_m.seek_delimeter;
                        ++i;
                        _m.del_message.push_back(ch);
                        //f_read(i);
                        break;
                    }
                    else
                    {
                        _m.message.push_back(ch);
                    }
                }
            }

            if (i == nbytes) //read has been finished
            {
                delimeter_pos = _m.seek_delimeter + 1;
                if (delimeter_pos >= _det_string.size())
                {
                    f_add_message();
                }
            }
        }
        else //message has been complited
        {
            f_add_message();
        }
    }

    return ret;
}

void epoll_v2::process_message(socket_client& c, std::string&& str)
{
    bool ok { true };
    if (is_aes)
    {
        if (!c._s.is_aes)
        {
            if (c._s.aes_key.empty())
            {
                int fd { c.fd };
                uint64_t count_accepted_on { c.count_accepted_on };

                t_pool.push_back([this, str, fd, count_accepted_on]()
                {
                    try
                    {
                        std::string keys = basefunc_std::uncompress_number(commpression_zlib::decompress_string(str));
                        if (keys.size() > 4)
                        {
                            std::int32_t size_of_p;
                            bitbase::chars_to_numeric(keys.substr(0, 4), size_of_p);

                            if (7 + size_of_p < keys.size())
                            {
                                std::string g_ = keys.substr(4, 1);
                                std::string p_ = keys.substr(5, size_of_p);
                                std::string A_ = keys.substr(5 + size_of_p);

                                std::string b_;
                                for (int i = 0; i < 100; ++i) b_ += std::to_string(basefunc_std::rand(1000000000, std::numeric_limits<int>::max()));

                                DH_BIG::DH g(g_.c_str(), 10);
                                DH_BIG::DH p(p_.c_str(), 10);
                                DH_BIG::DH b(b_.c_str(), 10);
                                DH_BIG::DH A(A_.c_str(), 10);

                                DH_BIG::DH B = DH_BIG::DH::pow(g, b, p);
                                DH_BIG::DH K = DH_BIG::DH::pow(A, b, p);

                                responser::item::sub s { MD5(K.get_string()).hexdigest() };
                                s.is_key_generation = true;
                                responser_.add(fd, count_accepted_on, std::move(s));

                                responser::item::sub s2 { basefunc_std::compress_number(B.get_string()) };
                                responser_.add(fd, count_accepted_on, std::move(s2));
                            }
                            else
                            {
                                throw std::out_of_range(std::to_string(7 + size_of_p) + " < " + std::to_string(keys.size()));
                            }
                        }
                    }
                    catch(std::exception& ex)
                    {
                        basefunc_std::cout(ex.what(), "aes_error", basefunc_std::COLOR::RED_COL);
                    }
                });

                /*try
                {
                    std::string keys = basefunc_std::uncompress_number(commpression_zlib::decompress_string(str));
                    if (keys.size() > 4)
                    {
                        std::int32_t size_of_p;
                        bitbase::chars_to_numeric(keys.substr(0, 4), size_of_p);

                        if (7 + size_of_p < keys.size())
                        {
                            std::string g_ = keys.substr(4, 1);
                            std::string p_ = keys.substr(5, size_of_p);
                            std::string A_ = keys.substr(5 + size_of_p);

                            std::string b_;
                            for (int i = 0; i < 100; ++i) b_ += std::to_string(basefunc_std::rand(1000000000, std::numeric_limits<int>::max()));

                            DH_BIG::DH g(g_.c_str(), 10);
                            DH_BIG::DH p(p_.c_str(), 10);
                            DH_BIG::DH b(b_.c_str(), 10);
                            DH_BIG::DH A(A_.c_str(), 10);

                            DH_BIG::DH B = DH_BIG::DH::pow(g, b, p);
                            DH_BIG::DH K = DH_BIG::DH::pow(A, b, p);

                            c._s.aes_key = MD5(K.get_string()).hexdigest();
                            c._s.aes_iv = c._s.aes_key.substr(0, 16);

                            responser::item::sub s { basefunc_std::compress_number(B.get_string()) };
                            push_message(c.fd, std::move(s));
                        }
                        else
                        {
                            throw std::out_of_range(std::to_string(7 + size_of_p) + " < " + std::to_string(keys.size()));
                        }
                    }
                }
                catch(std::exception& ex)
                {
                    basefunc_std::cout(ex.what(), "aes_error", basefunc_std::COLOR::RED_COL);
                }*/
            }
            else //check test message over aes
            {
                std::string msg = cryptopp_::decrypt(str, c._s.aes_key, c._s.aes_iv, 32, false);
                if (msg.compare("ok_aes") == 0)
                {
                    c._s.is_aes = true;
                    push_message(c.fd, responser::item::sub("ok_aes"));
                    if (is_gost_enc) c._s.is_gost_enc = true;
#ifdef epoll_multithreaded
                    multithreaded.set_encryption_data(c.fd, c._s);
#endif
                }
            }

            ok = false;
        }
        else //uncompress
        {
            str = cryptopp_::decrypt(str, c._s.aes_key, c._s.aes_iv, 32, false);
            if (c._s.is_gost_enc) str = gost::g89().decrypt(str, c._s.aes_key + c._s.aes_iv);
        }
    }

    if (ok && on_message)
    {
#ifdef debug_epoll_v2
        if (debug_fd == c.fd) timer::print_current_time(str.c_str());
#endif
        on_message(c, std::move(str), t_pool, responser_);
    }
}
