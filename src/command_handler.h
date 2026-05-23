#pragma once
#include <dpp/dpp.h>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include "commands/command.h"

class CommandHandler
{
    std::unordered_map<std::string, std::shared_ptr<Command>> commands;

    // Helper to get all currently cached guild IDs
    std::vector<dpp::snowflake> get_cached_guild_ids() const
    {
        std::vector<dpp::snowflake> guild_ids;
        if (auto *cache = dpp::get_guild_cache()) {
            std::shared_lock lock(cache->get_mutex());
            for (const auto &[guild_id, guild] : cache->get_container()) {
                guild_ids.push_back(guild_id);
            }
        }
        return guild_ids;
    }

    // Helper to build dpp::slashcommand objects from registered code commands
    std::vector<dpp::slashcommand>
    build_slash_commands(const dpp::cluster &bot) const
    {
        std::vector<dpp::slashcommand> slash_commands;
        for (const auto &[name, cmd] : commands) {
            dpp::slashcommand slash_cmd(
                cmd->get_name(), cmd->get_description(), bot.me.id);
            for (const auto &opt : cmd->get_options()) {
                slash_cmd.add_option(opt);
            }
            slash_commands.push_back(slash_cmd);
        }
        return slash_commands;
    }

    // Helper to delete all commands in the provided guilds
    void
    delete_guild_commands(dpp::cluster &bot,
                          const std::vector<dpp::snowflake> &guild_ids) const
    {
        for (dpp::snowflake guild_id : guild_ids) {
            bot.guild_bulk_command_delete(
                guild_id,
                [&bot, guild_id](const dpp::confirmation_callback_t &event) {
                    if (event.is_error()) {
                        bot.log(dpp::ll_error,
                                "Failed to delete commands for guild "
                                    + std::to_string(guild_id) + ": "
                                    + event.get_error().message);
                    }
                });
        }
    }

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
            bot.log(dpp::ll_info,
                    "Waiting 5 seconds for the guild cache to populate before "
                    "command cleanup...");

            // Start a 5-second one-shot timer to allow guilds to load, then run
            // the cleanup and registration
            static dpp::oneshot_timer timer(
                &bot, 5, [&bot, this](dpp::timer timer_handle) {
                    bot.log(
                        dpp::ll_info,
                        "Starting command registration cleanup and setup...");

                    auto guild_ids = get_cached_guild_ids();

                    // 1. Delete all global commands first
                    bot.global_bulk_command_delete(
                        [&bot, this, guild_ids](
                            const dpp::confirmation_callback_t &event) {
                            if (event.is_error()) {
                                bot.log(dpp::ll_error,
                                        "Failed to delete global commands: "
                                            + event.get_error().message);
                            }

                            // 2. Delete all guild-specific commands
                            delete_guild_commands(bot, guild_ids);

                            // 3. Register current commands from the code
                            // globally
                            auto slash_commands = build_slash_commands(bot);
                            bot.global_bulk_command_create(
                                slash_commands,
                                [&bot](const dpp::confirmation_callback_t
                                           &create_event) {
                                    if (create_event.is_error()) {
                                        bot.log(dpp::ll_error,
                                                "Failed to register global "
                                                "commands from code: "
                                                    + create_event.get_error()
                                                          .message);
                                    } else {
                                        bot.log(dpp::ll_info,
                                                "Successfully registered "
                                                "global commands from code.");
                                    }
                                });
                        });
                });
        }
    }

    // Dispatch the command when the slash command event triggers
    dpp::task<void> handle_slash_command(dpp::cluster &bot,
                              const dpp::slashcommand_t &event)
    {
        std::string name = event.command.get_command_name();
        auto it = commands.find(name);
        if (it != commands.end()) {
            co_await it->second->run(bot, event);
        } else {
            co_await event.co_reply("Unknown command!");
        }
        co_return;
    }
};
