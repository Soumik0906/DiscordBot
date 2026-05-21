#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <pqxx/pqxx>
#include <queue>
#include <thread>

class ConnectionPool
{
  public:
    // Delete copy and move constructors for singleton pattern
    ConnectionPool(const ConnectionPool &) = delete;
    ConnectionPool &operator=(const ConnectionPool &) = delete;
    ConnectionPool(ConnectionPool &&) = delete;
    ConnectionPool &operator=(ConnectionPool &&) = delete;

    // Get the global thread-safe singleton instance
    static ConnectionPool &get_instance()
    {
        static ConnectionPool instance{ 5 };
        return instance;
    }

    // Leases a connection from the pool
    // Blocks if none are free
    std::shared_ptr<pqxx::connection> acquire()
    {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this]() { return !pool_.empty(); });

        auto conn{ pool_.front() };
        pool_.pop();

        try {
            if (!conn || !conn->is_open()) {
                std::cout
                    << "[Database] Re-establishing lost/inactive connection\n";
                conn = std::make_shared<pqxx::connection>(connection_string_);
            } else {
                pqxx::work w(*conn);
                w.exec("SELECT 1;");
                w.commit();
            }
        } catch (const std::exception &e) {
            std::cerr << "[Database Warning] Connection check/repair failed: "
                      << e.what() << ". Attempting direct rebuild...\n";
            try {
                conn = std::make_shared<pqxx::connection>(connection_string_);
            } catch (const std::exception &err) {
                std::cerr
                    << "[Database Error] Full connection recovery failed: "
                    << err.what() << '\n';
            }
        }

        return conn;
    }

    void release(const std::shared_ptr<pqxx::connection> &conn)
    {
        if (!conn) {
            return;
        }
        {
            std::lock_guard lock(mutex_);
            pool_.push(conn);
        }
        cv_.notify_one();
    }

    ~ConnectionPool()
    {
        if (ping_running_.exchange(false)) {
            cv_ping_.notify_all();
            if (ping_thread_.joinable())
                ping_thread_.join();
            std::cout << "[Database] Background ping thread stopped.\n";
        }
    }

    void ping_loop()
    {
        std::cout << "[Database] Background ping thread started.\n";
        while (ping_running_) {
            std::unique_lock lock{ ping_mutex_ };
            cv_ping_.wait_for(lock, std::chrono::seconds(120), [this] {
                return !ping_running_;
            });
            if (!ping_running_) {
                break;
            }
            lock.unlock();

            ping_all();
        }
    }

    void ping_all()
    {
        std::lock_guard lock{ mutex_ };
        size_t pool_size{ pool_.size() };

        std::cout << "[Database] Pinging " << pool_size << " idle connection(s) in the pool..." << '\n';

        for (size_t i{ 0 }; i < pool_size; ++i) {
            auto conn{ pool_.front() };
            pool_.pop();
            bool ok{ false };
            
            try {
                if (conn && conn->is_open()) {
                    pqxx::work w{ *conn };
                    w.exec("SELECT 1;");
                    w.commit();
                    ok = true;
                }
            } catch (...) {}

            if (!ok) {
                try {
                    conn = std::make_shared<pqxx::connection>(connection_string_);
                    std::cout << "[Database] Successfully rebuilt a lost connection in the background." << '\n';
                } catch (const std::exception& e) {
                    std::cerr << "[Database Error] Background connection rebuild failed: " << e.what() << '\n';
                }
            }

            pool_.push(conn);
        }
    }


  private:
    explicit ConnectionPool(size_t pool_size)
    {
        const char *db_url{ std::getenv("DATABASE_URL") };
        if (!db_url) {
            std::cerr << "[Database Error] DATABASE_URL environment variable "
                         "is not set!\n";
            connection_string_ = "";
            return;
        }

        connection_string_ = db_url;
        if (connection_string_.find("sslmode") == std::string::npos) {
            if (connection_string_.find("?") == std::string::npos) {
                connection_string_ += "?sslmode=require";
            } else {
                connection_string_ += "&sslmode=require";
            }
        }

        if (connection_string_.find("keepalives") == std::string::npos) {
            if (connection_string_.find("?") == std::string::npos)
                connection_string_ +=
                    "?keepalives=1&keepalives_idle=60&keepalives_interval=10&"
                    "keepalives_count=3";
            else
                connection_string_ +=
                    "&keepalives=1&keepalives_idle=60&keepalives_interval=10&"
                    "keepalives_count=3";
        }

        for (size_t i{ 0 }; i < pool_size; ++i) {
            try {
                auto conn{ std::make_shared<pqxx::connection>(
                    connection_string_) };
                if (conn->is_open()) {
                    pool_.push(conn);
                } else {
                    std::cerr << "[Database Error] Connection " << i
                              << " failed to open.\n";
                }
            } catch (const std::exception &err) {
                std::cerr << "[Database Error] Failed to initialize connection "
                          << i << err.what() << '\n';
            }
        }

        std::cout << "[Database] Successfully initialized " << pool_.size()
                  << " active connections.\n";

        ping_running_ = true;
        ping_thread_ = std::thread(&ConnectionPool::ping_loop, this);
    }

    std::string connection_string_;
    std::queue<std::shared_ptr<pqxx::connection>> pool_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic_bool ping_running_{ false };
    std::mutex ping_mutex_;
    std::condition_variable cv_ping_;
    std::thread ping_thread_;
};

// RAII connection guard to lease a connection and automatically return it to
// the pool when out of scope
class ConnectionGuard
{
  public:
    ConnectionGuard()
        : conn_{ ConnectionPool::get_instance().acquire() }
    {
        if (!conn_ || !conn_->is_open()) {
            throw std::runtime_error(
                "Could not obtain a healthy database connection from the pool");
        }
    }

    ~ConnectionGuard()
    {
        ConnectionPool::get_instance().release(conn_);
    }

    pqxx::connection &get() const
    {
        return *conn_;
    }

    // Delete copy constructors to avoid double-leasing/releasing
    ConnectionGuard(const ConnectionGuard &) = delete;
    ConnectionGuard &operator=(const ConnectionGuard &) = delete;

  private:
    std::shared_ptr<pqxx::connection> conn_;
};