#pragma once
#include <dpp/dpp.h>
#include <memory>
#include <string>
#include <unordered_map>
#include "commands/command.h"

class CommandHandler
{
    std::unordered_map<std::string, std::shared_ptr<Command>> commands;

  public:
    CommandHandler() = default;

    // Register a command class in the handler
    template <typename T, typename... Args>
    void register_command(Args &&...args)
    {
        auto cmd = std::make_shared<T>(std::forward<Args>(args)...);
        commands[cmd->get_name()] = cmd;
    }

    // Register all registered commands globally with Discord
    void register_with_discord(dpp::cluster &bot)
    {
        if (dpp::run_once<struct register_bot_commands>()) {
            for (const auto &[name, cmd] : commands) {
                dpp::slashcommand slash_cmd(
                    cmd->get_name(), cmd->get_description(), bot.me.id);
                for (const auto &opt : cmd->get_options()) {
                    slash_cmd.add_option(opt);
                }
                bot.global_command_create(slash_cmd);
            }
        }
    }

    // Dispatch the command when the slash command event triggers
    void handle_slash_command(dpp::cluster &bot,
                              const dpp::slashcommand_t &event)
    {
        std::string name = event.command.get_command_name();
        auto it = commands.find(name);
        if (it != commands.end()) {
            it->second->run(bot, event);
        } else {
            event.reply("Unknown command!");
        }
    }
};
