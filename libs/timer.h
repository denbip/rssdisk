#ifndef TIMER_H
#define TIMER_H

#include <chrono>
#include <iostream>
#include <iomanip>
#include <functional>

#define RESET   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */

class timer
{
public:
    struct guard
    {
        guard(timer* _t, std::function<void(timer* _t)> _f) : t(_t), f(_f) { }
        ~guard()
        {
            run();
        }

        void run()
        {
            if (t != nullptr)
            {
                if (f != nullptr) f(t);
                delete t;
                t = nullptr;
            }
        }

    private:
        timer* t = nullptr;
        std::function<void(timer* _t)> f = nullptr;
    };

    timer()
    {
        init = std::chrono::high_resolution_clock::now();
    }

    unsigned long elapsed_mili(bool reset = false)
    {
        std::chrono::milliseconds ret = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - init);
        if (reset) init = std::chrono::high_resolution_clock::now();
        return (unsigned long)ret.count();
    }

    unsigned long elapsed_micro(bool reset = false)
    {
        std::chrono::microseconds ret = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - init);
        if (reset) init = std::chrono::high_resolution_clock::now();
        return (unsigned long)ret.count();
    }

    void cout_mili(bool reset = false)
    {
        std::chrono::milliseconds ret = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - init);
        std::cout << GREEN << "[timer] " << RESET << ret.count() << " mili" << std::endl;
        if (reset) init = std::chrono::high_resolution_clock::now();
    }

    void cout_mili(const char* text, bool reset = false)
    {
        std::chrono::milliseconds ret = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - init);
        std::cout << GREEN << "[timer] " << RESET << text << " " << ret.count() << " mili" << std::endl;
        if (reset) init = std::chrono::high_resolution_clock::now();
    }

    void cout_mili(const std::string& text, bool reset = false)
    {
        std::chrono::milliseconds ret = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - init);
        std::cout << GREEN << "[timer] " << RESET << text << " " << ret.count() << " mili" << std::endl;
        if (reset) init = std::chrono::high_resolution_clock::now();
    }

    void reset()
    {
        init = std::chrono::high_resolution_clock::now();
    }

    void cout_micro(bool reset = false)
    {
        std::chrono::microseconds ret = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - init);
        std::cout << GREEN << "[timer, micro] " << RESET << ret.count() << " micro" << std::endl;
        if (reset) init = std::chrono::high_resolution_clock::now();
    }

    void cout_micro(const char* text, bool reset = false)
    {
        std::chrono::microseconds ret = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - init);
        std::cout << GREEN << "[timer, micro] " << RESET << text << " " << ret.count() << " micro" << std::endl;
        if (reset) init = std::chrono::high_resolution_clock::now();
    }

    static void print_current_time(const char* text = "")
    {
        using Clock = std::chrono::high_resolution_clock;
        constexpr auto num = Clock::period::num;
        constexpr auto den = Clock::period::den;
        std::cout << GREEN << "[timer, current] " << RESET << text << " " << std::setprecision(3) << std::fixed << double(Clock::now().time_since_epoch().count() * num) / double(den) << std::endl;
    }

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> init;
};

#endif // TIMER_H
