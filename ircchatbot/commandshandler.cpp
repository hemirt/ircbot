#include "commandshandler.hpp"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>
#include <iostream>
#include <vector>
#include <string_view>
#include <sstream>

#include "echopage.hpp"

constexpr std::string_view commands_db_name = "commands.db";

CommandsHandler::CommandsHandler()
    : commands_db(commands_db_name)
{
    init_db();
}

void CommandsHandler::init_db()
{
    auto result = commands_db.execute_statement("CREATE TABLE IF NOT EXISTS commands (trigger TEXT NOT NULL PRIMARY KEY, response TEXT NOT NULL, channels TEXT, users TEXT);");
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
            auto cmd = create_command(line);
            if (cmd)
            {
                commands.emplace(*line[0], std::move(*cmd));
            }
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

void load_channels(const std::string& channels_string, CommandDetail& cmd)
{
    if (channels_string.empty())
    {
        return;
    }
    
    switch (channels_string.front())
    {
        case '-':
            cmd.c_include = false;
            break;
        case '+':
        default:
            cmd.c_include = true;
            break;
    }
    
    boost::algorithm::split(cmd.channels, channels_string.substr(1), boost::algorithm::is_any_of(","), boost::algorithm::token_compress_on);
}

void load_users(const std::string& users_string, CommandDetail& cmd)
{
    if (users_string.empty())
    {
        return;
    }
    
    switch (users_string.front())
    {
        case '-':
            cmd.u_include = false;
            break;
        case '+':
        default:
            cmd.u_include = true;
            break;
    }
    
    boost::algorithm::split(cmd.userids, users_string.substr(1), boost::algorithm::is_any_of(","), boost::algorithm::token_compress_on);
}

constexpr std::size_t COMMAND_DATA_SIZE = 4; // number of db columns

std::optional<CommandDetail> CommandsHandler::create_command(std::vector<std::optional<std::string>> command_data)
{
    if (command_data.size() != COMMAND_DATA_SIZE)
    {
        return std::nullopt;
    }
    
    CommandDetail cmd;
    cmd.trigger = *command_data[0];
    cmd.response = *command_data[1];
    
    auto&& channels = command_data[2];
    if (channels)
    {
        load_channels(*channels, cmd);
    }
    auto&& users = command_data[3];
    if (users)
    {
        load_users(*users, cmd);
    }
    
    return cmd;
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

    CommandDetail cmd;
    cmd.trigger = trigger;
    cmd.response = response;
    
    commands.emplace(trigger, cmd);

    return result.rc == SQLITE_OK;
}

bool CommandsHandler::remove_textcommand(std::string_view trigger)
{

    auto result = commands_db.execute_prepared_statement("DELETE FROM commands WHERE trigger=?;", { std::string(trigger) });

    if (auto it = commands.find(trigger); it != commands.end())
    {
        commands.erase(it);
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

    if (auto it = commands.find(trigger); it != commands.end())
    {   
        auto&& cmd = it->second;
        
        if (!cmd.channels.empty())
        {
            if (cmd.channels.contains(ircmessage.channel) != cmd.c_include)
            {
                return std::nullopt;
            }
        }
        
        if (!cmd.userids.empty())
        {
            if (cmd.userids.contains(ircmessage.user_id) != cmd.u_include)
            {
                return std::nullopt;
            }
        }
        
        auto response_string = cmd.response;
        
        std::vector<std::string_view> tokens;
        boost::algorithm::split(tokens, ircmessage.message, boost::algorithm::is_any_of(" "), boost::algorithm::token_compress_on);
                
        {
            boost::regex e("\\${\\d+}");
            
            std::vector<int> vec_num_params{0};  // vector of ${number} numbers
            boost::match_results<std::string::const_iterator> results;
            if (boost::regex_search(response_string, results, e)) {
                for (const auto& result : results) {
                    vec_num_params.push_back(boost::lexical_cast<int>(std::string_view(result.begin() + 2, result.end() - 1)));
                }
            }
            
            int maximal_number = *std::max_element(vec_num_params.begin(), vec_num_params.end());
                        
            if (maximal_number > tokens.size())
            {
                return std::nullopt;
            }
            else
            {
                std::stringstream ss;
                
                for (int i = 0; i <= maximal_number; ++i)
                {
                    ss.str(std::string());
                    ss.clear();
                    ss << "${" << i << "}";
                    
                    boost::algorithm::replace_all(response_string, ss.str(), tokens[i]);
                }
            }
        }
        
        {
            auto& Echo = Hemirt::Utility::EchoPage::get_instance();
            
            boost::regex e("\\$url{([^}]+)}");
            boost::match_results<std::string::const_iterator> results;
            
            while (boost::regex_search(response_string, results, e)) {
                std::string page = results[1];
                std::string replace = Echo.echo_page(page);
                response_string = boost::regex_replace(response_string, e, replace, boost::match_default | boost::format_first_only);
            }
        }
        
        return response_string;
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

std::string CommandDetail::channels_to_string() const
{
    std::string ret = c_include ? "+" : "-";
    for (auto&& i : channels)
    {
        ret += i + ",";
    }
    return ret;
}

std::string CommandDetail::userids_to_string() const
{
    std::string ret = u_include ? "+" : "-";
    for (auto&& i : userids)
    {
        ret += i + ",";
    }
    return ret;
}

bool CommandsHandler::add_channel_to_command(std::string_view trigger, std::string_view channel_sv)
{
    auto it = commands.find(trigger);
    if (it == commands.end())
    {
        return false;
    }
    auto&& cmd = it->second;
    
    std::string channel = std::string(channel_sv);
    boost::algorithm::to_lower(channel);
    cmd.channels.insert(channel);
    
    auto str = cmd.channels_to_string();
    auto result = commands_db.execute_prepared_statement("UPDATE commands SET channels = ? WHERE trigger = ?;", { std::move(str), std::string(trigger) });

    return result.rc == SQLITE_OK;
}

bool CommandsHandler::add_userid_to_command(std::string_view trigger, std::string_view userid)
{
    auto it = commands.find(trigger);
    if (it == commands.end())
    {
        return false;
    }
    auto&& cmd = it->second;
    
    cmd.userids.insert(std::string(userid));
    
    auto str = cmd.userids_to_string();
    auto result = commands_db.execute_prepared_statement("UPDATE commands SET users = ? WHERE trigger = ?;", { std::move(str), std::string(trigger) });

    return result.rc == SQLITE_OK;
}

int CommandsHandler::toggle_channels_to_command(std::string_view trigger)
{
    auto it = commands.find(trigger);
    if (it == commands.end())
    {
        return -1;
    }
    auto&& cmd = it->second;
    
    cmd.c_include = !cmd.c_include;
    auto str = cmd.channels_to_string();
    auto result = commands_db.execute_prepared_statement("UPDATE commands SET channels = ? WHERE trigger = ?;", { std::move(str), std::string(trigger) });
    
    if (result.rc != SQLITE_OK)
    {
        cmd.c_include = !cmd.c_include;
        return -2;
    }
    
    return cmd.c_include;
}

int CommandsHandler::toggle_userids_to_command(std::string_view trigger)
{
    auto it = commands.find(trigger);
    if (it == commands.end())
    {
        return -1;
    }
    auto&& cmd = it->second;
    
    cmd.u_include = !cmd.u_include;
    auto str = cmd.userids_to_string();
    auto result = commands_db.execute_prepared_statement("UPDATE commands SET users = ? WHERE trigger = ?;", { std::move(str), std::string(trigger) });
    
    if (result.rc != SQLITE_OK)
    {
        cmd.u_include = !cmd.u_include;
        return -2;
    }
    
    return cmd.u_include;
}