#include "command_handler.h"
#include "commands/ping.h"
#include "commands/schedule.h"
#include "scheduler.h"
#include "database.h"

#include <cstdlib>
#include <dpp/dpp.h>
#include <iostream>
#include <string>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

void run_dummy_webserver()
{
    const char *port_env{ std::getenv("PORT") };
    int port{ port_env ? std::stoi(port_env) : 10000 };

    int server_fd{ socket(AF_INET, SOCK_STREAM, 0) };
    if (server_fd < 0) {
        std::cerr << "Failed to create socket\n";
        return;
    }

    int opt{ 1 };
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Binds to 0.0.0.0 (all interfaces)
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        std::cerr << "Failed to bind to port " << port << "\n";
        close(server_fd);
        return;
    }

    if (listen(server_fd, 10) < 0) {
        std::cerr << "Failed to listen on port " << port << "\n";
        close(server_fd);
        return;
    }

    std::cout << "Dummy web server listening on port " << port << "\n";

    while (true) {
        int client_fd{ accept(server_fd, nullptr, nullptr) };
        if (client_fd >= 0) {
            char buffer[1024]{ 0 };
            int bytes_received{ static_cast<int>(read(client_fd, buffer, sizeof(buffer) - 1)) };
            if (bytes_received > 0) {
                std::string response{
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: 2\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "OK"
                };
                ssize_t bytes_written{ write(client_fd, response.c_str(), response.length()) };
                if (bytes_written < 0) {
                    std::cerr << "Failed to write response to client\n";
                }
            }
            close(client_fd);
        }
    }
    close(server_fd);
}

void connect_database()
{
    try
    {
        ConnectionGuard guard;

        pqxx::work transaction{ guard.get() };
        pqxx::result res{ transaction.exec("SELECT version()") };

        std::cout << "[Database Startup Check] Connected to Aiven Database: " << guard.get().dbname() << '\n';
        std::cout << "[Database Startup Check] Postgres version: " << res[0][0].as<std::string>() << '\n';

        transaction.commit();
    }
    catch (std::exception& e)
    {
        std::cerr << "[Database Startup Check Warning] Failed to connect/verify DB on startup: " << e.what() << '\n';
    }
}

int main()
{
    tzset();

    const char* tzone{ std::getenv("TZ") };
    if (tzone)
        std::cout << "Time zone: " << tzone << '\n';

    const char *token{ std::getenv("BOT_TOKEN") };

    if (token)
        std::cout << "Bot token: " << token << '\n';
    else {
        std::cout << "Bot token not found\n";
        exit(1);
    }

    // connect db
    connect_database();

    // Start dummy webserver on a detached background thread
    std::thread webserver_thread(run_dummy_webserver);
    webserver_thread.detach();

    dpp::cluster bot(token);
    CommandHandler handler{};
    Scheduler scheduler{bot};

    handler.register_command<PingCommand>();
    handler.register_command<ScheduleCommand>(scheduler);

    bot.on_log(dpp::utility::cout_logger());

    bot.on_ready([&bot, &handler](const dpp::ready_t &event) {
        std::cout << "Logged in as bot " << bot.me.username << '\n';
        handler.register_with_discord(bot);
    });

    bot.on_slashcommand([&bot, &handler](const dpp::slashcommand_t &event) {
        handler.handle_slash_command(bot, event);
    });

    // Start the background scheduler thread
    scheduler.start();

    bot.start(dpp::st_wait);

    return 0;
}