#pragma once
#include <dpp/message.h>
#include "../games/game_manager.h"
#include "../games/rps/rps.h"
#include "command.h"

class RpsCommand : public Command
{
  public:
    std::string get_name() const override
    {
        return "rps";
    }

    std::string get_description() const override
    {
        return "Challenge someone to Rock Paper Scissors";
    }

    std::vector<dpp::command_option> get_options() const override
    {
        return { dpp::command_option(
            dpp::co_user, "opponent", "The user you want to challenge", true) };
    }

    dpp::task<void> run(dpp::cluster &bot, const dpp::slashcommand_t &event) override
    {
        dpp::snowflake challenger = event.command.usr.id;
        dpp::snowflake opponent =
            std::get<dpp::snowflake>(event.get_parameter("opponent"));
        dpp::snowflake channel_id = event.command.channel_id;

        if (challenger == opponent) {
            co_await event.co_reply(dpp::message("You cannot challenge yourself!")
                            .set_flags(dpp::m_ephemeral));
            co_return;
        }

        auto game = std::make_shared<RockPaperScissors>(
            channel_id, challenger, opponent);

        bool started = GameManager::get_instance().start_game(channel_id, game);

        if (!started) {
            co_await event.co_reply(
                dpp::message("A game is already active in this channel!")
                    .set_flags(dpp::m_ephemeral));
            co_return;
        }

        // Send the initial public game screen
        co_await event.co_reply(game->get_game_screen());
        co_return;
    }
};
