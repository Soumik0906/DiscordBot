#pragma once
#include <iomanip>
#include <sstream>
#include <string>
#include "command.h"

class PingCommand : public Command
{
  public:
    std::string get_name() const override
    {
        return "ping";
    }

    std::string get_description() const override
    {
        return "Ping the bot";
    }

    void run(dpp::cluster &bot, const dpp::slashcommand_t &event) override
    {
        dpp::discord_client *shard{ event.from() };
        double ws_ping{ shard ? shard->websocket_ping * 1000.0 : -1.0 };

        double rtt_ms{ (dpp::utility::time_f()
                        - event.command.get_creation_time())
                       * 1000.0 };

        std::ostringstream msg;
        msg << "Pong!\n";
        msg << "- RTT: " << std::fixed << std::setprecision(2) << rtt_ms
            << " ms\n";
        msg << "- WS:  " << (ws_ping < 0 ? "N/A" : std::to_string(ws_ping))
            << " ms";

        event.reply(msg.str());
    }
};
