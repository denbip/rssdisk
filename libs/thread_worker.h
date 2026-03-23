#ifndef THREAD_WORKER_H
#define THREAD_WORKER_H

#include <thread>
#include <functional>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <queue>

#include "basefunc_std.h"

namespace thread_pool
{

class thread_pool
{
public:
    explicit thread_pool(unsigned count_thread, bool optimize_system_count_thread = true) : count_works(ATOMIC_VAR_INIT(0))
    {
        unsigned t = count_thread;
        if (optimize_system_count_thread) t = std::min(count_thread, std::thread::hardware_concurrency());
        if (t == 0) t = count_thread;
        if (t == 0) t = 1;

        isRunning.store(true, std::memory_order_release);
        paused.store(false, std::memory_order_release);

        for (unsigned i = 0 ; i < t; ++i)
        {
            std::thread thr = std::thread([this]()
            {
                while (isRunning.load(std::memory_order_acquire))
                {
                    while (paused.load(std::memory_order_acquire))
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }

                    std::function<void()> cc;
                    {
                        std::unique_lock<std::mutex> lock(mutex);
                        cond.wait_for(lock, std::chrono::milliseconds(100), [this]()
                        {
                            if (!d.empty() || !isRunning.load(std::memory_order_acquire)) return true;
                            else return false;
                        });

                        if (!isRunning.load(std::memory_order_acquire)) break;
                        if (d.empty()) continue;

                        cc = d.front();
                        d.pop_front();
                    }

                    cc();

                    count_works.fetch_sub(1, std::memory_order_acq_rel);

                    if (count_works.load(std::memory_order_acquire) == 0)
                    {
                        cond_work.notify_all();
                    }
                    else
                    {
                        cond.notify_one();
                    }
                }
            });
            threads.push_back(std::move(thr));
        }
    }
    ~thread_pool()
    {
        isRunning.store(false, std::memory_order_release);
        paused.store(false, std::memory_order_release);
        cond.notify_all();

        for (unsigned i = 0 ; i < threads.size(); ++i)
        {
            if (threads[i].joinable())
            {
                cond.notify_all();
                threads[i].join();
            }
        }
    }

    thread_pool(const thread_pool& other) = delete;
    thread_pool& operator=(const thread_pool& other) = delete;
    thread_pool(thread_pool&& other) = delete;
    thread_pool& operator=(thread_pool&& other) = delete;

    void stop()
    {
        isRunning.store(false, std::memory_order_release);
        paused.store(false, std::memory_order_release);
        cond.notify_all();
    }

    bool is_running() const
    {
        return isRunning.load(std::memory_order_acquire);
    }

    void push_back(std::function<void()>&& val)
    {
        if (isRunning.load(std::memory_order_acquire))
        {
            count_works.fetch_add(1, std::memory_order_acq_rel);

            {
                std::unique_lock<std::mutex> lock(mutex);
                d.emplace_back(std::move(val));
            }
            cond.notify_one();
        }
    }

    unsigned get_queque_size() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return d.size();
    }

    unsigned get_count_works() const
    {
        return count_works.load(std::memory_order_acquire);
    }

    bool empty() const
    {
        return count_works.load(std::memory_order_acquire) == 0;
    }

    void wait() const
    {
        while (count_works.load(std::memory_order_acquire) != 0)
        {
            {
                std::unique_lock<std::mutex> lock_work_(lock_work);
                cond_work.wait_for(lock_work_, std::chrono::milliseconds(100), [this]() { if (count_works.load(std::memory_order_acquire) == 0) return true; else return false; });
            }
        }
    }

    template<class Rep, class Period>
    bool wait(const std::chrono::duration<Rep, Period>& timeout) const
    {
        std::unique_lock<std::mutex> lock_work_(lock_work);
        return cond_work.wait_for(lock_work_, timeout, [this]() { if (count_works.load(std::memory_order_acquire) == 0) return true; else return false; });
    }

    std::size_t get_count_threads() const noexcept
    {
        return threads.size();
    }

    void detach_all_threads()
    {
        for (std::thread& t : threads)
        {
            if (t.joinable()) t.detach();
        }
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mutex);
        d.clear();
    }

    void pause()
    {
        paused.store(true, std::memory_order_release);
    }

    void unpause()
    {
        paused.store(false, std::memory_order_release);
    }

    void in_parallel(const std::int32_t sz, std::function<void(const std::int32_t start, const std::int32_t end, std::mutex& m)> f)
    {
        std::mutex lock_pool;
        std::int32_t count_per_thread = sz / get_count_threads();
        if (count_per_thread == 0) count_per_thread = 1;
        std::int32_t start { 0 };
        for (std::int32_t i = 0; i < get_count_threads(); ++i)
        {
            std::int32_t end = start + count_per_thread;
            if (end >= sz || (i == get_count_threads() - 1)) end = sz - 1;
            if (start > end) break;

            push_back([start, end, &lock_pool, &f]()
            {
                f(start, end, lock_pool);
            });
            start = end + 1;
        }

        wait();
    }

    static void in_parallel(const std::int32_t sz, std::uint32_t sz_threads, std::function<void(const std::int32_t start, const std::int32_t end, std::mutex& m)> f)
    {
        if (sz_threads <= 0) sz_threads = 1;
        thread_pool t_pool_in_parrallel { sz_threads };
        t_pool_in_parrallel.in_parallel(sz, f);
    }

    template<class T>
    void in_parallel(std::vector<T>& v, std::function<void(T& t)> f)
    {
        const std::int32_t sz { v.size() };

        std::int32_t count_per_thread = sz / get_count_threads();
        if (count_per_thread == 0) count_per_thread = 1;
        std::int32_t start { 0 };
        for (std::int32_t i = 0; i < get_count_threads(); ++i)
        {
            std::int32_t end = start + count_per_thread;
            if (end >= sz || (i == get_count_threads() - 1)) end = sz - 1;
            if (start > end) break;

            push_back([start, end, &f, &v]()
            {
                for (std::int32_t n = start; n <= end; ++n)
                {
                    f(v[n]);
                }
            });

            start = end + 1;
        }

        wait();
    }

    template<class T_id, class T_val>
    void in_parallel(const std::unordered_map<T_id, T_val>& v, std::function<void(const T_id& tk, const T_val& tv)> f)
    {
        const std::int32_t sz { v.size() };

        std::int32_t count_per_thread = sz / get_count_threads();
        if (count_per_thread == 0) count_per_thread = 1;
        std::int32_t start { 0 };
        for (std::int32_t i = 0; i < get_count_threads(); ++i)
        {
            std::int32_t end = start + count_per_thread;
            if (end >= sz || (i == get_count_threads() - 1)) end = sz - 1;
            if (start > end) break;

            push_back([start, end, &f, &v]()
            {
                for (std::int32_t n = start; n <= end; ++n)
                {
                    auto it = v.begin();
                    std::advance(it, n);
                    f(it->first, it->second);
                }
            });

            start = end + 1;
        }

        wait();
    }

private:
    std::vector<std::thread> threads;
    volatile std::atomic<unsigned> count_works;

    std::deque<std::function<void()> > d;
    mutable std::mutex mutex;
    std::condition_variable cond;
    volatile std::atomic<bool> isRunning;
    volatile std::atomic<bool> paused;

    mutable std::mutex lock_work;
    mutable std::condition_variable cond_work;
};

class pool_of_pool
{
public:
    pool_of_pool(unsigned count_pools, unsigned count_threads_per_pool, bool optimize_system_count_thread)
    {
        for (auto i = 0u; i < count_pools; ++i)
        {
            _pool.push_back( new thread_pool(count_threads_per_pool, optimize_system_count_thread) );
            _free.emplace(i);
        }
    }

    ~pool_of_pool()
    {
        for (auto i = 0u; i < _pool.size(); ++i)
        {
            _pool[i]->stop();
        }
    }

    void stop()
    {
        for (auto i = 0u; i < _pool.size(); ++i)
        {
            _pool[i]->stop();
        }
    }

    struct free_guard
    {
        free_guard(pool_of_pool* _that, thread_pool* _p, int _index, bool _with_priority) : that(_that), p(_p), index(_index), with_priority(_with_priority) { }

        ~free_guard()
        {
            if (p != nullptr)
            {
                that->release(index, with_priority);
                p = nullptr;
            }
        }

        thread_pool* p { nullptr };
    private:
        int index;
        bool with_priority;
        pool_of_pool* that;
    };

    thread_pool* get(std::size_t index)
    {
        if (index < _pool.size()) return _pool[index];
        return _pool[0];
    }

    free_guard wait_and_get(bool with_priority)
    {
        thread_pool* _p { nullptr };
        int i = wait_and_get(_p, with_priority);
        return { this, _p, i, with_priority };
    }

    int wait_and_get(thread_pool*& _p, bool with_priority)
    {
        while (true)
        {
            std::unique_lock<std::mutex> _l{ _lock };

            if (!_free.empty())
            {
                int _index = *(_free.begin());
                _free.erase(_index);
                _p = _pool[_index];

                if (with_priority)
                {
                    _priority_running.emplace(_index);
                    _p->unpause();
                }

                return _index;
            }

            std::condition_variable* _cd = new std::condition_variable();

            if (with_priority) _cvar.push_front(_cd);
            else _cvar.push_back(_cd);

            while (!_cd->wait_for(_l, std::chrono::milliseconds(10), [this](){ return !_free.empty(); }))
            {
                if (with_priority)
                {
                    for (auto i = 0u; i < _pool.size(); ++i)
                    {
                        _pool[i]->unpause();
                    }
                }
            }

            //_cd->wait(_l, [this](){ return !_free.empty(); });

            _cvar.erase(std::remove(_cvar.begin(), _cvar.end(), _cd), _cvar.end());
            delete _cd;

            if (!_free.empty())
            {
                int _index = *(_free.begin());
                _free.erase(_index);
                _p = _pool[_index];

                if (with_priority)
                {
                    _priority_running.emplace(_index);
                    _p->unpause();
                }

                return _index;
            }
        }
    }

    void release(int _index, bool with_priority)
    {
        {
            std::unique_lock<std::mutex> _l{ _lock };
            _free.emplace(_index);
            _priority_running.erase(_index);
            if (!_cvar.empty()) _cvar.front()->notify_one();
        }
    }

    void pause_no_priority()
    {
        {
            std::unique_lock<std::mutex> _l{ _lock };

            for (auto i = 0u; i < _pool.size(); ++i)
            {
                if (_priority_running.count(i) == 0)
                {
                    _pool[i]->pause();
                }
            }
        }
    }

    void unpause_no_priority()
    {
        {
            std::unique_lock<std::mutex> _l{ _lock };

            for (auto i = 0u; i < _pool.size(); ++i)
            {
                _pool[i]->unpause();
            }
        }
    }

private:
    std::vector<thread_pool*> _pool;
    std::mutex _lock;
    std::unordered_set<int> _free;
    std::deque<std::condition_variable*> _cvar;
    std::unordered_set<int> _priority_running;
};

/*class thread_worker
{
public:
    explicit thread_worker()
    {
        isRunning.store(true, std::memory_order_release);

        thr = std::thread([this]()
        {
            while (isRunning.load(std::memory_order_acquire))
            {
                std::function<void()> cc;
                {
                    std::unique_lock<std::mutex> lock(mutex);
                    cond.wait(lock, [this]() { if (!d.empty() || !isRunning.load(std::memory_order_acquire)) return true; else return false; });
                    if (!isRunning.load(std::memory_order_acquire)) break;
                    cc = d.front();
                    d.pop_front();
                }
                cc();

                is_work.store(false, std::memory_order_release);
                cond_work.notify_one();
            }
        });
    }
    ~thread_worker()
    {
        isRunning.store(false, std::memory_order_release);
        cond.notify_all();
        if (thr.joinable()) thr.join();
    }

    thread_worker(const thread_worker& other) = delete;
    thread_worker& operator=(const thread_worker& other) = delete;
    thread_worker(thread_worker&& other) = delete;
    thread_worker& operator=(thread_worker&& other) = delete;


    void push_back(const std::function<void()> val)
    {
        is_work.store(true, std::memory_order_release);

        std::lock_guard<std::mutex> lock(mutex);
        d.push_back(val);
        cond.notify_one();
    }

    void wait()
    {
        while (is_work.load(std::memory_order_acquire))
        {
            {
                std::unique_lock<std::mutex> lock_work_(lock_work);
                cond_work.wait(lock_work_, [this]() { if (!is_work.load(std::memory_order_acquire)) return true; else return false; });
            }
        }
    }

private:
    std::thread thr;

    std::deque<std::function<void()> > d;
    std::mutex mutex;
    std::condition_variable cond;
    std::atomic<bool> isRunning;

    std::mutex lock_work;
    std::condition_variable cond_work;
    std::atomic<bool> is_work;
};*/

}

#endif // THREAD_WORKER_H
