 #pragma once

#include <dpp/dpp.h>
#include "database.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <set>
#include <chrono>
#include <string>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>

class Scheduler
{
public:
    struct ScheduledJob
    {
        int id;
        std::string type; // once or recurring
        dpp::snowflake channel_id;
        std::string message_text;
        std::chrono::system_clock::time_point next_run_time;
        std::chrono::seconds interval;
        std::string interval_str;

        bool operator<(const ScheduledJob& other) const
        {
            if (next_run_time != other.next_run_time)
                return next_run_time < other.next_run_time;
            return id < other.id;
        }
    };

    explicit Scheduler(dpp::cluster& b) : bot(b) {}

private:
    dpp::cluster& bot;
    std::thread worker_thread; // main background worker thread for scheduler
    std::atomic<bool> running{ false };
    std::set<ScheduledJob> jobs;
    std::mutex mutex_; // make sure thread doesn't read the vector if slash command is adding a new job
    std::condition_variable cv_; // to sleep and wake up the thread

    // Helper to parse PostgreSQL UTC time string to C++ time_point
    std::chrono::system_clock::time_point parse_db_time(const std::string& str)
    {
        std::tm tm = {};
        std::istringstream ss(str);

        ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");

        std::time_t t{ timegm(&tm) };
        return std::chrono::system_clock::from_time_t(t);
    }

    // Helper to format C++ time_point to PostgreSQL UTC string
    std::string format_db_time(std::chrono::system_clock::time_point tp)
    {
        auto time_t_val{ std::chrono::system_clock::to_time_t(tp) };
        std::tm tm_val{ *std::gmtime(&time_t_val) };
        std::ostringstream oss;
        oss << std::put_time(&tm_val, "%Y-%m-%d %H:%M:%S+00");
        return oss.str();
    }

    // Executes a scheduled job (posts to Discord, updates database and the cache)
    void execute_job(const ScheduledJob& job)
    {
        if (job.type == "once")
        {
            // 1. Delete from database
            try
            {
                ConnectionGuard guard;
                pqxx::work txn{ guard.get() };
                txn.exec("DELETE FROM scheduled_messages WHERE id = $1;", job.id);
                txn.commit();
            }
            catch (const std::exception& e)
            {
                std::cerr << "[Scheduler Error] Failed to delete job ID " << job.id << "from DB: " << e.what() << '\n';
            }

            bot.message_create({job.channel_id, job.message_text});
            std::cout << "[Scheduler] Executed one job ID " << job.id << " to channel " << job.channel_id << '\n';
        }
        else if (job.type == "recurring")
        {
            // Calculate next run time
            auto now{ std::chrono::system_clock::now() };
            auto new_next_run{ job.next_run_time + job.interval };
            if (new_next_run <= now) {
                // Skip missed turns during downtime
                new_next_run = now + job.interval;
            }

            // Update in DB
            try
            {
                ConnectionGuard guard;
                pqxx::work txn{ guard.get() };
                txn.exec("UPDATE scheduled_messages SET next_run_time = $1 WHERE id = $2;",
                    { format_db_time(new_next_run), job.id });
                txn.commit();
            }
            catch (const std::exception& e)
            {
                std::cerr << "[Scheduler Error] Failed to update recurring job ID " << job.id << "in DB: " << e.what() << '\n';
            }

            // Put back in sorted cache
            {
                std::lock_guard guard{ mutex_ };
                ScheduledJob updated_job{ job };
                updated_job.next_run_time = new_next_run;
                jobs.insert(updated_job);
            }
        }
    }
};