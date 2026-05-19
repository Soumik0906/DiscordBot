#pragma once

#include <dpp/dpp.h>
#include <functional>
#include <chrono>

class Scheduler
{
    dpp::cluster& bot;

public:
    explicit Scheduler(dpp::cluster& b) : bot(b) {}

    
    void schedule_once(
        std::chrono::seconds delay,
        std::function<void()> task
    )
    {
        // Start a timer and immediately stop it after the first execution.
        bot.start_timer([this, task](dpp::timer h) {
            task();
            bot.stop_timer(h);
        }, delay.count());
    }

    void schedule_recurring(
        std::chrono::seconds interval,
        std::function<void()> task
    )
    {
        bot.start_timer([task](dpp::timer h) {
            task();
        }, interval.count());
    }
};
