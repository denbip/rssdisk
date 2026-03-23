#ifndef TS_DEQUE
#define TS_DEQUE

#include <deque>
#include <mutex>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <functional>

#include <iostream>

template<class T>
class ts_deque
{
public:
    ts_deque()
    {
        isRunning.store(true, std::memory_order_release);
    }

    bool push_back_check_size(const T& val, const unsigned max_size)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (d.size() < max_size)
        {
            d.push_back(val);
            cond.notify_one();
            return true;
        }
        return false;
    }

    void push_front(const T& val)
    {
        std::lock_guard<std::mutex> lock(mutex);
        d.push_front(val);
        cond.notify_one();
    }

    void push_back(const T& val)
    {
        std::lock_guard<std::mutex> lock(mutex);
        d.push_back(val);
        cond.notify_one();
    }

    void push_back(T&& val)
    {
        std::lock_guard<std::mutex> lock(mutex);
        d.push_back(std::move(val));
        cond.notify_one();
    }

    bool pop_front(T& reff)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (d.size() > 0)
        {
            reff = d.front();
            d.pop_front();
            return true;
        }
        return false;
    }

    bool front(T& reff)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (d.size() > 0)
        {
            reff = d.front();
            return true;
        }
        return false;
    }

    bool pop_front()
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (d.size() > 0)
        {
            d.pop_front();
            return true;
        }
        return false;
    }

    void wait_signal(std::function<void(T)> callBack)
    {
        while (isRunning.load(std::memory_order_acquire))
        {
            T cc;
            {
                std::unique_lock<std::mutex> lock(mutex);
                cond.wait(lock, [this]() { if (!d.empty() || !isRunning.load(std::memory_order_acquire)) return true; else return false; });
                if (!isRunning.load(std::memory_order_acquire)) break;
                cc = std::move(d.front());
                d.pop_front();
            }
            callBack(cc);
        }
    }

    void stop()
    {
        isRunning.store(false, std::memory_order_release);
        cond.notify_all();
    }

    void swap(std::deque<T>& d_)
    {
        std::lock_guard<std::mutex> lock(mutex);
        std::swap(d, d_);
    }

private:
    std::deque<T> d;
    std::mutex mutex;
    std::condition_variable cond;
    std::atomic<bool> isRunning;
};

#endif // TS_DEQUE

