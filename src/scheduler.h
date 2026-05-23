#pragma once

#include <chrono>
#include <condition_variable>
#include <ctime>
#include <dpp/dpp.h>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include "database.h"

class Scheduler
{
  public:
    struct ScheduledJob {
        int id;
        std::string type; // once or recurring
        dpp::snowflake channel_id;
        std::string message_text;
        std::chrono::system_clock::time_point next_run_time;
        std::chrono::seconds interval;
        std::string interval_str;

        bool operator<(const ScheduledJob &other) const
        {
            if (next_run_time != other.next_run_time)
                return next_run_time < other.next_run_time;
            return id < other.id;
        }
    };

    std::vector<ScheduledJob> job_list() const
    {
        std::lock_guard lock{ mutex_ };
        return { jobs.begin(), jobs.end() };
    }

    bool remove_job(int id)
    {
        {
            std::lock_guard lock{ mutex_ };

            auto it = std::find_if(
                jobs.begin(), jobs.end(), [id](const ScheduledJob &job) {
                    return id == job.id;
                });

            if (it == jobs.end())
                return false;

            jobs.erase(it);
        }

        try {
            ConnectionGuard guard;
            pqxx::work txn{ guard.get() };
            pqxx::result res{ txn.exec_params(
                "DELETE FROM scheduled_messages WHERE id = $1", id) };
            txn.commit();
        } catch (const std::exception &e) {
            std::cout << "Failed to delete job from DB: " << e.what() << '\n';
        }

        return true;
    }

    explicit Scheduler(dpp::cluster &b)
        : bot(b)
    {
    }

    ~Scheduler()
    {
        stop();
    }

    // Loads schedules from the database once and starts the background
    // thread
    void start()
    {
        if (running.exchange(true))
            return;

        try {
            ConnectionGuard guard;
            pqxx::work txn{ guard.get() };
            pqxx::result res{ txn.exec(
                "SELECT id, type, channel_id, message_text, next_run_time, "
                "interval_seconds, interval_str "
                "FROM scheduled_messages "
                "ORDER BY next_run_time ASC;") };

            std::lock_guard lock{ mutex_ };
            jobs.clear();
            bool db_updated{ false };

            for (const auto &row : res) {
                ScheduledJob job;
                job.id = row["id"].as<int>();
                job.type = row["type"].as<std::string>();
                job.channel_id = row["channel_id"].as<uint64_t>();
                job.message_text = row["message_text"].as<std::string>();
                job.next_run_time =
                    parse_db_time(row["next_run_time"].as<std::string>());

                long long seconds =
                    row["interval_seconds"].is_null()
                        ? 0
                        : row["interval_seconds"].as<long long>();
                job.interval = std::chrono::seconds(seconds);
                job.interval_str = row["interval_str"].is_null()
                                       ? ""
                                       : row["interval_str"].as<std::string>();

                // Startup catch-up check for missed recurring jobs
                auto now{ std::chrono::system_clock::now() };
                if (job.type == "recurring" && job.next_run_time <= now) {
                    auto new_next_run{ job.next_run_time + job.interval };
                    if (new_next_run <= now) {
                        auto missed_turns =
                            (now - new_next_run) / job.interval + 1;
                        new_next_run += missed_turns * job.interval;
                    }

                    std::cout << "[Scheduler] Catching up missed job #"
                              << job.id << " to next future time: "
                              << format_db_time(new_next_run) << '\n';

                    job.next_run_time = new_next_run;

                    txn.exec_params("UPDATE scheduled_messages SET "
                                    "next_run_time = $1 WHERE id = $2;",
                                    format_db_time(new_next_run),
                                    job.id);

                    db_updated = true;
                }

                jobs.insert(job);
            }

            if (db_updated) {
                txn.commit();
            }

            std::cout << "[Scheduler] Successfully loaded " << jobs.size()
                      << " job(s) from the database.\n";
        } catch (const std::exception &e) {
            std::cerr << "[Scheduler Error] Failed to load jobs from DB on "
                         "start: "
                      << e.what() << '\n';
        }

        worker_thread = std::thread{ &Scheduler::poll_loop, this };
        std::cout << "[Scheduler] Background thread worker started.\n";
    }

    // Stops the background thread worker cleanly
    void stop()
    {
        if (running.exchange(false)) {
            cv_.notify_all();
            if (worker_thread.joinable())
                worker_thread.join();
            std::cout << "[Scheduler] Background thread worker stopped.\n";
        }
    }

    // Schedules oneshot message
    void schedule_once(dpp::snowflake channel_id,
                       const std::string &message_text,
                       std::chrono::system_clock::time_point target_time)
    {
        int new_id{ 0 };
        std::string time_str{ format_db_time(target_time) };

        try {
            // Insert into database and retrieve the generated serial ID
            ConnectionGuard guard;
            pqxx::work txn{ guard.get() };
            pqxx::result res =
                txn.exec_params("INSERT INTO scheduled_messages (type, "
                                "channel_id, message_text, next_run_time) "
                                "VALUES ('once', $1, $2, $3) RETURNING id;",
                                static_cast<uint64_t>(channel_id),
                                message_text,
                                time_str);
            new_id = res[0][0].as<int>();
            txn.commit();
        } catch (const std::exception &e) {
            std::cerr << "[Scheduler Error] Failed to save once job to DB: "
                      << e.what() << '\n';
            throw;
        }

        // Add to in memory cache safely
        {
            std::lock_guard guard{ mutex_ };
            ScheduledJob job;
            job.id = new_id;
            job.type = "once";
            job.channel_id = channel_id;
            job.message_text = message_text;
            job.next_run_time = target_time;
            job.interval = std::chrono::seconds(0);
            job.interval_str = "";

            jobs.insert(job);
        }

        // Notify worker thread
        cv_.notify_one();
        std::cout << "[Scheduler] Scheduled one-off message ID " << new_id
                  << " for " << time_str << '\n';
    }

    // Schedules a recurring message
    void schedule_recurring(dpp::snowflake channel_id,
                            const std::string &message_text,
                            std::chrono::system_clock::time_point target_time,
                            std::chrono::seconds interval,
                            const std::string &interval_str)
    {
        int new_id = 0;
        std::string time_str = format_db_time(target_time);

        // Insert into database and retrieve the generated serial ID
        try {
            ConnectionGuard guard;
            pqxx::work txn{ guard.get() };
            pqxx::result res = txn.exec_params(
                "INSERT INTO scheduled_messages (type, channel_id, "
                "message_text, next_run_time, interval_seconds, "
                "interval_str) "
                "VALUES ('recurring', $1, $2, $3, $4, $5) RETURNING id;",
                static_cast<uint64_t>(channel_id),
                message_text,
                time_str,
                interval.count(),
                interval_str);
            new_id = res[0][0].as<int>();
            txn.commit();
        } catch (const std::exception &e) {
            std::cerr
                << "[Scheduler Error] Failed to save recurring job to DB: "
                << e.what() << '\n';
            throw;
        }

        // Add to the in-memory set cache safely
        {
            std::lock_guard lock(mutex_);
            ScheduledJob job;
            job.id = new_id;
            job.type = "recurring";
            job.channel_id = channel_id;
            job.message_text = message_text;
            job.next_run_time = target_time;
            job.interval = interval;
            job.interval_str = interval_str;

            jobs.insert(job);
        }

        // Notify worker thread
        cv_.notify_one();
        std::cout << "[Scheduler] Scheduled recurring message ID " << new_id
                  << " starting " << time_str << " (every " << interval_str
                  << ")\n";
    }

  private:
    dpp::cluster &bot;
    std::thread worker_thread; // main background worker thread for scheduler
    std::atomic<bool> running{ false };
    std::set<ScheduledJob> jobs;
    mutable std::mutex mutex_;   // makes sure thread doesn't read the vector
                                 // if slash command is adding a new job
    std::condition_variable cv_; // to sleep and wake up the thread

    // Helper to parse PostgreSQL UTC time string to C++ time_point
    std::chrono::system_clock::time_point parse_db_time(const std::string &str)
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

    // Executes a scheduled job (updates database and the cache)
    void execute_job(const ScheduledJob &job)
    {
        bot.message_create({ job.channel_id, job.message_text });

        if (job.type == "once") {
            // 1. Delete from database
            [job] -> dpp::job {
                try {
                    ConnectionGuard guard;
                    pqxx::work txn{ guard.get() };
                    txn.exec_params(
                        "DELETE FROM scheduled_messages WHERE id = $1;",
                        job.id);
                    txn.commit();
                } catch (const std::exception &e) {
                    std::cerr << "[Scheduler Error] Failed to delete job ID "
                              << job.id << "from DB: " << e.what() << '\n';
                }

                std::cout << "[Scheduler] Executed one job ID " << job.id
                          << " to channel " << job.channel_id << '\n';
                co_return;
            }();
        } 
        else if (job.type == "recurring") {
            [job, this] -> dpp::job {
                // Calculate next run time
                auto now{ std::chrono::system_clock::now() };
                auto new_next_run{ job.next_run_time + job.interval };

                // Update in DB
                try {
                    ConnectionGuard guard;
                    pqxx::work txn{ guard.get() };
                    txn.exec_params("UPDATE scheduled_messages SET "
                                    "next_run_time = $1 WHERE id = $2;",
                                    format_db_time(new_next_run),
                                    job.id);
                    txn.commit();
                } catch (const std::exception &e) {
                    std::cerr << "[Scheduler Error] Failed to update recurring "
                                 "job ID "
                              << job.id << "in DB: " << e.what() << '\n';
                }

                // Put back in sorted cache
                {
                    std::lock_guard guard{ mutex_ };
                    ScheduledJob updated_job{ job };
                    updated_job.next_run_time = new_next_run;
                    jobs.insert(updated_job);
                }

                cv_.notify_one();
                co_return;
            }();
        }
    }

    // Thread worker loop
    void poll_loop()
    {
        while (running) {
            std::unique_lock lock{ mutex_ };
            if (jobs.empty()) {
                cv_.wait(lock, [this] { return !running || !jobs.empty(); });
            } else {
                auto now{ std::chrono::system_clock::now() };
                auto soonest_job{ *jobs.begin() };

                if (soonest_job.next_run_time <= now) {
                    jobs.erase(jobs.begin());
                    lock.unlock(); // unlock job queue mutex so that others
                                   // can schedule right away

                    try {
                        execute_job(soonest_job);
                    } catch (const std::exception &e) {
                        std::cerr << "[Scheduler Error] Loop task "
                                     "execution failed: "
                                  << e.what() << '\n';
                    }
                } else {
                    cv_.wait_until(
                        lock, soonest_job.next_run_time, [this, soonest_job] {
                            if (!running)
                                return true;
                            if (jobs.empty())
                                return true;
                            // wake up if soonest job changed to
                            // something sooner
                            return jobs.begin()->next_run_time
                                   < soonest_job.next_run_time;
                        });
                }
            }
        }
    }
};