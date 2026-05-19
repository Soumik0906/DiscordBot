#pragma once

#include <boost/asio.hpp>

#include <vector>
#include <memory>
#include <thread>
#include <functional>

class Scheduler
{
    boost::asio::io_context io;

    boost::asio::executor_work_guard<
        boost::asio::io_context::executor_type
    > work_guard;

    std::vector<std::thread> workers;

    std::vector<
        std::shared_ptr<boost::asio::steady_timer>
    > timers;

public:
    Scheduler(size_t thread_count = 4)
        : work_guard(boost::asio::make_work_guard(io))
    {
        for (size_t i{ 0 }; i < thread_count; ++i) {
            workers.emplace_back([this](){
                io.run();
            });
        }
    }

    ~Scheduler()
    {
        io.stop();

        for (auto& thread : workers)
            thread.join();
    }

    void schedule_once(
        std::chrono::seconds delay,
        std::function<void()> task
    )
    {
        auto timer {
            std::make_shared<boost::asio::steady_timer>(io, delay)
        };

        timers.emplace_back(timer);

        timer->async_wait(
            [task, timer](const boost::system::error_code& ec) {
                if (!ec) {
                    task();
                }
            }
        );
    }

    void schedule_recurring(
        std::chrono::seconds interval,
        std::function<void()> task
    )
    {
        auto timer {
            std::make_shared<boost::asio::steady_timer>(io, interval)
        };

        timers.emplace_back(timer);

        auto handler {
            std::make_shared<
                std::function<void(const boost::system::error_code&)>
            >()
        };

        *handler =
            [this, timer, interval, task, handler]
        (const boost::system::error_code& ec)
            {
                if (ec)
                    return;

                task();

                timer->expires_after(interval);

                timer->async_wait(*handler);
            };

        timer->async_wait(*handler);
    }
};