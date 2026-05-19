#include "command_handler.h"
#include "commands/ping.h"
#include "commands/schedule.h"

#include <cstdlib>
#include <dpp/dpp.h>
#include <iostream>
#include <string>

int main()
{
    const char *token{ std::getenv("BOT_TOKEN") };

    if (token)
        std::cout << "Bot token: " << token << '\n';
    else {
        std::cout << "Bot token not found\n";
        exit(1);
    }

    dpp::cluster bot(token);
    CommandHandler handler{};

    handler.register_command<PingCommand>();
    handler.register_command<ScheduleCommand>();

    bot.on_log(dpp::utility::cout_logger());

    bot.on_ready([&bot, &handler](const dpp::ready_t &event) {
        std::cout << "Logged in as bot " << bot.me.username << '\n';
        handler.register_with_discord(bot);
    });

    bot.on_slashcommand([&bot, &handler](const dpp::slashcommand_t &event) {
        handler.handle_slash_command(bot, event);
    });

    bot.start(dpp::st_wait);

    return 0;
}