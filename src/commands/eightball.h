#pragma once

#include "command.h"

#include <dpp/appcommand.h>
#include <string>
#include <vector>
#include <random>

class EightBallCommand : public Command
{
  private:
    std::vector<std::string> responses
    {
        "✅ It is certain",
        "💯 It is decidedly so",
        "🔥 Without a doubt",
        "✅ Yes definitely",
        "🛡️ You may rely on it",
        "👀 As I see it, yes",
        "📈 Most likely",
        "🌤️ Outlook good",
        "👍 Yes",
        "✨ Signs point to yes",
        "🌫️ Reply hazy, try again",
        "⏳ Ask again later",
        "🤐 Better not tell you now",
        "🔮 Cannot predict now",
        "🧠 Concentrate and ask again",
        "❌ Don't count on it",
        "🚫 My reply is no",
        "📉 My sources say no",
        "🌧️ Outlook not so good",
        "💀 Very doubtful"
    };

  public:
    std::string get_name() const override {
        return "8ball";
    }

    std::string get_description() const override {
        return "Ask 8ball a question";
    }

    std::vector<dpp::command_option> get_options() const override 
    {
        return {
            dpp::command_option(dpp::co_string, "question",
                "What do you want to ask?", true)
        };
    }

    dpp::task<void> run(dpp::cluster& bot, const dpp::slashcommand_t& event) override 
    {
        static thread_local std::mt19937 gen{ std::random_device{}() };

        std::uniform_int_distribution<size_t> dist{ 0, responses.size() - 1 };

        std::string ques{ std::get<std::string>(
            event.get_parameter("question")) 
        };

        auto response{ responses.at(dist(gen)) };

        dpp::embed embed;
        embed.set_color(dpp::colors::purple)
        .set_title("🎱 The Magic 8-Ball Says...")
        .add_field("❓ Question\n", ques)
        .add_field("🔮 Answer\n", response);

        co_await event.co_reply(embed);
        co_return;
    }
};