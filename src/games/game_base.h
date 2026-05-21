#pragma once
#include <dpp/dispatcher.h>
#include <dpp/dpp.h>
#include <dpp/snowflake.h>
#include <string>
#include <vector>

class GameBase
{
  protected:
    std::string game_type{};             // e.g., tictactoe, connect4
    dpp::snowflake channel_id;           // channel where game is being played
    std::vector<dpp::snowflake> players; // user id's of participants
    size_t current_turn_idx = 0;         // index pointing to player vector
    bool is_finished = false;

  public:
    GameBase(const std::string &type,
             dpp::snowflake ch_id,
             const std::vector<dpp::snowflake> &player_ids)
        : game_type(type)
        , channel_id(ch_id)
        , players(player_ids)
    {
    }

    virtual ~GameBase() = default;

    virtual std::string get_name() const = 0;

    virtual dpp::message get_game_screen() = 0;

    virtual void handle_interaction(const dpp::button_click_t &event) = 0;

    bool has_player(dpp::snowflake user_id) const
    {
        for (auto id : players)
            if (id == user_id)
                return true;
        return false;
    }

    bool is_player_turn(dpp::snowflake user_id) const
    {
        if (players.empty() || is_finished)
            return false;
        return players[current_turn_idx] == user_id;
    }

    bool is_over() const
    {
        return is_finished;
    }
};