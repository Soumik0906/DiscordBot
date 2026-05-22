#pragma once
#include <dpp/dispatcher.h>
#include <dpp/dpp.h>
#include <string>
#include "../game_base.h"

class RockPaperScissors : public GameBase
{
  private:
    int p1_choice = 0; // 0 = None, 1 = Rock, 2 = Paper, 3 = Scissors
    int p2_choice = 0;

    std::string get_choice_emoji(int choice) const;
    std::string get_choice_name(int choice) const;
    int determine_winner() const; // 1 (P1), 2 (P2)

  public:
    RockPaperScissors(dpp::snowflake channel_id,
                      dpp::snowflake challenger,
                      dpp::snowflake opponent)
        : GameBase("rps", channel_id, { challenger, opponent })
    {
    }

    std::string get_name() const override
    {
        return "Rock Paper Scissors";
    }

    dpp::message get_game_screen() override;

    void handle_interaction(const dpp::button_click_t &event) override;
};