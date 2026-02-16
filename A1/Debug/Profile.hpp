#pragma once
#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <mutex>

struct Timer 
{
    using clock = std::chrono::high_resolution_clock;

    Timer(std::function< void(double) > const &cb_):
    cb(cb_),
    before(clock::now())
    {}
    ~Timer() 
    {
        clock::time_point after = clock::now();
        cb(std::chrono::duration< double >(after - before).count());
    }
    std::function< void(double) > cb;
    clock::time_point before;
};

struct LabelTimer : public Timer
{
    std::string PipelineName;
    
    LabelTimer(std::string Name, std::function<void(std::string, double)> const &cb_)
        : Timer([Name, cb_](double elapsed) { cb_(Name, elapsed); }), 
          PipelineName(Name) {}
};

inline int& GetTraceCount() 
{
    static int Count = 0;
    return Count;
}

#define TRACE_SIMPLE_CLOCK(label) \
    static int TraceIdx##__LINE__ = GetTraceCount()++; \
    static double TotalTime##__LINE__ = 0; \
    static long long Count##__LINE__ = 0; \
    Timer timer_##__LINE__([&](double duration) { \
        static std::mutex s_cout_mutex; \
        std::lock_guard<std::mutex> lock(s_cout_mutex); \
        \
        TotalTime##__LINE__ += duration; \
        Count##__LINE__++; \
        double avg = TotalTime##__LINE__ / Count##__LINE__; \
        \
        for(int i=0; i < TraceIdx##__LINE__; ++i) std::cout << "\n"; \
        \
        std::cout << "\r[" << label << "]" \
                  << " Avg: " << avg * 1000.0 << " ms    "; \
        \
        for(int i=0; i < TraceIdx##__LINE__; ++i) std::cout << "\033[F"; \
        std::cout << std::flush; \
    })