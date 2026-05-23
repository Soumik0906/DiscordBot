#include "rps.h"
#include <dpp/colors.h>
#include <dpp/dispatcher.h>
#include <dpp/message.h>
#include <dpp/snowflake.h>

std::string RockPaperScissors::get_choice_emoji(int choice) const
{
    if (choice == 1)
        return "🪨";
    if (choice == 2)
        return "📄";
    if (choice == 3)
        return "✂️";
    return "❔";
}

std::string RockPaperScissors::get_choice_name(int choice) const
{
    if (choice == 1)
        return "Rock";
    if (choice == 2)
        return "Paper";
    if (choice == 3)
        return "Scissors";
    return "None";
}

int RockPaperScissors::determine_winner() const
{
    if (p1_choice == p2_choice)
        return 0; // tie

    if ((p1_choice == 1 && p2_choice == 3) || (p1_choice == 3 && p2_choice == 2)
        || (p1_choice == 2 && p2_choice == 1)) {
        return 1;
    }

    return 2;
}

dpp::message RockPaperScissors::get_game_screen()
{
    dpp::message msg;
    msg.set_channel_id(channel_id);

    std::string content = "🎮 **Rock Paper Scissors** 🎮\n";
    content += "Challenger: <@" + std::to_string(players[0]) + ">\n";
    content += "Opponent: <@" + std::to_string(players[1]) + ">\n\n";

    if (!is_finished) {
        content += "Select your move!\n\n";
        content += "**Status:**\n";
        content += "- Challenger: "
                   + std::string(p1_choice != 0 ? "✅ Ready" : "⏳ Thinking...")
                   + "\n";
        content += "- Opponent: "
                   + std::string(p2_choice != 0 ? "✅ Ready" : "⏳ Thinking...")
                   + "\n";

        // create buttons
        dpp::component action_row;
        action_row.add_component(dpp::component()
                                     .set_label("Rock 🪨")
                                     .set_type(dpp::cot_button)
                                     .set_style(dpp::cos_primary)
                                     .set_id("rps_rock"));

        action_row.add_component(dpp::component()
                                     .set_label("Paper 📄")
                                     .set_type(dpp::cot_button)
                                     .set_style(dpp::cos_success)
                                     .set_id("rps_paper"));

        action_row.add_component(dpp::component()
                                     .set_label("Scissors ✂️")
                                     .set_type(dpp::cot_button)
                                     .set_style(dpp::cos_danger)
                                     .set_id("rps_scissors"));

        msg.add_component(action_row);

    } else {
        int winner = determine_winner();
        content += "🏆 **Game Over** 🏆\n\n";

        content += "<@" + std::to_string(players[0]) + "> chose "
                   + get_choice_emoji(p1_choice) + "("
                   + get_choice_name(p1_choice) + ")\n";

        content += "<@" + std::to_string(players[1]) + "> chose "
                   + get_choice_emoji(p2_choice) + "("
                   + get_choice_name(p2_choice) + ")\n\n";

        if (winner == 0) {
            content += "🤝🏻 **It's a Tie!**";
        } else {
            content +=
                "🎉 <@" + std::to_string(players[winner - 1]) + "> **Wins!**";
        }
    }

    msg.set_content(content);
    return msg;
}

dpp::task<void> RockPaperScissors::handle_interaction(const dpp::button_click_t &event)
{
    dpp::snowflake user_id = event.command.usr.id;
    bool is_p1 = (user_id == players[0]);

    if ((is_p1 && p1_choice != 0) || (!is_p1 && p2_choice != 0)) {
        co_await event.co_reply(dpp::message("You have already made your move!")
                        .set_flags(dpp::m_ephemeral));
        co_return;
    }

    int choice = 0;
    if (event.custom_id == "rps_rock")
        choice = 1;
    if (event.custom_id == "rps_paper")
        choice = 2;
    if (event.custom_id == "rps_scissors")
        choice = 3;

    if (choice == 0)
        co_return;

    if (is_p1)
        p1_choice = choice;
    else
        p2_choice = choice;

    if (p1_choice != 0 && p2_choice != 0)
        is_finished = true;

    co_await event.co_reply(dpp::message("You chose " + get_choice_name(choice) + " "
                             + get_choice_emoji(choice) + "!")
                    .set_flags(dpp::m_ephemeral));

    dpp::cluster *bot = event.from()->creator;
    dpp::message public_board = get_game_screen();
    public_board.id = event.command.message_id;
    public_board.channel_id = channel_id;

    co_await bot->co_message_edit(public_board);
    co_return;
}