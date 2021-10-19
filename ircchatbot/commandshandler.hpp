#ifndef COMMANDS_HANDLER_HPP
#define COMMANDS_HANDLER_HPP

#include "database.hpp"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <boost/algorithm/string/regex.hpp>
#include <set>

#include "ircmessage.hpp"

using ChannelName = std::string;
using UserId = std::string;
using Trigger = std::string;

struct CommandDetail
{
    Trigger trigger;
    std::string response;
    std::set<ChannelName, std::less<>> channels;
    bool c_include = true;
    std::string channels_to_string() const;
    std::set<UserId, std::less<>> userids;
    bool u_include = true;
    std::string userids_to_string() const;
};

class CommandsHandler
{
public:
    CommandsHandler();

    bool add_banphrase(std::string_view phrase, int timeout);
    bool remove_banphrase(std::string_view phrase);

    bool add_textcommand(std::string_view trigger, std::string_view response);
    bool remove_textcommand(std::string_view trigger);

    /* handle PRIVMSG IrcMessages */
    std::optional<std::string> handle_privmsg(const IrcMessage& ircmessage);
    int is_banphrased(std::string_view line);
    std::string show_cmd(std::string_view trigger);

    std::optional<CommandDetail> create_command(std::vector<std::optional<std::string>> command_data);

    bool add_channel_to_command(std::string_view trigger, std::string_view channel);
    bool add_userid_to_command(std::string_view trigger, std::string_view userid);

    int toggle_userids_to_command(std::string_view trigger);
    int toggle_channels_to_command(std::string_view trigger);

private:
    Database commands_db;
    std::map<boost::regex, int> banphrases;
    std::map<Trigger, CommandDetail, std::less<>> commands;

    void init_db();
};

#endif // COMMANDS_HANDLER_HPP
