#pragma once
#include <dpp/appcommand.h>
#include <vector>
#include "command.h"

class ScheduleCommand : public Command
{
  public:
    std::string get_name() const override
    {
        return "schedule";
    }

    std::string get_description() const override
    {
        return "Schedule a message";
    }

    std::vector<dpp::command_option> get_options() const override
    {
        return {
            dpp::command_option(dpp::co_sub_command, "once", "Schedule a message once")
                .add_option(dpp::command_option(dpp::co_channel, "channel", "Destination channel", true))
                .add_option(dpp::command_option(dpp::co_string, "date", "DD/MM/YYYY (or) today (or) tomorrow", true))
                .add_option(dpp::command_option(dpp::co_string, "time", "HH:MM", true)),
            dpp::command_option(dpp::co_sub_command, "recurring", "Schedule a recurring message")
                .add_option(dpp::command_option(dpp::co_string, "interval", "Interval (e.g. daily, weekly)", true))
                .add_option(dpp::command_option(dpp::co_channel, "channel", "Destination channel", true))
                .add_option(dpp::command_option(dpp::co_string, "date", "DD/MM/YYYY (or) today (or) tomorrow", true))
                .add_option(dpp::command_option(dpp::co_string, "time", "HH:MM", true)),
        };
    }

    void run(dpp::cluster &bot, const dpp::slashcommand_t &event) override
    {
        auto subcommand = event.command.get_command_interaction().options[0];

        if (subcommand.name == "once") {
            event.reply("Scheduling message once...");
        }
        else if (subcommand.name == "recurring") {
            event.reply("Scheduling recurring message...");
        }
    }
};