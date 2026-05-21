#pragma once

#include "../scheduler.h"
#include "command.h"

#include <chrono>
#include <ctime>
#include <dpp/appcommand.h>
#include <dpp/message.h>
#include <iomanip>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>


class Scheduler;

inline std::chrono::system_clock::time_point
parse_datetime(const std::string &date_str, const std::string &time_str)
{
    std::tm tm = {};

    // today / tomorrow
    if (date_str == "today" || date_str == "tomorrow") {
        auto now = std::chrono::system_clock::now();

        std::time_t t = std::chrono::system_clock::to_time_t(now);

        tm = *std::localtime(&t);

        if (date_str == "tomorrow")
            tm.tm_mday += 1;
    } else {
        std::istringstream ds(date_str);

        ds >> std::get_time(&tm, "%d/%m/%Y");

        if (ds.fail())
            throw std::runtime_error("Invalid date format");
    }

    // parse HH:MM:SS
    std::tm time_tm = {};

    std::istringstream ts(time_str);

    ts >> std::get_time(&time_tm, "%H:%M:%S");

    if (ts.fail())
        throw std::runtime_error("Invalid time format");

    tm.tm_hour = time_tm.tm_hour;
    tm.tm_min = time_tm.tm_min;
    tm.tm_sec = time_tm.tm_sec;
    tm.tm_isdst = -1;

    std::time_t final_time = std::mktime(&tm);

    return std::chrono::system_clock::from_time_t(final_time);
}

inline std::chrono::seconds parse_interval(const std::string &interval)
{
    std::regex re(R"((\d+)\s*([dhms]))");

    std::sregex_iterator begin(interval.begin(), interval.end(), re);

    std::sregex_iterator end;

    long long total_seconds = 0;

    for (auto it = begin; it != end; ++it) {
        int value = std::stoi((*it)[1].str());

        char unit = (*it)[2].str()[0];

        switch (unit) {
            case 'd':
                total_seconds += value * 86400;
                break;

            case 'h':
                total_seconds += value * 3600;
                break;

            case 'm':
                total_seconds += value * 60;
                break;

            case 's':
                total_seconds += value;
                break;

            default:
                throw std::runtime_error("Invalid interval unit");
        }
    }

    if (total_seconds == 0)
        throw std::runtime_error("Invalid interval");

    return std::chrono::seconds(total_seconds);
}

class ScheduleCommand : public Command
{
    Scheduler &scheduler;

  public:
    explicit ScheduleCommand(Scheduler &s)
        : scheduler(s)
    {
    }

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
            dpp::command_option(
                dpp::co_sub_command, "once", "Schedule a message once")
                .add_option(dpp::command_option(
                    dpp::co_string, "message", "Message to be scheduled", true))
                .add_option(dpp::command_option(
                    dpp::co_channel, "channel", "Destination channel", true))
                .add_option(
                    dpp::command_option(dpp::co_string,
                                        "date",
                                        "DD/MM/YYYY (or) today (or) tomorrow",
                                        true))
                .add_option(dpp::command_option(
                    dpp::co_string, "time", "HH:MM:SS", true)),

            dpp::command_option(dpp::co_sub_command,
                                "recurring",
                                "Schedule a recurring message")
                .add_option(dpp::command_option(dpp::co_string,
                                                "interval",
                                                "Interval (1d, 1h 30m, 5m 5s)",
                                                true))
                .add_option(dpp::command_option(
                    dpp::co_string, "message", "Message to be scheduled", true))
                .add_option(dpp::command_option(
                    dpp::co_channel, "channel", "Destination channel", true))
                .add_option(
                    dpp::command_option(dpp::co_string,
                                        "date",
                                        "DD/MM/YYYY (or) today (or) tomorrow",
                                        false))
                .add_option(dpp::command_option(
                    dpp::co_string, "time", "HH:MM:SS", false)),

            dpp::command_option(dpp::co_sub_command, "list", "List schedules"),

            dpp::command_option(
                dpp::co_sub_command, "remove", "Delete a schedule")
                .add_option(
                    dpp::command_option(dpp::co_integer,
                                        "id",
                                        "ID of the schedule (check list)",
                                        true)),

        };
    }

    void run(dpp::cluster &bot, const dpp::slashcommand_t &event) override
    {
        auto subcommand{ event.command.get_command_interaction().options[0] };

        auto get_string{ [&](const std::string &name) -> std::string {
            for (const auto &opt : subcommand.options) {
                if (opt.name == name)
                    return std::get<std::string>(opt.value);
            }
            return "";
        } };

        std::string message_text{ get_string("message") };

        dpp::snowflake channel_id{ 0 };
        for (const auto &opt : subcommand.options) {
            if (opt.name == "channel") {
                channel_id = std::get<dpp::snowflake>(opt.value);
                break;
            }
        }

        try {
            // Schedule once
            if (subcommand.name == "once") {
                std::string date_str{ get_string("date") };
                std::string time_str{ get_string("time") };

                auto target_time{ parse_datetime(date_str, time_str) };
                auto now{ std::chrono::system_clock::now() };

                auto delay{ std::chrono::duration_cast<std::chrono::seconds>(
                    target_time - now) };

                if (delay.count() <= 0) {
                    event.reply(
                        dpp::message("Cannot schedule a message in the past!")
                            .set_flags(dpp::m_ephemeral));
                    return;
                }

                scheduler.schedule_once(channel_id, message_text, target_time);
                event.reply(dpp::message("Message scheduled successfully!")
                                .set_flags(dpp::m_ephemeral));

            }
            // Schedule recurring
            else if (subcommand.name == "recurring") {
                std::string interval_str{ get_string("interval") };
                auto interval{ parse_interval(interval_str) };

                std::string date_str{ get_string("date") };
                std::string time_str{ get_string("time") };

                if (date_str.empty() && !time_str.empty()) {
                    date_str = "today";
                }

                // if date and time are provided, wait until that time before
                // starting the loop
                if (!date_str.empty() && !time_str.empty()) {
                    auto target_time{ parse_datetime(date_str, time_str) };
                    auto now{ std::chrono::system_clock::now() };
                    auto delay{
                        std::chrono::duration_cast<std::chrono::seconds>(
                            target_time - now)
                    };

                    if (delay.count() <= 0) {
                        event.reply(
                            dpp::message(
                                "Cannot schedule a message in the past!")
                                .set_flags(dpp::m_ephemeral));
                        return;
                    }

                    scheduler.schedule_recurring(channel_id,
                                                 message_text,
                                                 target_time,
                                                 interval,
                                                 interval_str);
                } else {
                    auto target_time{ std::chrono::system_clock::now()
                                      + interval };
                    scheduler.schedule_recurring(channel_id,
                                                 message_text,
                                                 target_time,
                                                 interval,
                                                 interval_str);
                }

                event.reply(dpp::message("Recurring message set!")
                                .set_flags(dpp::m_ephemeral));

            }
            // Schedule list
            else if (subcommand.name == "list") {
                dpp::embed embed;
                embed.set_title("Scheduled Messages");
                for (const auto &job : scheduler.job_list()) {
                    auto tt{ std::chrono::system_clock::to_time_t(
                        job.next_run_time) };

                    std::ostringstream time_ss;
                    time_ss << std::put_time(std::localtime(&tt),
                                             "%Y-%m-%d %H:%M:%S");

                    embed.add_field("Id # " + std::to_string(job.id),
                                    job.message_text + "\n⏰ " + time_ss.str());
                }

                event.reply(embed);
            }
            // Schedule remove
            else if (subcommand.name == "remove") {
                auto id = std::get<int64_t>(event.get_parameter("id"));

                if (scheduler.remove_job(id)) {
                    event.reply(dpp::message("Message #" + std::to_string(id)
                                             + " was deleted successfully!")
                                    .set_flags(dpp::m_ephemeral));

                } else {
                    event.reply(dpp::message("No scheduled message with ID #"
                                             + std::to_string(id) + " exists.")
                                    .set_flags(dpp::m_ephemeral));
                }
            }

        } catch (std::exception &e) {
            event.reply(
                dpp::message(std::string{ "Failed to schedule message! " }
                             + e.what())
                    .set_flags(dpp::m_ephemeral));
        }
    }
};