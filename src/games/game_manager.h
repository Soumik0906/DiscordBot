#pragma once

#include "game_base.h"

#include <dpp/dispatcher.h>
#include <dpp/dpp.h>
#include <dpp/message.h>
#include <dpp/snowflake.h>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

class GameManager
{
  private:
    std::unordered_map<dpp::snowflake, std::shared_ptr<GameBase>>
        active_games; // [channel_id, game]

    mutable std::shared_mutex games_mutex;

    GameManager() = default;
    ~GameManager() = default;

  public:
    GameManager(const GameManager &) = delete;
    GameManager &operator=(const GameManager &) = delete;
    GameManager(const GameManager &&) = delete;
    GameManager &operator=(const GameManager &&) = delete;

    static GameManager &get_instance()
    {
        static GameManager instance;
        return instance;
    }

    bool start_game(dpp::snowflake channel_id, std::shared_ptr<GameBase> game)
    {
        std::unique_lock lock{ games_mutex };
        if (active_games.find(channel_id) != active_games.end())
            return false;
        active_games[channel_id] = game;
        return true;
    }

    std::shared_ptr<GameBase> get_game(dpp::snowflake channel_id)
    {
        std::shared_lock lock{ games_mutex };
        auto it = active_games.find(channel_id);
        return (it != active_games.end() ? it->second : nullptr);
    }

    void end_game(dpp::snowflake channel_id)
    {
        std::unique_lock lock{ games_mutex };
        active_games.erase(channel_id);
    }

    dpp::task<void> handle_button_click(const dpp::button_click_t &event)
    {
        dpp::snowflake channel_id = event.command.channel_id;
        auto game = get_game(channel_id);
        if (!game) co_return;

        if (!game->has_player(event.command.usr.id)) {
            co_await event.co_reply(dpp::message("You are not a participant in this game!")
                            .set_flags(dpp::m_ephemeral));
            co_return;
        }

        co_await game->handle_interaction(event);

        if (game->is_over()) {
            end_game(channel_id);
        }
        co_return;
    }
};
