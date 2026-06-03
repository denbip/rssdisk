#ifndef THREAD_TIMER_H
#define THREAD_TIMER_H

#include <thread>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <atomic>

namespace thread
{
    class timer
    {
    public:
        timer() = default;
        timer(const timer& o) = delete;
        timer& operator=(const timer& o) = delete;

        //returns false if killed
        template<class R, class P>
        bool wait_for(std::chrono::duration<R, P> const& time)
        {
            std::chrono::steady_clock::time_point until = std::chrono::steady_clock::now() + time;

            while (!terminate && until >= std::chrono::steady_clock::now())
            {
                std::unique_lock<std::mutex> lock(m);
                cv.wait_until(lock, until, [&]
                {
                     return terminate || until < std::chrono::steady_clock::now();
                });
            }

            return !terminate;
        }

        void kill()
        {
            std::unique_lock<std::mutex> lock(m);
            terminate = true;
            cv.notify_all();
        }

        void reset()
        {
            std::unique_lock<std::mutex> lock(m);
            terminate = false;
        }

    private:
        std::condition_variable cv;
        std::mutex m;
        bool terminate = false;
    };

    class timer_cv
    {
    public:
        timer_cv() = default;
        timer_cv(const timer& o) = delete;
        timer_cv& operator=(const timer& o) = delete;

        //returns false if killed
        template<class R, class P>
        bool wait_for(std::chrono::duration<R, P> const& time)
        {
            std::chrono::steady_clock::time_point until = std::chrono::steady_clock::now() + time;

            while (!terminate && until >= std::chrono::steady_clock::now() && !nv.load(std::memory_order_acquire))
            {
                std::unique_lock<std::mutex> lock(m);
                cv.wait_until(lock, until, [&]
                {
                    return terminate || until < std::chrono::steady_clock::now() || nv.load(std::memory_order_acquire);
                });
            }

            nv.store(false, std::memory_order_release);

            return !terminate;
        }

        void notify()
        {
            nv.store(true, std::memory_order_release);
            cv.notify_all();
        }

        void kill()
        {
            std::unique_lock<std::mutex> lock(m);
            terminate = true;
            cv.notify_all();
        }

        void reset()
        {
            std::unique_lock<std::mutex> lock(m);
            terminate = false;
        }

    private:
        std::condition_variable cv;
        std::mutex m;
        bool terminate = false;
        std::atomic_bool nv { ATOMIC_VAR_INIT(false) };
    };
}

#endif // THREAD_TIMER_H
