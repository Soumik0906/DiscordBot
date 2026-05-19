#pragma once
#include <dpp/dpp.h>
#include <string>
#include <vector>

class Command
{
  public:
    virtual ~Command() = default;

    // The lowercase name of the slash command
    virtual std::string get_name() const = 0;

    // The description of the slash command
    virtual std::string get_description() const = 0;

    // Optional parameters/options for the command (default is empty)
    virtual std::vector<dpp::command_option> get_options() const
    {
        return {};
    }

    // The execution logic for the command
    virtual void run(dpp::cluster &bot, const dpp::slashcommand_t &event) = 0;
};
