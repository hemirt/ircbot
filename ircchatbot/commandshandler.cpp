#include "commandshandler.hpp"

#include <iostream>

constexpr std::string_view commands_db_name = "commands.db";

CommandsHandler::CommandsHandler()
    : commands_db(commands_db_name)
{
    init_db();
}

void CommandsHandler::init_db()
{
    auto result = commands_db.execute_statement("CREATE TABLE IF NOT EXISTS commands (trigger TEXT NOT NULL PRIMARY KEY, response TEXT NOT NULL);");
    if (result.rc != SQLITE_OK)
    {
        std::string msg = "Commands DB table commands error: " + result.errmsg;
        std::cerr << msg << std::endl;
        throw std::runtime_error(msg);
    }

    {
        result = commands_db.execute_statement("SELECT * FROM commands;");
        if (result.rc != SQLITE_OK)
        {
            std::string msg = "Commands DB table commands select all error: " + result.errmsg;
            std::cerr << msg << std::endl;
            throw std::runtime_error(msg);
        }

        for (auto&& line : result.data)
        {
            textcommands.emplace(*line[0], *line[1]);
        }
    }

    result = commands_db.execute_statement("CREATE TABLE IF NOT EXISTS banphrases (phrase TEXT NOT NULL PRIMARY KEY, timeout INT NOT NULL);");
    if (result.rc != SQLITE_OK)
    {
        std::string msg = "Commands DB table banphrases error: " + result.errmsg;
        std::cerr << msg << std::endl;
        throw std::runtime_error(msg);
    }

    {
        result = commands_db.execute_statement("SELECT * FROM banphrases;");
        if (result.rc != SQLITE_OK)
        {
            std::string msg = "Commands DB table banphrases select all error: " + result.errmsg;
            std::cerr << msg << std::endl;
            throw std::runtime_error(msg);
        }

        for (auto&& line : result.data)
        {
            boost::regex r_phrase(*line[0], boost::regex_constants::no_except);
            if (r_phrase.status() != 0)
            {
                continue;
            }
            int timeout = std::stoi(*line[1]);
            banphrases.emplace(r_phrase, timeout);
        }
    }
}

bool CommandsHandler::add_banphrase(std::string_view phrase, int timeout)
{
    boost::regex r_phrase(phrase.begin(), phrase.end(), boost::regex_constants::no_except);
    if (r_phrase.status() != 0)
    {
        return false;
    }

    auto result = commands_db.execute_prepared_statement("INSERT INTO banphrases (phrase, timeout) VALUES (?, ?);", { std::string(phrase), timeout });

    banphrases.emplace(r_phrase, timeout);

    return result.rc == SQLITE_OK;
}

bool CommandsHandler::remove_banphrase(std::string_view phrase)
{
    boost::regex r_phrase(phrase.begin(), phrase.end(), boost::regex_constants::no_except);
    if (r_phrase.status() != 0)
    {
        return false;
    }

    auto result = commands_db.execute_prepared_statement("DELETE FROM banphrases WHERE phrase=?;", { std::string(phrase) });

    banphrases.erase(r_phrase);

    return result.rc == SQLITE_OK;
}

bool CommandsHandler::add_textcommand(std::string_view trigger, std::string_view response)
{
    auto result = commands_db.execute_prepared_statement("INSERT INTO commands (trigger, response) VALUES (?, ?);", { std::string(trigger), std::string(response) });

    textcommands.emplace(trigger, response);

    return result.rc == SQLITE_OK;
}

bool CommandsHandler::remove_textcommand(std::string_view trigger)
{

    auto result = commands_db.execute_prepared_statement("DELETE FROM commands WHERE trigger=?;", { std::string(trigger) });

    if (auto it = textcommands.find(trigger); it != textcommands.end())
    {
        textcommands.erase(it);
    }

    return result.rc == SQLITE_OK;
}

std::optional<std::string> CommandsHandler::handle_privmsg(const IrcMessage& ircmessage)
{
    if (ircmessage.type != IrcMessage::Type::PRIVMSG)
    {
        return std::nullopt;
    }
    
    std::string_view trigger;
    auto space_pos = ircmessage.message.find(' ');
    if (space_pos == std::string_view::npos)
    {
        trigger = ircmessage.message;
    }
    else
    {
        trigger = ircmessage.message.substr(0, space_pos);
    }

    if (auto it = textcommands.find(trigger); it != textcommands.end())
    {
        return it->second;
    }

    return std::nullopt;
}

int CommandsHandler::is_banphrased(std::string_view line)
{
    boost::regex_iterator<std::string_view::const_iterator> end;
    int total_timeout = 0;

    for (auto&& [phrase, timeout] : banphrases)
    {
        boost::regex_iterator<std::string_view::const_iterator> start(line.begin(), line.end(), phrase);
        auto count = std::distance(start, end);
        if (count > 0)
        {
            if (timeout == -1)
            {
                return -1;
            }
            else
            {
                total_timeout += timeout * count;
            }
        }
    }

    return total_timeout;
}